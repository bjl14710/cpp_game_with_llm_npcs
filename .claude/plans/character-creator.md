# Plan: Character Creator — player-authored persona + socketed look
Date: 2026-07-05 (overnight, autonomous — decisions logged here and in OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
A Mii-style character creator inside the existing pause-menu system: the
player writes a personality (name, backstory, traits) and assembles a look
from interchangeable 3D parts (body, head, eyes, hair) with a palette, sees
a live preview, and spawns the character into the town where it talks
through the exact same LLM persona pipeline as the designer-authored NPCs.
Personality and look are two fully independent records, stored separately
in the game's established SQLite persistence, keyed by character_id.

## Goal
The player can create a named character with a custom personality and a
custom parts-based look, spawn it in the town, walk up and have a
conversation with it — and find it still there (same look, same
personality, same memory hooks) after restarting the game.

## Premise corrections vs the brief (decisions logged)
1. "Sprite pixel dimensions" — the game is 3D now (raylib 5.5, KayKit
   rigged characters, primitives). The size-contract system is about WORLD
   size vs native MODEL dimensions. Mii-style parts therefore = low-poly
   3D primitive composites (the issue-#64 fountain pattern), not sprites.
   KayKit's rigged characters are single skinned meshes and cannot donate
   swappable heads/hair, so parts are authored primitive recipes in the
   same flat-color art style.
2. "Storage approach the conversation-persistence work established" — that
   shipped (vendored SQLite + ConversationStore, merged to main via #57/
   #59). CharacterStore follows its exact pattern.

## Core design decisions (autonomous, most-defensible)
- **Socket contract**: every PartDef declares, in ITS OWN local space:
  an implicit anchor at its origin, its local bounding size, and a map of
  named child sockets ("head", "eyes", "hair" → Vec3). Assembly recursively
  places each part at its parent's socket; there is no per-pair offset
  table anywhere. Adding a part = adding one PartDef; adding a category =
  one more socket name, no restructuring.
- **Size-contract interaction** (brief flagged it): sockets resolve in
  UNSCALED local space first; the finished assembly is measured and then
  uniformly scaled so its total height equals the character height
  contract (1.8 world units, same as pack characters). One Uniform
  contract for the whole assembly — no parallel sizing system, no per-part
  world sizes.
- **Style tags**: each part carries a styleTag ("round", "blocky", or
  "any"). A look is valid iff all its parts are pairwise compatible
  (equal tags, or either is "any"). The picker and randomizer only offer
  compatible parts; the core validator enforces it.
- **Persona format**: player input is serialized to the SAME `.persona`
  key=value text format the roster uses and parsed back with the existing
  `parsePersonaText` — one format, one parser, zero new dialogue logic.
- **Independence**: `character_look` and `character_persona` are separate
  SQLite tables (and separate C++ records) joined only by character_id, per
  the ticket. The spawn path is the only place both are read.
- **Animation v1**: composite characters get procedural motion only
  (walk bob + idle sway derived from the walking flag) — no skeleton.
  Rigged animation for composites is out of scope; logged as future work.
- **Live preview**: while the creator page is open, main.cpp draws the
  draft look as a composite character a few units in front of the camera
  (the menu already renders over the live 3D frame, so the preview is a
  real in-engine render, rotating slowly for inspection).
- **Branch/PR**: one branch `feature/character-creator` stacked on the
  city-polish chain tip (feature/issue-67-atmosphere-pass) because it
  touches RaylibRenderer/Assets/main which every open PR in the chain
  modifies. One draft PR closing the five tickets below. No merges.

## Out of Scope (this version)
- Rigged/skeletal animation for composite characters; gestures/mood
  billboards for them beyond facing + bob.
- Editing or deleting an existing character (create + spawn only).
- AI-generated personas (the VISION.md cloud-creator concept), part
  unlocks, sharing/export, more than 4 categories (the system leaves room;
  v1 ships body/head/eyes/hair).
- Multiplayer replication of custom looks (NetNpcPose carries no look;
  bump kNetProtocolVersion when that lands).

## Affected Areas
- NEW `src/core/CharacterParts.{hpp,cpp}` — PartDef, the built-in catalog,
  socket assembly (`assembleLook`), style compatibility, `randomizeLook`,
  palette presets. Pure logic, no raylib — fully unit-testable.
- NEW `src/core/CharacterStore.{hpp,cpp}` — SQLite store, two tables
  (`character_persona(character_id, persona_text, updated_at)`,
  `character_look(character_id, look_json, updated_at)`); save/load/list.
  Follows ConversationStore's open/ok/degrade pattern.
- NEW `src/core/CharacterRecord.hpp` — CharacterLook {partId per category,
  paletteId} + (de)serialization to a tiny JSON string; CharacterPersona =
  persona text + spawn position (kept separate from look).
- `src/core/PersonaLoader.hpp/.cpp` — no change (parsePersonaText reused);
  NEW small helper `renderPersonaText(...)` (inverse serializer) added to
  PersonaLoader so format knowledge stays in one file.
- `src/app/RaylibRenderer.{hpp,cpp}` — `drawCompositeCharacter(const
  CharacterLook&, Vec3 pos, float facingDeg, bool walking, float phase)`:
  maps part ids → primitive recipes (flat KayKit-palette colors), applies
  assembly transforms + the 1.8 height contract.
- `src/app/Menu.{hpp,cpp}` — new Page::Creator (reached from Main):
  name/backstory/traits text fields (joinAddress_ editing pattern),
  per-category part cyclers, palette cycler, Randomize, "Save & Spawn";
  CreatorHooks injected by main (onSave, draft-look accessor for preview).
- `src/app/main.cpp` — creator wiring: hooks, preview draw, spawn (Npc +
  world.addNpc + resize wasCaught/npcLastPos), custom-look registry
  (npc index → CharacterLook) so the render loop picks composite vs pack
  model; startup load of all stored characters; memory hooks work as-is
  (keyed by persona name, unchanged).
- `CMakeLists.txt` — new core sources (tests auto-glob).
- NEW `tests/test_character_parts.cpp`, `tests/test_character_store.cpp`.

## Implementation Order (each step committable)
1. Core parts + sockets + catalog + randomize + tests.
2. CharacterRecord + CharacterStore (SQLite) + tests.
3. Renderer: drawCompositeCharacter honoring the height contract;
   screenshot verification of several part combinations.
4. Menu Creator page + main.cpp preview/spawn/persist wiring; screenshot.
5. Tickets closed, draft PR, OVERNIGHT_REPORT update.

## Acceptance Criteria
- [ ] Core: every style-compatible combination of catalog parts assembles
      with every category placed and total height > 0; incompatible
      combinations are rejected; randomizeLook(seed) is deterministic and
      always valid. Tests in test_character_parts.cpp.
- [ ] Store: save/load round-trips look and persona independently by
      character_id; corrupt rows degrade (skip, keep going); missing DB →
      ok()==false and the game still runs. Tests in
      test_character_store.cpp.
- [ ] Renderer: screenshots of ≥3 distinct part/palette combinations show
      correctly snapped parts (no floating/embedded parts) at the same
      world height as pack NPCs.
- [ ] UI: creator reachable from the pause menu; typing works in all text
      fields; randomize changes the preview; Save & Spawn places the
      character in the town with a nameplate.
- [ ] Spawn/persist: created character talks via the LLM using its authored
      persona; after restart it re-spawns with the same look and persona.
- [ ] `make -C tests test` green; game builds and smoke-runs.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| Empty name on save | Toast "name required"; no save, stay on page. |
| Empty backstory/traits | Allowed; persona renders without those lines. |
| Name collides with roster NPC | Allowed (names aren't keys); memory rows are keyed by name, so a duplicate shares memory — logged as known v1 quirk in the PR. |
| DB unwritable | CharacterStore.ok() false → creator still works this session; toast notes "won't persist". |
| Corrupt look JSON on load | Skip that character with a stderr note; others load. |
| Part id in a stored look no longer in the catalog | Look falls back to the first compatible part of that category (logged), never crashes. |
| Spawn spot occupied | Spawn probes a small ring of offsets around the plaza until circleIntersectsAny is clear. |
| Randomize while editing text | Randomize only touches the look, never the persona fields. |

## Open Questions
None — decisions above were made under the autonomy instructions and are
mirrored in OVERNIGHT_REPORT.md.

## Suggested GitHub Issues
1. feat(core): character part catalog with socket contracts — assembly,
   style tags, randomize, tests
2. feat(core): CharacterStore — independent persona/look SQLite records
3. feat(render): composite character renderer under the size contract
4. feat(ui): creator menu page — persona fields, part pickers, live
   preview, randomize
5. feat(game): spawn and persist created characters in the town
