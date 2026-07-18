# Plan: Derived Facing, Collision Completeness, Weapon Viewmodels
Date: 2026-07-05 (overnight, autonomous — decisions logged)
Status: READY FOR IMPLEMENTATION
Estimated complexity: M

## Premise verification (all three confirmed real, decisions logged)
1. **Moonwalk — confirmed, and the brief's diagnosis is exactly right.**
   `Npc::facingDeg_` is stored state, set by conversational behaviors
   (faceToward) — but `World::updateCombat`'s flee/hostile movement writes
   `npc.position()` directly and never updates facing, so a fleeing NPC
   walks backward-facing-you by construction (introduced with yesterday's
   combat port; Follow/ReturnHome behaviors set facing manually and could
   drift the same way). Fix at the source: facing derives from actual
   per-frame displacement, one method, one threshold.
2. **Clipping — confirmed as a data-completeness gap.** Collision comes
   solely from `City::buildings_` AABBs. Traffic lights (4) and park bushes
   (4) are drawn from hardcoded positions in RaylibRenderer with NO
   colliders — walk straight through them. Everything else audited: taxi/
   cart/bench/fountain have colliders; sidewalk slabs and road strips are
   flat (walkable by design, not clipping). Fix: move lights/bushes into
   City as authored obstacles so visuals render FROM collision data —
   one source, footprints match by construction.
3. **Weapon animations — confirmed missing entirely.** First-person view has
   no weapon viewmodel; core already tracks the single authoritative pair
   (`player().weapon`, `attackAnimFraction`) but nothing renders it. Fix:
   a viewmodel driven only by those two values, per-weapon visuals in a
   table keyed by WeaponKind (new weapons add a table row, not a branch).

## The Idea (one paragraph)
Unify three splits of the same disease: facing vs velocity (derive facing
from motion in one place), visuals vs colliders (obstacles authored once in
City, rendered from that data), and equipped-state vs animation (viewmodel
reads the one authoritative weapon + attack-animation value). Each fix is
the source-level version, so new animation states, new obstacle types, and
new weapons can't reintroduce the bugs.

## Out of Scope
Skeletal first-person arms (primitive/prop viewmodel only), NPC weapon
visuals, strafing/running player animations, multiplayer replication of
combat facing (falls out automatically — facing is in NetNpcPose already).

## Affected Areas
- `src/core/Npc.{hpp,cpp}` — `deriveFacingFromMotion(prev, dt)`: facing =
  atan2 of displacement when speed > threshold; called once per frame after
  ALL movement (behavior + combat). Behaviors stop hand-setting facing
  while moving; standing-still facing (lookAt during chat) is preserved by
  the threshold.
- `src/app/main.cpp` — the existing per-NPC position-delta loop (already
  tracks npcLastPos for walk detection) also calls the derivation; player
  viewmodel render call.
- `src/core/City.{hpp,cpp}` — makeDowntown gains authored obstacle entries
  for the 4 traffic lights and 4 park bushes (small AABBs, real collision).
- `src/app/Assets.cpp` — curated stems + Uniform size specs for
  "trafficlight_*" and "bush_*" ids.
- `src/app/RaylibRenderer.cpp` — delete the hardcoded light/bush draw
  loops (they now render via the normal buildings pass); add
  `drawViewmodel(const Player&)`: per-WeaponKind table {shape, rest pose,
  attack pose}, animated by attackAnimFraction (punch thrust / recoil).
- `tests/test_npc_behavior.cpp` or new — facing derivation unit tests;
  `tests/test_city.cpp` — obstacle-completeness (lights/bushes collide).

## Implementation Order
1. Derived facing (core + call site + tests).
2. Obstacle completeness (City data + Assets specs + renderer cleanup + tests).
3. Weapon viewmodel (renderer + main).
4. Screenshot verification + report.

## Acceptance Criteria
- [ ] Unit test: combat-flee movement then derivation → facing within ~1° of
      the motion direction; standing NPC keeps its lookAt facing.
- [ ] Unit test: `circleIntersectsAny` is true at every traffic light and
      bush position; renderer no longer hardcodes their positions.
- [ ] Viewmodel visible in screenshots (fist and gun), animated on attack;
      switching weapons switches it with zero per-weapon code branches
      outside the table.
- [ ] Suite green; game builds and smoke-runs.

## Edge Cases
| Situation | Expected behaviour |
|-----------|--------------------|
| NPC nudged a tiny distance (< threshold) | Facing unchanged — no flip while idling/chatting. |
| NPC dead | No derivation (corpse keeps its final facing). |
| Obstacles added later | Author once in City; renderer picks them up via curated stems — nothing else to touch. |
| New WeaponKind ported | Add a viewmodel table row; everything else derives. |

## Open Questions
None — plan is complete.

## Suggested GitHub Issues
1. fix(npc): derive facing from motion — kills the moonwalk class of bugs
2. fix(city): author colliders for every visible obstacle (lights, bushes)
3. feat(render): first-person weapon viewmodel driven by equipped state
