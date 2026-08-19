# Plan: Sandbox Map Editor — Build Maps from Pieces, Place NPCs, Play Them
Date: 2026-07-12 (autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: XL

## The Idea (one paragraph)
A sandbox mode where the player assembles a map from predefined pieces —
buildings, props, street furniture drawn from the SAME asset/collision
systems the downtown uses — on a snapping grid, places NPCs into it (any
shipped persona or creator-made character, with their real persona and
look), saves it as a human-readable JSON file, and toggles into play mode
to walk it with dialogue fully working. This is composition, not new core
systems: a placed piece becomes a normal `Building` AABB (so it renders
through Assets and collides through City automatically), and a placed NPC
spawns through the same persona/look pipeline as the town roster. The JSON
save format is deliberately the future output contract for LLM-generated
maps (plan: llm-world-generation).

## Goal
The player builds a custom map from a piece palette, populates it with
premade and self-created characters, saves it, and plays it — talking to
the NPCs exactly as in town.

## Out of Scope (this version)
- Piece ROTATION (the KayKit Fill draw path is rotation-free by design —
  session-3 precedent; pieces place axis-aligned. Logged; rotation is a
  follow-up that touches the renderer contract).
- New art: every piece maps to models/recipes that already exist
  (curated ids + the hashed generic-building fallback + primitive
  composites like the fountain).
- Terrain/ground editing (ground stays the flat plane + auto slabs);
  roads as VISUAL tiles are included as flat "paint" pieces without
  colliders, but no road-network logic.
- Multiplayer in sandbox maps (solo only; host/join keeps the town).
- LLM generation of maps (separate dependent plan) — but the JSON format
  here is written for it: explicit ids, flat structure, no derived state.
- Undo/redo (delete + re-place covers v1; logged).
- Schedules in sandbox play — placed NPCs IGNORE their town schedules
  (schedule coordinates reference downtown; walking to (-70,-36) in a
  custom map is nonsense). They stay where placed; dialogue, moods,
  memory, gossip, follow all work. Logged decision, matches the brief's
  "schedules if applicable".

## Affected Areas
- New `src/core/PieceCatalog.{hpp,cpp}` — PieceDef {id, label, assetId
  (what Assets/renderer key on), footprint tiles (w×d), height, solid
  (collider or visual-only), styleTag, pack="core"} + `pieceCatalog()`
  data rows (~20 starter pieces: the five shop fronts, apartment/office
  fillers, tower, fountain, bench, bushes, traffic light, cars, cart,
  dumpster, trash, road-paint tiles). Adding a piece later = one row —
  the PartDef/palette pattern again.
- New `src/core/SandboxMap.{hpp,cpp}` — the document + FORMAT CONTRACT:
  `{ "version": 1, "name": "...", "tile": 8,
     "pieces": [{"piece": "<pieceId>", "x": <tileX>, "z": <tileZ>}...],
     "npcs":   [{"source": "persona:<stem>" | "character:<characterId>",
                 "x": <units>, "z": <units>, "facing": <deg>}...] }`
  toJson/fromJson (nlohmann, vendored), `validate(map, catalog)` (ids
  exist, in bounds, on grid, no solid-footprint overlaps, npc positions
  not inside solids) returning named errors (the same validator the LLM
  plan will reuse), and `City buildCity(map, catalog)` — pieces become
  Building rows: id = assetId + "#" + n, correct AABB from tile footprint,
  height, facadeKind. Collision and rendering follow for free.
- `src/app/Assets.{hpp,cpp}` (or the renderer's lookup) — curated-model
  lookup strips an instance suffix at '#' so "bakery#2" finds the bakery
  model (one small, tested helper; the ONLY renderer-side change).
- `src/core/World.{hpp,cpp}` — the repopulation seam (the structural
  heart, isolated in its own issue): `void loadCity(City city)` clears
  npcs_/projectiles_ and swaps the city. Npc holds an LlmClient& so World
  can't be reassigned — swapping CONTENTS in place is the design. Main's
  per-NPC side arrays (npcLooks, npcLastPos, wasCaught, savedTurns) and
  roster spawning get extracted into reusable lambdas
  (`spawnTownRoster()`, `spawnMapNpcs(map)`, `resetNpcSideArrays()`) so
  town ↔ sandbox ↔ town round-trips cleanly.
- `src/app/main.cpp` — `AppMode::SandboxEdit` (+ play uses the normal
  Playing mode against the loaded map):
  * Edit camera: high tilted vantage (fixed pitch ~-55°), WASD pans on
    the ground plane, scroll zooms height.
  * Cursor: mouse ray → ground plane → snapped tile; ghost preview of the
    current piece (drawn through the normal building path, tinted
    translucent); green/red tint by validity.
  * Input: click place, right-click delete (topmost piece/NPC under
    cursor), [ ] cycle piece, Tab toggles piece-vs-NPC placement, P play
    toggle, Escape back to sandbox menu (autosaves draft).
  * Play toggle: buildCity + loadCity + spawnMapNpcs; player spawns at
    map center (first free tile outward). Edit toggle restores the
    editor; leaving sandbox restores the town via the same seam.
- `src/app/Menu.{hpp,cpp}` — a Sandbox page: list `saves/maps/*.json`
  (name + piece/npc counts), New (name field), Edit, Play, plus the NPC
  picker in the editor reusing the roster + CharacterStore lists.
- New dir `saves/maps/` (saves/ already gitignored).
- Tests (all core, headless): `tests/test_piece_catalog.cpp` (rows valid,
  footprints positive, assetIds nonempty), `tests/test_sandbox_map.cpp`
  (JSON round-trip byte-stable, validate() per failure class, buildCity
  AABB math, '#' suffix stripping, overlap detection),
  `tests/test_world.cpp` extension (loadCity clears and swaps; collision
  against the new city works).

## Implementation Order
1. **PieceCatalog + SandboxMap + validate + buildCity** — the format
   contract and math, fully unit-tested. No UI. *Committable.*
2. **World repopulation seam** — World::loadCity + main's extracted
   spawn/side-array lambdas; town→town reload proves the seam headlessly.
   *Committable.*
3. **Asset instance-suffix lookup** ('#' strip) + a smoke screenshot of a
   buildCity'd map rendering with real models. *Committable.*
4. **Editor mode** — camera, snapped ghost cursor, place/delete, piece
   cycling, HUD hints; save/load JSON; sandbox menu page. *Committable.*
5. **NPC placement** — picker over roster + stored characters; placed
   NPCs render their real looks in-editor; play mode spawns them through
   the standard pipeline (schedules suppressed). *Committable.*
6. **Play toggle + polish** — center spawn, Escape/back flows, draft
   autosave. *Committable.*
7. **Visual QA gate (per the brief)** — build a small test map
   headlessly (write a fixture JSON), load it via a `--map <file>` smoke
   flag (add alongside --frames), place two NPCs in the fixture,
   screenshot, confirm pieces render at correct scale and NPCs stand
   with correct looks; collision verified by the core tests. Report into
   OVERNIGHT_REPORT.md. *Part of the wrap.*

## Acceptance Criteria
- [ ] A map JSON with every piece kind round-trips byte-stable and
      validates; each failure class (bad id, off-grid, out of bounds,
      overlap, NPC-in-wall) yields a named error (unit tests).
- [ ] buildCity produces Buildings whose AABBs match footprints exactly;
      a player circle collides with a placed solid piece identically to a
      native downtown building (core test via resolveMovement).
- [ ] Visual-only pieces (road paint) never block movement.
- [ ] In the editor: pieces snap to the 8-unit grid, invalid placements
      show red and are refused, delete removes under-cursor.
- [ ] Any shipped persona AND any created character can be placed; in
      play mode they use their exact stored persona/look, dialogue works,
      schedules do NOT fire (test: scheduled NPC stays placed).
- [ ] Save → quit → load reproduces the map (JSON is the only state).
- [ ] Town ↔ sandbox ↔ town round-trip leaves the town fully functional
      (roster, looks, journal, combat arrays sized right).
- [ ] `--map <file>` smoke flag renders a fixture map headlessly for
      screenshots; visual-qa report in OVERNIGHT_REPORT.md.
- [ ] `make -C tests test` green; game builds.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Map references a deleted created-character | That npc entry is skipped with a logged reason at load; the map still opens (mirrors stale-look demotion). |
| Map JSON malformed / wrong version | Named error in the sandbox menu; nothing loads; the file is never rewritten. |
| Piece id unknown (catalog changed) | validate() names it; the map opens in EDIT mode with the offending entries dropped and a toast, so the player can repair (logged decision: repairable > rejected for hand-made maps; the LLM path uses strict validate()). |
| Overlapping solid placement attempted | Ghost shows red; click refused with a toast. |
| NPC placed inside a solid | Refused (same red ghost rule). |
| Play-mode player spawn blocked | Spiral outward to the first free tile (the plaza-ring pattern from Save & Spawn). |
| Dismissing/killing a placed NPC in play mode | Normal world behavior; the MAP FILE is untouched (play mutates the world, never the document). |
| Editing while a dialogue/LLM request is in flight | Entering sandbox closes dialogue and drains routes, same as menu-quit path. |

## Open Questions
None blocking — decisions logged: 8-unit tile, no rotation, schedules
suppressed in sandbox, '#' instance suffix, repairable hand-load vs
strict LLM-load, XL sizing driven by the World repopulation seam.

## Suggested GitHub Issues
1. **feat(sandbox): piece catalog + map format + validate + buildCity** — the data contract, fully tested. (Concept: *compile-to-existing-systems: an editor with no new runtime*.)
2. **feat(world): repopulation seam — loadCity + extracted spawn paths** — town↔map switching. (Concept: *swapping world contents when the world can't be reassigned*.)
3. **feat(render): instance-suffixed asset lookup + map smoke flag** — '#' strip + --map. (Concept: *stable ids vs instance identity*.)
4. **feat(sandbox): editor mode — camera, ghost cursor, place/delete, save** — the editing loop. (Concept: *ray-to-grid cursor with validity preview*.)
5. **feat(sandbox): NPC placement from both character pools** — picker + play-mode spawning. (Concept: *one spawn pipeline, three entry points*.)
6. **chore(qa): fixture map + visual gate + report** — screenshots + collision confirmation. (Concept: *fixture-driven visual verification*.)
