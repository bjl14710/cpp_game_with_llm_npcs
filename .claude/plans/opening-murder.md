# Plan: The Opening Murder — Mystery Setup and Ground Truth
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

At match start one resident is already dead. This generates that: pick a victim
and a killer, place the body, decide where the killer was, who saw something,
and what evidence exists — all as a **real data structure the vote can be
checked against, not prose**.

Two things make it smaller than it sounds. **Dead NPCs already render as
corpses** (`NpcState::Dead` collapses the figure, rotates −90°, plays the rig's
death clip, suppresses the mood emote — the renderer literally comments "The
dead don't emote"). And `KnownFact` already has the right shape for seeded
knowledge. The output is one `MysterySetup` that is authoritative, reproducible
from a seed, and **never leaves the host**.

## Goal

A match can start with a randomly chosen victim dead in a named place, every
survivor aware of it, and the game holding a checkable answer to "who did it"
that no player or client can read.

## Decisions taken

| Question | Answer |
|---|---|
| What do players see at the start? | **Arrive to a town that already knows.** The body is in place and every survivor carries the death fact. No cutscene dependency, so this is playable the day it lands. (Superseded in part 2026-08-08: match start now opens with the cutscene — but the *state* is unchanged, the town still already knows.) |
| Does the killer know it is the killer? | **No — ground truth only, no prompt work.** Concealment is the role layer's job. |

## The accepted v1 limitation, stated plainly

With no concealment layer, **you can walk up to the killer and ask, and they
will not deny it.** That is deliberate. The deliverable is a mystery the vote can
be *checked* against; making it *unsolvable by asking* is the role layer, and
making it *solvable by reasoning* is the storyline templates.

## Key facts

- `NpcState::Dead` exists and renders. No corpse work needed.
- `KnownFact` = `{factId, subject, content (≤140 chars), source, learnedAtSeconds}`,
  `factId` a stable hash of subject+content. `Gossip.hpp` supplies
  `normalizeSubject` and `factIdFor`.
- `WorldState` already tracks per-agent knowledge: `grantKnowledge`, `knows`,
  `factsKnownBy`.
- **A deterministic-PRNG precedent exists.** `CharacterParts.cpp` uses a
  hand-rolled xorshift32: *"so randomizeLook(seed) is stable across platforms —
  std::mt19937 would work too, but this keeps the contract obvious and
  header-free."* It also documents `Rng rng{seed ? seed : 1u}` because xorshift
  cannot start at 0.
- `World::rng_` is an `mt19937` used for combat accuracy rolls. **Do not reuse
  it** — sharing would make mystery generation depend on how many shots were
  fired, destroying reproducibility.

## Why determinism is a requirement

1. **Tests.** "Given seed 12345, the victim is X" is the only way to assert on
   generation without mocking randomness everywhere.
2. **Multiplayer.** The host generates and clients never receive it; a
   reproducible seed means the host can be replayed or audited without shipping
   the answer over the wire.

## Out of Scope

- Any system-prompt text. No role layer, no concealment, no lying.
- Solvability. Nothing guarantees the evidence chains to a unique answer.
- The vote. This exposes `voteIsCorrect`; ballots and retaliation are separate.
- Cutscenes.
- Networking. No serialization of the setup, ever.
- Killing the victim through combat. The victim *starts* dead; `updateCombat` is
  untouched.
- Police reaction.

## Design

```cpp
struct Evidence {
    std::string id, zoneId, description;
    bool pointsAtKiller = false;   // true clue, or deliberate red herring
};

struct Witness {
    std::string agent, sawZoneId, observed;
    double atHour = 0.0;
};

// The authoritative answer. Host-only. Never serialized, never rendered.
struct MysterySetup {
    std::string victim, killer, sceneZoneId;
    Vec3 bodyPosition{};
    double murderHour = 0.0;
    std::vector<Evidence> evidence;
    std::vector<Witness> witnesses;
};

MysterySetup generateMystery(const std::vector<Persona>& roster, unsigned seed);
bool voteIsCorrect(const MysterySetup& setup, const std::string& accused);
void seedMysteryFacts(WorldState& state, const MysterySetup& setup);
```

## The leak rule — three defences

1. **It never enters `WorldState`.** That is the thing designed to be shared and
   gossiped. Only its *consequences* go in.
2. **It is never serialized.** `WorldSnapshot` carries players and NPCs; adding
   mystery fields would ship the answer to every client.
3. **`killer` is read through `voteIsCorrect` only** — one place to audit, and a
   stray `setup.killer` in rendering or dialogue code stands out in review.

A test asserts that after `seedMysteryFacts`, no committed fact names the killer
in connection with the murder.

## Implementation Order

1. `Mystery` types + `generateMystery` + tests. Pure. Determinism and
   victim ≠ killer are the whole of this step.
2. `seedMysteryFacts` + tests, including the no-leak assertion.
3. **Start an NPC dead** via a direct state entry that bypasses `takeDamage` —
   that path emits `NpcDamagedEvent` and can flip armed neighbours Hostile. The
   opening murder must not start a firefight.
4. Wire into match start.
5. `voteIsCorrect` plus a **compile-gated** debug reveal. It must be off in any
   build that can host a networked match, or host advantage goes from
   theoretical to one keypress.

## Acceptance Criteria

- [ ] Same roster and seed → identical setup, across runs and platforms.
- [ ] Seed 0 still produces a valid setup (the xorshift gotcha).
- [ ] `victim != killer`; both are roster names.
- [ ] 100 seeds produce more than one distinct victim.
- [ ] `bodyPosition` is walkable and inside `sceneZoneId`.
- [ ] Every survivor and the player `knows` the death fact.
- [ ] A non-witness does NOT know a witness's observation.
- [ ] **No committed fact names the killer as the killer.**
- [ ] The victim is `NpcState::Dead` and **no `NpcDamagedEvent` was emitted**.
- [ ] `voteIsCorrect(setup, setup.killer)` is true; false for every other name.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Roster < 2 | Fail loudly at match start rather than victim == killer |
| The victim is the police persona | Allowed, and interesting |
| Body position collides | Bounded retries, then the zone centre. Never unreachable |
| The killer dies later | Setup unchanged; ground truth is history, not live state |
| `seedMysteryFacts` twice | Idempotent — `addFact` is first-teller-wins |
| Debug reveal left enabled | Compile-gated off by default. Shipping the answer on a keypress is the worst possible bug here |

## Open Questions

1. How many witnesses and how much evidence? Quantity is the storyline
   templates' job, driven by their validator rather than guessed here.
2. Does the corpse persist across days, or clear after day 1?
3. Does the victim's `ConversationStore` memory survive? A dead NPC with a
   remembered history of the player is a loose end.
