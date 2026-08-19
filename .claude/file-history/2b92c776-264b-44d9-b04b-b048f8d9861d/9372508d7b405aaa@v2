# Plan: Issue #66 — street life: parked cars and alley props with colliders
Date: 2026-07-05 (overnight session 3, autonomous)
Status: READY FOR IMPLEMENTATION
Estimated complexity: S

## Key constraint discovered (decision logged)
KayKit cars are Z-long at identity (probed: 0.42 x 0.94 footprint) and the
Uniform draw path applies no rotation, so every parking spot goes on the two
Z-RUNNING streets (x = +-32) where identity orientation reads correctly.
"Police car outside the station" therefore parks on the west street's east
edge directly beside the station block (and a second car flanks its east
side) rather than on the X-running station-front street — the pure-data
alternative to adding rotation support, per the issue's "zero new rendering
code" economy note.

## Placements (avoid zebra tiles at +-16/+-48, junctions +-32, personas)
Free curb stretches on x=+-32 streets: z in [-8,8], [56,72], [-72,-56].
- police_car  (-28.8,-64.0)-(-25.0,-57.5)  beside station block, west flank
- hatchback_b ( 25.0,-70.0)-( 28.6,-63.5)  beside station block, east flank
- sedan_a     ( 35.0, -7.5)-( 38.8, -1.5)  hardware block curb (clear of Hal
                                            at (36,0) by >1 unit)
- sedan_b     ( 35.0, 60.0)-( 38.8, 66.5)  park curb
- hatchback_a (-39.3, 58.0)-(-35.8, 64.5)  apts_c curb
Alley props (SW alley between apt_a/apt_b; coffee/office alley):
- dumpster_a  (-67.2,-86.0)-(-63.2,-83.5)  (dumpster is X-long — fits alley)
- trash_a     (-62.5,-87.4)-(-61.1,-86.0)
- trash_b     ( 45.0,-65.4)-( 46.2,-64.2)
All in City::makeDowntown with real AABBs; nothing in the plaza, nothing in
front of a shop door, nothing on a crossing.

## Changes
- City.cpp: 8 new authored obstacles (facadeKind 13 cars, 14 props).
- Assets.cpp: curated stems (car_police/car_sedan/car_hatchback/dumpster/
  trash_A/trash_B) + Uniform specs (police 1.5, cars 1.35, dumpster 1.4,
  trash 0.9/0.7); add "trash_B" to the preload list.
- tests/test_city.cpp: extend the completeness table with the 8 new ids.

## Acceptance
- [ ] Completeness test covers every new id (findBuilding + collision).
- [ ] Screenshots: cars along both Z-running streets, police car beside the
      station block, props in the SW alley.
- [ ] Suite green; walk routes clear by construction (placements above).
