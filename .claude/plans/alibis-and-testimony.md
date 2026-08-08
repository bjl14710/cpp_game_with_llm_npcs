# Plan: Alibis and Testimony on the Existing Fact System
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

The mystery's testimony layer is mostly already built and nobody has spent it.
`Gossip.cpp` + `FactStore.cpp` + `Journal.hpp` implement PROPOSE → VALIDATE →
COMMIT fact propagation with **per-NPC** knowledge, stable content-hashed ids,
SQLite persistence, and proximity spread.

Most importantly, **`Journal.hpp` already flags contradictions**: two facts
sharing a subject with different content are marked conflicting on both sides,
time-ordered, with attribution. That is the detective mechanic, complete and
unused.

## Goal

A player can interrogate three residents about the same evening, get three
different accounts, and see the journal flag two of them as contradicting each
other — without the game ever saying which is false.

## Decisions taken

| Question | Answer |
|---|---|
| How is a lie modelled? | **No lie flag.** A lie is an ordinary fact: same subject, different content. |
| Does seeded testimony gossip? | **Yes, but on a slower clock than player-introduced facts.** |

## The finding that reshaped this plan

The original ask included *"a way to mark a fact as a lie."* **Don't build it.**
The substrate gives lies for free, and a flag would make the game worse:

- `factIdFor(subject, content)` hashes **both**, so "Ray was at the bakery" and
  "Ray was at the library" are two distinct facts on subject `ray_whereabouts`.
  They coexist; `addFact` is first-teller-wins per *id*, so neither evicts the
  other.
- `journalEntries` already groups by subject, orders by time, and sets
  `conflicting = true` on **both** sides when one subject carries more than one
  content. Its own comment: *"two different stories about the same thing."*

So a lie needs **zero new fields**. It needs a liar.

The host still knows which account is false — that lives in the host-only
`MysterySetup`, never in `WorldState`. This keeps the leak rule intact:
`WorldState` is the thing designed to be gossiped, replicated and written to
SQLite, so the answer must never be in it.

## The pacing problem, and the fix

A match day compresses ~11 in-world hours into ~8 real minutes. At that rate
`kGossipMinAgeSeconds` (30 game minutes) elapses in about **20 real seconds**,
and with 20 NPCs in a compact downtown at `kGossipChance = 0.35` per pair per
tick, **knowledge would homogenise within day 1** — flattening the per-NPC
distinctness the entire mechanic depends on.

Fix: seeded testimony spreads on its own, slower schedule. Two candidates:

1. **Rate by provenance.** Facts whose `source` is a persona name (seeded
   testimony) use a longer min-age and lower chance than facts sourced from
   `"player"`. **No new field** — `source` already distinguishes them.
2. A seeded-fact age offset.

Prefer (1): no new state, and it reads as a rule about *kinds of knowledge*,
which is what it is. Second-hand testimony is also a free red-herring generator
— "Ray told me he saw…" is genuinely weaker evidence than seeing it, and the
journal's attribution already carries that distinction.

## Out of Scope

- Making the killer *tell* the lie. This seeds the false alibi as a fact the
  killer holds; motivating them to volunteer it is the role layer.
- Any `Persona.hpp` or prompt change.
- Generating the mystery (consumes `MysterySetup`).
- The vote.
- A new journal UI — `journalEntries` and the Journal menu page already render
  conflicts.
- Interrogation mechanics. Players ask in natural language as now.
- Changing `validateProposedFacts` or the PROPOSE → VALIDATE → COMMIT boundary.
  LLM output still never writes directly.

## Design

```cpp
// Turns host-only ground truth into per-NPC knowledge on the bus.
// Commits nothing that identifies the killer AS the killer.
void seedTestimony(WorldState& state, const MysterySetup& setup);
```

| Fact | Subject | Known by |
|---|---|---|
| The death | `<victim>_death` | Everyone, including the player |
| Each witness's observation | `<subject>_whereabouts` | That witness only |
| The killer's false alibi | `<killer>_whereabouts` | The killer only |
| Physical evidence | `<evidence_id>` | Nobody initially — granted on discovery |

The killer's alibi and any true observation of the killer share the subject
`<killer>_whereabouts` with different content. **That is the contradiction, and
it is structural rather than flagged.**

**Evidence is knowledge nobody starts with.** Committing it with no knowers means
it exists on the bus but surfaces to nobody until a discovery grants it — exactly
what `grantKnowledge` is for.

## Implementation Order

1. Subject naming + `seedTestimony` + tests. Pure, against a hand-built setup.
   The no-leak assertion lands here.
2. **Verify contradiction detection end-to-end.** Seed a truth and a competing
   alibi, grant both to the player, assert `journalEntries` returns both flagged
   `conflicting`. **This test is the whole point of the plan** — if it passes,
   the detective mechanic works.
3. Provenance-based gossip rate. Measure how long full homogenisation takes with
   20 agents and pick constants so it exceeds one match day.
4. Wire into match start.

## Acceptance Criteria

- [ ] Each witness `knows` only their own observation.
- [ ] Every survivor and the player `knows` the death fact.
- [ ] A true observation and the false alibi coexist with the **same subject and
      different content**; neither evicted the other.
- [ ] Granted both, `journalEntries` returns both with `conflicting == true` on
      **both** sides.
- [ ] **No fact content names the killer as the killer**, and no fact carries any
      marker distinguishing the lie from the truth.
- [ ] Evidence facts are known by nobody until `grantKnowledge`.
- [ ] After one match day with 20 agents milling, seeded facts have **not** fully
      homogenised — at least one NPC still lacks at least one seeded fact.
- [ ] Player-introduced facts propagate at today's unchanged rate.
- [ ] Existing `test_gossip.cpp` and `test_journal.cpp` stay green.

**Resist editing `Journal.hpp`.** Its purity as a read-only view is why this
works.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Two witnesses saw the same thing | Same content hashes to one `factId`; both granted knowledge of one fact. Corroboration is exactly what should emerge |
| The killer's alibi accidentally matches the truth | Generation must reject it — an alibi identical to the truth is not a lie |
| A witness is the victim | Dropped at seed time; the dead give no testimony |
| Gossip carries an observation to the killer | Allowed and desirable — the killer learning they were seen is good drama and leaks nothing |
| The player tells an NPC a false fact | Already supported; commits as an ordinary competing account. The player can muddy the water, which is fair |
| `seedTestimony` twice | Idempotent |
| `FactStore` failed to open | Standard degrade: works in memory, nothing persists |

## Open Questions

1. The right slow-gossip constants — deliberately unanswered; step 3 measures
   homogenisation time and picks from data.
2. Should second-hand testimony be attributable in *content*? `source` records
   who first said it, but `propagateGossip` flips a knowledge bit rather than
   rewriting content, so chain length is not represented.
3. Does the Journal page need a mystery filter? With 20 NPCs it could get long.
4. **Is each networked player a distinct knowledge agent?** `journalEntries`
   reads facts known by `"player"` — singular. This plan should settle it; it
   affects far more than one screen.
