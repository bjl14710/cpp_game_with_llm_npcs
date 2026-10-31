# Plan: Entity Size Contracts + Weapons Port (+ UI verification)
Date: 2026-07-05 (overnight, autonomous — decisions logged)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## Premise reconciliation (decisions logged per autonomy rules)
1. **"Sprite pixel dimensions" → collision-AABB stretch.** The game renders
   3D glTF models, not sprites. The actual defect matches the brief's spirit
   exactly: `drawModelFittedToAABB` stretches every model non-uniformly to
   fill its collision AABB, so the taxi (8x5x1.6 collision box) renders as a
   squashed blob, the fountain's stand-in is a pancake, and the hot-dog cart
   is a stretched crate — while characters scale uniformly to a declared 1.8
   world height. The size contract goes in at the entity-type level as the
   brief demands; no per-asset scale constants.
2. **Menu + dialogue UI already exist** — shipped tonight (PR #52, merged to
   main): pause menu (controls rebinding, multiplayer host/join) and the
   free-text streaming dialogue box. This ticket verifies them on the branch
   and adds nothing. If "speaking options" means canned selectable replies
   (Fallout-style), that is a real design change — flagged plainly in the
   report as a separate scoping question, not invented tonight.
3. **Weapons exist** on `initial_npcs_and_world` (the merged weapons PRs
   #14-#20 — the only weapon branch, so "most complete/recent" is
   unambiguous). Issue #46 holds the integration map from tonight's earlier
   aborted union merge. Known hazard: that code ships its own TODO stubs
   (e.g. `World::updateCombat` pieces) — the port must audit each function,
   implement what's stubbed, or cut it cleanly (repo rule: no TODOs), with
   omissions listed in the report.

## The Idea (one paragraph)
Give every rendered entity a declared world-size contract — buildings fill
their footprints (Fill mode), vehicles/props/characters scale uniformly to a
declared world height and stand centered on their footprint (Uniform mode) —
so relative sizes are right by construction and swapping art packs never
requires touching scale code. Then port the combat core from the weapons
branch (health/state machine, weapons, projectiles, combat events), finish
its stubs, and rebuild its thin app layer (attack input, HUD, death screen,
combat callouts) on raylib.

## Goal
Cars, carts, and fountains look properly sized next to people; the player can
draw a weapon, attack an NPC, see them flee or fight back, and die trying —
all on the raylib renderer.

## Out of Scope (this version)
- Canned "speaking option" dialogue choices (flagged as a design question).
- New weapon types beyond what the weapons branch defined.
- Multiplayer combat replication (combat runs host-side only; snapshot
  replication of combat state is follow-up — kNetProtocolVersion untouched
  tonight means joined guests don't see combat state changes yet; logged).
- Any new art or UI framework.

## Affected Areas
- `src/app/Assets.{hpp,cpp}` — SizeSpec table: per entity id/type, mode
  (Fill|Uniform), declared world height, applied in one place.
- `src/app/RaylibRenderer.{hpp,cpp}` — drawModelFittedToAABB splits into
  fill vs uniform placement; prop callers pass specs.
- Combat port (from origin/initial_npcs_and_world, per issue #46 map):
  `src/core/CombatEvents.hpp`, `Player.hpp`, `Weapon.hpp`, `World.{hpp,cpp}`
  combat API (audit + finish stubs), `Npc.{hpp,cpp}` health/state machine
  (union with mood-era code), `personas/cop.persona`, both combat test files.
- `src/app/main.cpp` — attack input, weapon HUD, death screen, combat
  callouts (rebuilt small on raylib, referencing the branch's SFML version).
- `tests/` — ported combat tests must pass; new size-contract unit test not
  applicable (rendering) — sizes verified by screenshot.

## Implementation Order
1. Size contracts (Assets SizeSpec + renderer Fill/Uniform paths + audit of
   every drawn entity) — screenshot-verified.
2. Combat core port + stub audit + tests green.
3. Combat app layer on raylib (input/HUD/death/callouts) — screenshot-verified.
4. Report updates (decisions, omissions, speaking-options flag).

## Acceptance Criteria
- [ ] Taxi/cart/fountain render at plausible sizes next to characters
      (screenshot), with zero per-asset scale constants in code.
- [ ] Ported combat tests pass alongside the existing 106.
- [ ] No TODO markers anywhere in ported combat code.
- [ ] Attack input damages an NPC; civilians flee; cops fight back; player
      death shows the death screen (smoke + screenshot where possible).
- [ ] `make -C tests test` green; game builds and smoke-runs.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| Model missing for a sized entity | Same primitive fallback as today, at the declared size. |
| Uniform model wider than its collision AABB | Visual may overhang the footprint slightly — accepted (visual/collision split is deliberate); noted for polish. |
| Combat stub unimplementable tonight | Cut cleanly + listed in report, never committed as TODO. |
| Dead NPC talked to | Conversation blocked with a system line (state machine guards). |

## Open Questions
Speaking options (canned replies) — design question for the user, flagged in
the report.

## Suggested GitHub Issues
1. feat(render): per-entity world-size contracts (Fill/Uniform) — step 1
2. feat(combat): port + finish weapons core; raylib combat app layer — steps 2-3
