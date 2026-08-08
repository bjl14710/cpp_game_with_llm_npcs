# Plan: Twenty-One Distinct Residents
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — issues #170, #171, #172 (done), #173, #175
Estimated complexity: L

## The Idea

Detective mode needs 21 residents, one of whom dies at the start. The hard part
is not writing eleven more `.persona` files — it is that a player under time
pressure has to tell 21 characters apart by silhouette, voice and location, and
that the game has to render them at a playable framerate. **Both halves were
measured before this plan was written, and both are worse than they look.**

## Goal

A player can walk into downtown, meet 21 residents who are individually
recognisable at a glance and by how they talk, at a framerate no worse than
today's ten.

## Measurement 1 — render cost (2026-08-07, M1 Pro)

Method: `./build/cpp_game_with_llm_npcs --frames N --camera 0 30 180 --hour 12`,
timed at 300 and 900 frames so startup cancels out of the marginal cost. Eleven
throwaway personas generated, measured, deleted. Encoded as
`tools/bench_npc_render.py`.

| Roster | 300 frames | 900 frames | Marginal | fps |
|---|---|---|---|---|
| 10 personas | 11.13 s | 26.98 s | **26.4 ms** | 37.9 |
| 21 personas | 14.91 s | 36.11 s | **35.3 ms** | 28.3 |

- **~0.81 ms per NPC per frame.** Enormous for one character.
- Startup grew 3.2 s → 4.3 s.
- **The game is already under 60 fps at ten.** This does not start healthy.

Cause: no distance culling anywhere, and each NPC draws its whole composite a
*second* time for the inverted-hull outline pass.

**Caveat found later:** the marginal figure depends on the frame counts chosen,
because early frames carry shader compilation. A 120/360 run reports 17.8 ms
where 300/900 reports 26.4 on the same build. Always quote 300/900.

**The LLM is not the constraint.** `LlmClient` runs one worker thread for all
NPCs and only one conversation happens at a time (groups cap at 3). An idle NPC
costs zero tokens and zero latency. Roster size never touches the model bill.

## Measurement 2 — the silhouette budget (2026-08-08, found by issue #172)

**This one invalidated the plan's original arithmetic.**

Parts carry a style family (`round` or `blocky`) and `lookIsValid` rejects any
mix. So six core bodies and four core heads are **not 24 combinations:**

```
3 round bodies  x 2 round heads  = 6
3 blocky bodies x 2 blocky heads = 6   ->  12, not 24
```

The current ten already use **ten of the twelve**. 21 distinct silhouettes is
**unreachable from this catalog by any arrangement, independent of framerate.**

Found by trying to fix a real shipped collision: `barista` and `librarian` both
wore `body_slim/head_oval`. Moving the barista to `head_tall` failed with
*"body_slim and head_tall mix incompatible styles"*. She is now
`body_pear/head_oval`, the only unused round-family pair.

**Fix: four new core parts** (one body and one head per family) → 4×3 + 4×3 =
24. Three would reach exactly 21 with zero slack. Tracked as issue #175.

## Decisions taken

| Question | Answer |
|---|---|
| How to handle the measured 28 fps? | **Cull first, then add the cast, in one milestone.** Shipping at 28 fps means doing the work later under worse conditions. |
| The catalog can't seat 21 — what now? | **Add core parts** (2026-08-08). Four, for headroom. Issue #175. |

## Out of Scope

- The murder, victim, killer, roles, storylines. This produces 21 *ordinary*
  residents.
- Voice acting, per-character models, new art beyond the four parts in #175.
- A general LOD system, mesh LODs, impostors, instancing. 21 characters is not
  a culling problem.
- Frustum culling — distance only; frustum has its own correctness questions.
- Procedural crowds. Would spend the budget this milestone buys back.
- Any change to the LLM path. Measured as a non-issue.

## What voice distinctness can and cannot be tested

**Voice cannot be unit-tested.** The honest proxies are a unique trait set and a
unique `style` string; genuine distinctness is a review judgement, and the plan
says so rather than pretending otherwise.

**Related finding (#172):** no shipped persona uses the structured trait
library. All ten have empty `traitIds`; `traits/*.trait` ships eight traits with
rules and few-shot examples that nothing references. The gate therefore checks
the free-text `traits =` line (unique 10/10), with a test asserting `traitIds`
stay empty so the day someone uses them it prompts moving the gate to the
stronger field.

## Implementation Order

Performance and catalog first — authoring 21 residents against a budget that
fails, or a pool that cannot hold them, means rewriting them.

1. **Distance-cull NPC rendering** (#170). Radius derived from the fog falloff
   (`fogDensity = 0.006`) so the two cannot drift and cause popping. Re-measure.
2. **Skip the outline pass at range** (#171). The composite is drawn twice;
   dropping the second pass beyond ~25 units where the rim is sub-pixel should
   be the biggest single win. **Gate: 21 residents must reach ≤ 26.4 ms/frame
   before any persona is written.** If the two cuts miss it, stop and re-scope.
3. **Diversity gate** (#172, DONE — PR #174). Silhouette, trait-set and style
   uniqueness at the current ten, plus a prove-it-can-fail case.
4. **Four new core parts** (#175). Takes the pool from 12 to 24 silhouettes.
5. **Author 11 residents** (#173) and flip the roster assertion to 21, with a
   closing frame-cost measurement.

## Acceptance Criteria

- [ ] 21 residents reach marginal ≤ **26.4 ms/frame** at 300/900.
- [ ] An NPC beyond the cull radius is not drawn and runs no outline pass.
- [ ] The outline still draws at conversational range; no pop at the boundary.
- [ ] **Never cull the NPC the player is talking to.**
- [ ] Culling is render-only: update, schedules, gossip and the location log
      keep running for off-screen residents. A mystery where distant NPCs stop
      having alibis is broken.
- [ ] No two residents share a body+head silhouette, trait set, style string or
      home position.
- [ ] The gate **fails** on a deliberate duplicate.
- [ ] ≥ 21 style-compatible body/head pairs exist in the core catalog.
- [ ] A human can name residents from silhouette alone in a plaza lineup —
      judged by `visual-qa`, not a unit test.
- [ ] `make -C tests test` green.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Silhouette pool still too tight after #175 | Add another head per family rather than weakening the gate |
| NPC culled mid-conversation | Never happens — the conversation partner is exempt |
| Cull radius shorter than fog distance | Popping. Radius must exceed full-haze distance; assert it |
| Persona schedule hours outside 0–23 | Already rejected by the loader with a named error |
| The 21st resident dies at match start | Nothing may hard-code 21 as a *live* count |

## Open Questions

1. The exact cull radius — derive from `fogDensity`, record the number.
2. Do 21 residents need 21 distinct home locations? Six named buildings exist;
   interacts with the zone plan.
3. Relationships between residents — in the original ask, but not testable and
   not needed until the mystery casts them. Deferred.
