# Stylized Character Assets — Conventions (READ FIRST, overnight run)

Plan: `.claude/plans/stylized-character-assets.md`. Scaffold: PartDef.meshName
(live), TODO markers in RaylibRenderer/Assets/fetch_assets.sh, stubs in
tests/test_stylized_parts.cpp (un-skip per step).

## ☠ THE ENGINE IS RAYLIB, NOT GODOT
The originating brief assumed Godot. It is wrong; the correction is logged
in the plan. Do NOT install Godot, do NOT fetch godotshaders.com code, do
NOT create .tscn/.tres/.gdshader files. Raylib equivalents:
- Cel shader: our own GLSL 330 via LoadShaderFromMemory (kFogVertexShader
  pattern in Assets.cpp), banded diffuse COMPOSED with the existing fog
  stage in ONE character shader (same uniforms the fog stage receives).
- Outlines: the #103 inverted-hull technique, mesh version (redraw model
  ~1.05 scaled, near-black, rlSetCullFace(RL_CULL_FACE_FRONT), batch
  drains around state changes).
- Import: raylib LoadModel on GLTF/GLB (the KayKit characters already
  load); "import pipeline" = a documented checklist + measured bounds.

## Non-negotiables (from the brief + standing invariants)
1. Style commitment beats fidelity: simpler/consistent always wins. No
   realistic eyes/skin, no per-character shaders.
2. All THREE consumers switch together: roaming NPCs, creator output,
   player avatar surfaces. Defaults move to the new family; `core`
   composite parts stay loadable behind their pack tag (stored looks must
   keep rendering).
3. Sockets/size contract stay authoritative: mesh parts declare MEASURED
   bounds as localSize; the 1.8u contract scale does the sizing. Pack
   proportions that fight the contract → adjust size specs deliberately
   and log old→new; NEVER hand-scale meshes ad hoc.
4. Aim/facing stay single-source (lookDirection, facingDeg). Any animation
   work that would reintroduce moonwalk desync is rejected, not patched.
5. Unmappable modular pieces: ship what maps, list exceptions in
   OVERNIGHT_REPORT.md. Never force bad attachments; never block the
   ticket on stragglers.

## Animation tiers (the honest risk)
- Tier A (COMMITTED): static socket assembly + the existing procedural
  locomotion (bob/death/gesture-bob). Nothing regresses.
- Tier B (attempt AFTER Tier A is green, timeboxed): pack-rigged
  idle/walk/run via UpdateModelAnimation with head/hair bone-attached by
  sampling the animated pose. Doesn't land cleanly → Tier A stands,
  exception logged. Mixamo only if the pack lacks essential clips — it is
  free-but-NOT-CC0 and gets its own LICENSES.md entry.

## Acquisition & provenance
Quaternius Ultimate Modular Characters (CC0), fetched by
tools/fetch_assets.sh with a PINNED immutable archive URL + sha256 (the
KayKit pattern in that script). Unpinnable/unfetchable → fallback to the
KayKit character packs ALREADY ON DISK (assets/models/characters/), noted
as a substitution. Either way: pack name/version/license appended to
assets/LICENSES.md. CC0 needs no attribution; the record is for
auditability.

## Faces
Flat textures on quaternius heads (head-ORIENTED quad at the face socket,
not a camera billboard). FaceTexture's generator grows a stylized set
(~8 faces × mood variants); creator Eyes/Mouth picks become face picks for
this family; the floating emote billboard is RETIRED for this family
(mood = face variant) — fixes session-7 finding #1. Core-family heads keep
their primitive eyes/mouths and billboard.

## Verification recipe (unchanged)
`caffeinate -u -t 3` then `./build/cpp_game_with_llm_npcs --frames 90
shot.png --camera x z yaw --hour 12` (PNG lands in CWD). Suite:
`make -C tests test`, un-skip stubs in the implementing commit. Final gate:
visual-qa side-by-side (created character + roaming NPC + player avatar),
judged for cross-consumer consistency AND no-uncanny. Draft PR into dev;
never merge; never touch main.
