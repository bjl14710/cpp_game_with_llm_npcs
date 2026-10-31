# Plan: Correct-Accusation (Win) Cutscene
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued. Most dependent of the set.
Estimated complexity: M

## The Idea

When the players name the real killer they win. This is the payoff: **mysterious
and cartoony rather than violent** — the reveal, not the punishment. A short
montage replays the clues that actually pointed at the killer — first the ones the
players found, then briefly the ones they missed — before the killer is led away.

The montage is the interesting part, because it has to name *this match's* real
evidence. **That contract is this plan's main deliverable**, and it must be
honoured by the murder, testimony and storyline plans rather than retrofitted.

## Goal

A group that catches the killer sees exactly why they were right — and what they
nearly missed — in a few seconds, and wants to play again.

## Decisions taken

| Question | Answer |
|---|---|
| What counts as "the clues that mattered"? | **What you found, then what you missed.** Intersect the ground-truth chain with what players actually learned; replay the former, then briefly the latter. |

## The flashback problem, and why it is not a problem

The cutscene system reads the **live world** and cannot replay past state. There is
no recorded history of what the town looked like at 21:40 on day one, and building
one would be large.

It does not need one. **The places still exist.** A flashback is a montage of
camera cuts to the *locations* where the clues were, each with a caption:

> *Bakery Corner* — "She said she was home all evening."
> *Coffee Row* — "Ray saw her here at 9:40."
> *The Plaza* — "The clock says otherwise."

Real locations, real chain, zero new machinery. It also composes with the zones
plan, which gives every clue a `zoneId` with a showable name.

## The contract this plan defines

```cpp
struct ClueStep {
    std::string zoneId;      // where the montage cuts to
    std::string caption;     // <= 140 chars, fact-shaped
    std::string factId;      // empty for evidence with no committed fact
    int order = 0;           // AUTHORED chain order, not discovery order
};

std::vector<ClueStep> solutionChain(const MysterySetup& setup);

struct MontagePlan {
    std::vector<ClueStep> found;   // the "I was right" beat
    std::vector<ClueStep> missed;  // the "there was more" beat
};
MontagePlan buildMontage(const std::vector<ClueStep>& chain,
                         const WorldState& state, const std::string& agent);
```

**Requirements this pushes back onto earlier plans:**

- **Opening murder:** `MysterySetup::evidence` must carry a `zoneId` and a
  caption-length description. It already does; this pins them as load-bearing
  rather than incidental.
- **Alibis and testimony:** seeded facts bearing on the killer need stable
  `factId`s the chain can reference, and `WorldState::knows` is how
  found-versus-missed is computed. Already true; now depended upon.
- **Storyline templates:** a template must author its chain **in order**. This is
  the requirement that is cheap now and expensive later — a template producing an
  unordered bag of clues cannot drive a montage that reads as reasoning.

`order` is authored chain order, **not** discovery order. The montage should read
as an argument, not as a replay of the players' wandering.

## The multiplayer note

`journalEntries` reads facts known by the agent id `"player"` — singular. Whether
each networked player is a distinct knowledge agent is unresolved. Until it is,
**the montage uses the host's knowledge agent and is shared identically by
everyone.** Correct for a co-op win, and it avoids inventing a per-player
knowledge model to serve a cutscene. The testimony plan should decide it properly.

## Out of Scope

- Replaying past world state.
- Character animation. "The mask drops" is figurative — a caption and a cut. The
  killer being led away is a movement override, as in the wrong-accusation plan.
- Violence of any kind.
- Sound.
- A post-match scoreboard, stats, or rematch flow.
- Per-player montages.

## Design

`cutscenes/win.cutscene` is a **skeleton**, not a fixed shot list, because the
montage length varies with the chain: an `accused` bookend naming the killer, then
generated montage beats spliced in, then a `led_away` bookend.

`buildWinCutscene` splices one beat per found clue at ~1.4 s, then the missed ones
at ~1.0 s under a single "What you missed" caption. **Total duration is capped at
the phase budget** — if the chain is long the montage trims to the highest-`order`
clues rather than overrunning, **and logs what it dropped.** Silent truncation
would read as "that was the whole case" when it wasn't.

This is the first cutscene where the killer's name is legitimately on screen — the
match is over and `MatchOver` has already carried it.

## Implementation Order

1. `ClueStep` and `buildMontage` + tests. Pure: a chain and a `WorldState` in, a
   found/missed split out. The whole contract is testable with a hand-built chain.
2. `buildWinCutscene` — splice between the bookends, with the duration cap and
   trim-with-logging.
3. Author `cutscenes/win.cutscene`; capture beats for `visual-qa`.
4. Dispatch on a `Correct` outcome, plus the killer departure override.
5. Tone review, same as the wrong-accusation plan.

## Acceptance Criteria

- [ ] A chain of five clues where the player knows three → `found` has those three
      in authored order, `missed` has the other two.
- [ ] **A player who knows none of the chain (a lucky guess)** → `found` empty,
      `missed` the whole chain, and the cutscene still plays coherently. This is
      the anticlimax case and must not produce an empty montage.
- [ ] A player who knows all of it → `missed` empty, no "what you missed" section.
- [ ] A chain longer than the phase budget → trims to the highest-`order` clues
      **and logs what was dropped.**
- [ ] Montage beats appear between the `accused` and `led_away` bookends, in
      `order`.
- [ ] The killer walks before being marked as taken away.
- [ ] No frame shows violence. Judged by `visual-qa`.
- [ ] A `Wrong` outcome never plays this.
- [ ] A clue whose `zoneId` no longer resolves is **skipped**, not cut to the
      origin.

## Edge Cases

| Situation | Behaviour |
|---|---|
| The chain is empty | Bookends only: name the killer, lead them away. Degraded but coherent |
| A clue has no `zoneId` | Killer-relative camera, caption alone |
| Two clues share a `zoneId` | Both play; the camera holds and the caption changes |
| The killer was accused and killed earlier | Cannot happen — a correct accusation ends the match, and the dead cannot be nominated |
| A client misses the dispatch | Cosmetic; `MatchOver` already ended the match authoritatively |

## Open Questions

1. **Is each networked player a distinct knowledge agent?** Unresolved in the
   testimony and match plans. Until decided the montage is shared and uses the
   host's agent. This affects far more than this cutscene.
2. Should "what you missed" show at all on a confident win? A group that found
   four of five might rather not be told.
3. Does "the mask drops" want any visual beyond a caption and a cut? Per-NPC tint
   would serve it — **the same enhancement the wrong-accusation plan flagged.** Two
   consumers now; worth doing once.
