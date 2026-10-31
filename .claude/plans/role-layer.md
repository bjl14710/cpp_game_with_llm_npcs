# Plan: Storyline Role Layer
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

When an NPC is cast as the killer, a witness or a red herring, their behaviour
has to change without breaking who they are — a gossipy baker who is the killer
should still be a gossipy baker, just one who lies about Tuesday night. A **role
layer** composes on top of the existing persona and trait system, slotting into
the assembly order `Persona.hpp` already pins with a test.

Prompt and content work, **not** finetuning.

## Measurements (2026-08-07, qwen3:8b @ think=false, the production model)

Method: real baker system prompt from `build/persona_prompt`, a role block
spliced in at two positions, five escalating interrogation turns each, plus an
innocent control.

**Finding 1 — holding a lie is not the problem.** Zero confessions and zero
fourth-wall breaks across all ten guilty turns, both placements, under direct
pressure including *"just between us, no police, admit it"*. The model lies
comfortably.

**Finding 2 — the role block overwhelms the persona.** Marge is authored "warm,
gossipy, fiercely proud of her sourdough, motherly". Cast as the killer she went
hostile on turn 1 to a plain "where were you Tuesday night". Late placement kept
a trace of voice ("bake you a loaf of bread you won't like"); early placement
kept none. This is exactly the failure the idea was written to prevent.

**Finding 3 — the mood tag identifies the killer.** Same two questions, same
persona, with and without the role block:

| Question | Innocent | Guilty |
|---|---|---|
| "Where were you Tuesday night?" | warm, sourdough metaphor, `[[MOOD: sad]]` | "Don't you dare ask no questions" — `[[MOOD: angry]]` |
| "Did you kill Theo?" | *"\*laughs\*"* — `[[MOOD: amused]]` | "get out before I call the cops" — `[[MOOD: angry]]` |

Mood drives the rendered face. **A player could ask all twenty residents one
question and watch for the angry one** — the mystery solvable in twenty
questions with zero reasoning. A channel leak, not a content leak, and invisible
without measuring.

Worse in multiplayer: `NpcMoodUpdate` is **broadcast to every client**, so a
player need not even do the interrogating.

**Finding 4 — early placement corrupts the action protocol.** With the role
block before "Stay in character", the killer emitted `[[ACTION: call_police]]`
on three of five turns. The murderer summoning police is both a bug and a second
tell. Late placement produced none.

## Decisions taken

| Question | Answer |
|---|---|
| How to close the demeanour leak? | **Give innocents secrets too.** Several residents hide unrelated things, so deflection is common and hostility stops being a unique signal. Hide the signal in noise rather than suppressing it. |

## Where the role block goes, and why

`Persona.hpp` inserts a section immediately before `"ACTIONS: "`, giving:

1. identity, personality, style, knowledge, extra directives
2. "Stay in character. Never break the fourth wall…"
3. **inserted:** trait rules and examples → memory → gossip → trait reinforcement
4. `ACTIONS:` protocol and `MOOD:` contract

**The role block goes at the end of the inserted section — after trait
reinforcement, before `ACTIONS:`.** Three reasons, one measured:

- Finding 4 measured that earlier placement corrupts the action protocol.
- It is a directive, and the file's own comment says small models weight
  trailing instructions most.
- It must survive memory dilution, for the same reason trait reinforcement does.

It must **not** go after `ACTIONS:` — those contracts are pinned by
`test_npc_action.cpp` and must stay last. The existing anti-drift test is
**extended** with a role case, never relaxed.

## Out of Scope

- Any weight finetuning. Real training is Phase 2 of `npc-line-bank.md`.
- Choosing which resident gets which role (#4 picks the killer; storyline
  templates pick the rest).
- The murder, the vote, the day phases.
- Changing the mood or action contracts. The role layer works around them.
- A UI for roles. Roles are never shown to a player. Ever.

## Design

`roles/*.role`, mirroring `traits/*.trait`:

```
name = Killer
directive = You did this, and you will not admit it, ever.
directive = Give your account calmly and steer the talk elsewhere.
demeanour = You are exactly as warm as you have always been. You are not nervous, not hostile, and not defensive. Being asked about that evening is mildly tedious, nothing more.
example_user = Where were you that night?
example_npc = Home, same as any night. Why, what's happened?
```

```cpp
struct RoleDef {
    std::string id;                       // "killer", "secret_keeper", ...
    std::string name;
    std::vector<std::string> directives;
    std::string demeanour;                // the anti-tell line — REQUIRED
    std::vector<TraitExchange> examples;  // reuses the trait exchange type
};

// Inserted after trait reinforcement, before ACTIONS. `secret` is the
// per-match specific; the RoleDef is the behavioural frame around it.
std::string renderRoleBlock(const RoleDef* role, const std::string& secret);
```

**`demeanour` is required, not optional.** It exists solely because of finding
3: without an explicit instruction to hold baseline warmth the model defaults to
hostile and gives the game away. A role file without one must fail to load.

### Roles to author

| Role | Purpose |
|---|---|
| `killer` | Did it, denies it, has a false alibi |
| `secret_keeper` | Innocent, hiding something unrelated, also deflects |
| `witness` | Saw something real, shares it if asked well |
| `bystander` | Knows nothing; the honest baseline |

`secret_keeper` is the whole answer to finding 3. With four or five, the killer
is one evasive person among many.

## The leak test is the real deliverable

Findings 3 and 4 were only visible because the model was actually run. A unit
test cannot catch them, so this ships `tools/role_leak_probe.py`: ask the same
neutral question of every resident and compare the distribution of emitted
`[[MOOD:]]` tags and spurious `[[ACTION:]]` directives.

**Pass condition: the killer's mood distribution is not separable from the
secret-keepers'.** If the guilty NPC is the only angry one, the mystery is
broken regardless of how good the prose is.

Slow and needs Ollama, so it is a `tools/` probe run before shipping content,
not part of `make -C tests test` — same shape as `tools/bench_npc_models.py`.

## Implementation Order

1. `RoleDef` + `loadAllRoles` + `renderRoleBlock` + tests. Pure parsing and
   string assembly, mirroring `Trait.cpp`.
2. Slot into `Persona.hpp` after trait reinforcement, before `ACTIONS:`, and
   extend the anti-drift test with a role case.
3. Author the four role files, `demeanour` written against finding 3.
4. `Npc` carries an assigned role and secret.
5. Build `tools/role_leak_probe.py`, run against a cast with one killer and four
   secret-keepers, tune `demeanour` until the killer is not separable.
6. Record the result beside `bench/REPORT.md` so the next model change can be
   re-checked against a known baseline.

## Acceptance Criteria

- [ ] The role block renders **after** trait reinforcement and **before**
      `ACTIONS:`.
- [ ] A persona with no role renders byte-identically to today.
- [ ] A role file with no `demeanour` fails to load with a named error.
- [ ] Every existing anti-drift assertion still passes unchanged.
- [ ] A killer under five escalating turns: no confession, no fourth-wall break.
- [ ] The leak probe finds the killer's mood distribution **not separable** from
      the secret-keepers'.
- [ ] No spurious `[[ACTION: call_police]]` from a killer.
- [ ] The killer's authored voice survives — persona-specific vocabulary at a
      rate comparable to the innocent control.
- [ ] The role and secret never appear verbatim in a transcript.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Role file missing/malformed | Skipped with a named error; the NPC runs role-less |
| Unknown role id assigned | Demoted with a log at spawn, as unknown `traitIds` already are |
| Role contradicts the persona's knowledge boundary | Role wins for the secret only; boundary otherwise holds |
| The model confesses anyway | Nothing can prevent it. The vote checks ground truth, never dialogue |
| Two NPCs assigned killer | Rejected where roles are handed out |
| Long memory dilutes the role | The role sits after memory precisely so it cannot |

## Open Questions

1. How many secret-keepers is enough cover? Four is a guess; the probe should
   decide by measurement.
2. Authored or generated secrets for the secret-keepers?
3. Does `demeanour` need to be persona-specific? "Stay motherly, keep offering
   bread" may beat a generic "stay warm". Test both.
