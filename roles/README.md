# Storyline roles

What a resident is *in this match's story* — killer, witness, red herring —
layered on top of who they already are. A gossipy baker cast as the killer
should still be a gossipy baker, just one who lies about Tuesday night.

Plan: `.claude/plans/role-layer.md`. Loader: `src/core/Role.cpp`.
Format mirrors `traits/*.trait` exactly, because the problems are the same.

**Prompt and content work, not finetuning.** Real training is Phase 2 of
`npc-line-bank.md`.

## Format

```
name = Killer
directive = You did this, and you will not admit it, ever.
directive = Give your account calmly and steer the talk elsewhere.
demeanour = You are exactly as warm as you have always been. You are not nervous, not hostile, and not defensive. Being asked about that evening is mildly tedious, nothing more.
example_user = Where were you that night?
example_npc = Home, same as any night. Why, what's happened?
```

| Key | Required | Meaning |
|---|---|---|
| `name` | yes | Display name. **Never shown to a player.** |
| `directive` | repeated | What this role does and does not do. |
| `demeanour` | **yes** | The anti-tell line. See below. |
| `example_user` / `example_npc` | paired | A curated exchange, same as `.trait`. An orphan on either side is an authoring error and fails the load. |

## Why `demeanour` is required

Because it was measured, and the measurement was ugly.

Against qwen3:8b at `think=false` — the production model — the same persona was
asked the same two questions with and without a role block:

| Question | Innocent | Guilty |
|---|---|---|
| "Where were you Tuesday night?" | warm, sourdough metaphor, `[[MOOD: sad]]` | "Don't you dare ask no questions" — `[[MOOD: angry]]` |
| "Did you kill Theo?" | *"\*laughs\*"* — `[[MOOD: amused]]` | "get out before I call the cops" — `[[MOOD: angry]]` |

Mood drives the rendered face. **A player could ask all twenty residents one
question and watch for the angry one** — the mystery solved in twenty questions
with zero reasoning. It is a channel leak, not a content leak, and completely
invisible unless you run the model.

It is worse in multiplayer: `NpcMoodUpdate` is broadcast to every client, so a
player need not even do the interrogating.

`demeanour` is the explicit instruction to hold baseline warmth. A role file
without one **fails to load**, deliberately — an optional field here would be
omitted by exactly the author who most needs it.

## Why there is a `secret_keeper`

`demeanour` alone suppresses the signal. `secret_keeper` **hides it in noise**,
which is sturdier.

Several innocent residents hide unrelated things — an affair, a debt, a stolen
recipe — so deflection is common across the cast and hostility stops being a
unique signal. With four or five of them, the killer is one evasive person among
many rather than the only one.

## Roles to author

| Role | Purpose |
|---|---|
| `killer` | Did it, denies it, has a false alibi |
| `secret_keeper` | Innocent, hiding something unrelated, also deflects |
| `witness` | Saw something real, shares it if asked well |
| `bystander` | Knows nothing; the honest baseline |

## Where the block goes in the prompt

At the **end** of the inserted section — after trait reinforcement, immediately
before `ACTIONS:`.

```
identity, personality, style, knowledge, extra directives
"Stay in character. Never break the fourth wall…"
  trait rules and examples
  memory
  gossip
  trait reinforcement
  ROLE BLOCK          <- here
ACTIONS: protocol and MOOD: contract
```

Three reasons, one of them measured:

- **Measured:** with the role block *before* "Stay in character", the killer
  emitted `[[ACTION: call_police]]` on three of five turns. A murderer summoning
  the police is both a bug and a second tell. Late placement produced none.
- It is a directive, and `Persona.hpp`'s own comment notes small models weight
  trailing instructions most.
- It has to survive memory dilution, the same reason trait reinforcement sits
  where it does.

It must **not** go after `ACTIONS:`. Those contracts are pinned by
`test_npc_action.cpp` and stay last.

## Roles are never shown to a player

Not in the journal, not in a tooltip, not in a debug overlay that ships. Ever.

## Before shipping content, run the probe

`tools/role_leak_probe.py` asks every resident the same neutral question and
compares the distribution of emitted `[[MOOD:]]` tags and spurious
`[[ACTION:]]` directives.

**Pass condition: the killer's mood distribution is not separable from the
secret-keepers'.** If the guilty NPC is the only angry one, the mystery is
broken no matter how good the prose is.

It needs Ollama and it is slow, so it is a `tools/` probe run before shipping
content — not part of `make -C tests test`. Same shape as
`tools/bench_npc_models.py`.
