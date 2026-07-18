# Plan: Fix In-Game Map Generation + Grow the Sandbox Catalog
Date: 2026-07-14
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
The in-game "Generate" flow appears to do nothing — and the bench already
measured why: it uses the all-in-one gen-village mode, which **0 of 3
local models can produce validly** (map + cast + cross-links in one giant
JSON), so after 2–4 minutes of silent retries it fails with only a
6-second status line nobody sees. The fix is staged generation (map first
— which models one-shot in seconds — then cast, then programmatic
placement from the cast's own position fields), plus persistent, visible
progress/failure UI. Alongside, the piece catalog roughly doubles using
assets the pack already ships but the catalog ignores (streetlight,
hydrant, watertower, crates, more cars, eight house variants) plus
composite-primitive recipes for the things the pack lacks: trees, hedges,
flower beds, rocks — and the v1-cut visual road pieces get their renderer
support so maps can have streets.

## Goal
Typing "a fishing village with a grumpy sailor" in the sandbox reliably
produces a visible, populated, editable map within ~a minute — with trees.

## Diagnosis (measured, not guessed)
- Bench (#127): gen-map validity — qwen2.5 one-shot in 2.1s, qwen3 passes;
  gen-cast — qwen3 passes; **gen-village — 0% across all three models**.
- The in-game chain calls `buildVillagePrompt` (gen-village). Failure after
  `kWorldGenMaxAttempts` shows one `worldgenSay` HUD line with a 6s TTL,
  drawn under the menu overlay if the menu is open. Net effect: "nothing
  pops up".

## Out of Scope (this version)
- Cloud models for generation (the pluggable backend now allows it — a
  config choice, not code).
- New art assets or texture work (composites + existing pack only).
- Piece rotation (still the standing sandbox v1 cut).
- Terrain/elevation. Streets remain flat visual paint with no colliders.

## Affected Areas
- `src/core/WorldGen.{hpp,cpp}` — staged pipeline: `buildMapPrompt`
  (exists), `buildCastForMapPrompt(description, mapSummary, traits)` (new:
  cast generation that SEES the map's pieces/open areas), placement
  derivation (cast persona `position` → PlacedNpc; validate against
  solids; a colliding/out-of-bounds position is a CastError fed back —
  still no auto-repair), `summarizeMapForPrompt(map)`.
- `src/app/main.cpp` — the generation chain becomes a small state machine
  (Phase::Map → Phase::Cast → done), each phase with its own retry budget;
  persistent `worldgenStatus` (no TTL while active; failure sticks until
  any input); status ALSO drawn on the Sandbox menu page.
- `src/app/Menu.cpp` — Sandbox page shows the live generation phase next
  to the Generate button; button disabled while active.
- `tools/worldgen_cli.cpp` — `gen-village` reimplemented over the staged
  pipeline (same modes, better numbers); bench re-run afterwards.
- `src/core/PieceCatalog.cpp` — catalog growth (below).
- `src/app/Assets.cpp` — curate the new asset ids (streetlight, hydrant,
  watertower, box_A, car_stationwagon, building_A..H as house pieces).
- `src/app/RaylibRenderer.cpp` — composite recipes for tree_round,
  tree_pine, hedge, flower_bed, rock (fountain pattern: primitives on the
  authored footprint); visual-piece rendering support: solid=false pieces
  draw (road paint via the existing road-tile models, flat, no collider) —
  buildCity emits them into a parallel `visuals` list on SandboxMap→City?
  Simplest: City gains a lightweight `decals` vector (id + footprint)
  rendered by drawCity; sandbox-only, empty for the downtown.
- Tests: `tests/test_sandbox_map.cpp` (visual pieces emit decals not
  Buildings; catalog floors), `tests/test_worldgen_validate.cpp` (staged
  placement derivation: valid positions pass, colliding positions become
  named CastErrors), catalog well-formedness auto-covers new rows.

## Piece catalog additions (~20 → ~40)
Real assets, unused today: `streetlight`, `firehydrant`, `watertower`
(landmark!), `crate` (box_A), `car_wagon`, plus `house_a`..`house_h`
(the eight building variants as distinct 3x2/4x3 pieces — no more
hash-luck). Composite recipes (no assets exist): `tree_round`,
`tree_pine`, `hedge` (1x2 solid), `flower_bed` (visual), `rock`.
Visual road paint (renderer support added this plan): `road_straight_x`,
`road_straight_z`, `road_corner`, `road_junction`, `road_tsplit`.
All pack "core", vocabulary auto-includes them (it renders from the
catalog), validator picks them up for free.

## Implementation Order
1. **Staged worldgen core** — map summary, cast-for-map prompt, placement
   derivation with named errors; CLI gen-village rewired; unit tests
   offline. *Committable.*
2. **In-game chain + UX** — phase state machine, persistent status, menu
   integration, failure that sticks. *Committable.*
3. **Catalog: real assets** — new rows + Assets curation; screenshot.
   *Committable.*
4. **Catalog: composites + visual pieces** — tree/hedge/flower/rock
   recipes, decal rendering for road paint; screenshot. *Committable.*
5. **Re-bench + report** — bench_schema_models.py over the staged
   pipeline; expect gen-village validity to jump from 0%; numbers to
   bench/REPORT.md. *Committable.*
6. **Visual QA gate** — generate a village live in-game (or via CLI +
   --map), screenshot the result with trees/streets, verdict to
   OVERNIGHT_REPORT.md.

## Acceptance Criteria
- [ ] CLI `gen-village` succeeds with at least one local model on the
      bench corpus (was 0%); numbers recorded.
- [ ] In-game: typing a description produces either an OPEN EDITOR with
      the generated map, or a failure message that stays visible until
      the player acts — never silence.
- [ ] Generation status is visible from the Sandbox page and in-world.
- [ ] Placement derivation: a cast position inside a solid or out of
      bounds yields a named CastError (fed back), never a nudge.
- [ ] Catalog ≥ 38 pieces; every new solid piece collides like a native
      building (existing combo/buildCity tests extended); visual pieces
      emit NO collider and DO render (new decal test + screenshot).
- [ ] Trees/hedge/flower/rock render as composites matching the toon
      style (outlined where sensible), screenshot-verified.
- [ ] `make -C tests test` green; game builds.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Map stage fails all retries | Failure sticks on screen; cast stage never runs; no partial artifacts. |
| Cast stage fails after map succeeded | Offer the MAP alone (open in editor, toast "characters failed — map opened without them"); the map was valid, don't discard it. |
| Cast position collides with a placed solid | Named CastError with the piece it hits; retried, never nudged. |
| Player starts a second Generate while one runs | Button disabled + toast (already guarded by worldgenRequestId). |
| Player quits mid-generation | In-flight replies drain harmlessly next boot (requests are per-session). |
| Old maps (no visual pieces) | Unchanged — decals list simply empty. |
| Model emits a visual piece where a solid is expected | They're normal catalog ids; validator/overlap rules already treat solid=false as non-colliding. |

## Open Questions
None — the failure mode is measured, the assets are inventoried, and the
staged design follows directly from which modes the bench showed models
CAN do.

## Suggested GitHub Issues
1. **feat(worldgen): staged map→cast generation with derived placements** — the 0%→working fix. (Concept: *decomposing generation to match measured model capability*.)
2. **feat(worldgen): persistent generation status + sandbox-page integration** — never silent again. (Concept: *long-running async UX in a game loop*.)
3. **feat(sandbox): catalog growth from shipped assets** — streetlight/hydrant/watertower/houses/cars. (Concept: *asset inventory before asset creation*.)
4. **feat(render): composite nature pieces + visual road decals** — trees/hedge/flowers/rock + solid=false rendering. (Concept: *decals vs colliders in a compiled map*.)
5. **chore(bench): re-measure staged gen-village validity** — prove the fix with numbers. (Concept: *regression benchmarks for prompts*.)
