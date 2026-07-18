# Plan: CC0 Stylized Character Assets — One Committed Cartoon Style
Date: 2026-07-14 (autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: XL

## ⚠ Premise correction (logged per the autonomy rules, decided, not asked)
The brief says "Godot's renderer is well-suited to this target; no engine
change is involved" and names Godot-specific solutions (eldskald's Godot 4
cel shader, Godot GLTF import). **This game runs on raylib** — settled by
the session-7 engine assessment ("no migration") and every renderer system
since (fog shader, inverted-hull outlines, composite recipes are raylib
GLSL/rlgl). The brief's goals are engine-agnostic and all have direct
raylib equivalents, several half-built already:
| Brief says (Godot) | This plan does (raylib) |
|---|---|
| eldskald's Godot 4 cel shader (MIT) | Our own banded cel-lighting GLSL 330, composed WITH the existing fog stage in one character shader (LoadShaderFromMemory, the fog-shader pattern) — no third-party license needed |
| Godot GLTF import pipeline | raylib LoadModel GLTF (the KayKit characters already load this way); a documented import checklist instead of editor import settings |
| Godot inverted-hull outlines | The mesh version of the inverted-hull pass already shipped for composites (#103): redraw the model scaled ~1.05, near-black, front-cull |

## The Idea (one paragraph)
Replace the "weird/scary" primitive-composite look by swapping in real CC0
stylized modular character meshes (Quaternius Ultimate Modular Characters;
fallback: the KayKit character packs already on disk), wired through the
EXISTING socket/size-contract/pack systems as a new part family — heads,
bodies, hair as meshes instead of primitive recipes — with faces as flat
swappable textures on the head (Mii/Animal Crossing style), a banded toon
shader + outlines on every character material, and all three consumers
(roaming NPCs, creator output, player avatar surfaces) switched together
in one pass. The uncanny mismatch is fixed by committing to stylization
everywhere at once; the graphics-pack seam built in #101 finally carries a
real second pack.

## Goal
Every character in the game — townsfolk, created NPCs, the player's own
avatar — renders from one consistent, friendly, cel-shaded stylized asset
family, and nobody reads as uncanny.

## Out of Scope (this version)
- Engine changes of any kind (premise correction above).
- Realistic eyes/skin/per-character shaders (brief's own rule: style
  commitment beats fidelity).
- Deleting the "core" composite parts — they stay behind their pack tag,
  no longer default anywhere.
- Cloth/physics, LODs, facial bone animation (faces are flat textures by
  design).
- Mixamo retargeting UNLESS the chosen pack lacks essential clips (it's
  free-but-not-CC0; if used it gets its own credits entry — but see the
  animation tiers below; the base plan avoids it).

## The honest risk: modular meshes + skeletal animation in raylib
Assembling a character from separate part MESHES and animating them with
one skeleton means sampling bone matrices per frame and attaching parts to
bones — real engineering in raylib (the current rigged path animates only
WHOLE models). Two-tier commitment, per the brief's "integrate what maps
cleanly, list exceptions":
- **Tier A (committed)**: modular parts assembled STATICALLY through the
  existing socket system (positions from sockets, one contract scale) with
  the current procedural locomotion (walk bob, tip-over death, gestures →
  bob) that already animates every composite character today. Everything
  ships: new meshes, faces, cel shader, outlines, unified rollout.
- **Tier B (attempted after Tier A is green)**: locomotion from the pack's
  rigged base body (idle/walk/run clips via UpdateModelAnimation, the
  KayKit path) with head/hair parts attached to the head bone by sampling
  the animated pose. If bone attachment doesn't land cleanly in the time
  budget, Tier A stands and the exception is listed in the report — the
  game keeps the bob it has NOW, so nothing regresses.
- Aim/facing stay derived from the single-source values (lookDirection,
  facingDeg) in both tiers — retargeting never reintroduces moonwalk
  desync because movement math is untouched.

## Affected Areas
- `tools/fetch_assets.sh` — add the Quaternius pack as a pinned, sha256'd
  archive URL (the existing KayKit pattern); fallback logic prints which
  pack was used. `assets/LICENSES.md` gains the provenance record (pack
  name/version/CC0; Mixamo separately if ever used).
- `src/core/CharacterParts.hpp/.cpp` — PartDef gains `meshName` (empty =
  primitive recipe, the existing behavior); a new `"quaternius"` part
  family: bodies/heads/hair mapped from the pack's modular pieces, sockets
  authored per part against measured mesh bounds, sizes conforming to the
  1.8u contract via the size-spec route (logged values, no ad-hoc mesh
  scaling). New: `activePack()` / default-pool filtering so the creator,
  `randomizeLook`, and `lookForPersona` draw from the new family by
  default while `core` parts stay loadable (stored looks keep working).
- `src/app/Assets.cpp` — loads the part meshes; routes EVERY character
  material through the new cel shader (assert: no character draws with the
  default lit material).
- `src/app/RaylibRenderer.cpp` — recipe dispatch gains the mesh branch
  (draw meshName at socket, contract-scaled) — the #101 seam doing its
  job; mesh inverted-hull outlines; the flat-face quad: an oriented
  (head-attached, NOT camera-billboard) textured quad at the face socket.
- `src/core/FaceTexture.{hpp,cpp}` — the procedural face generator grows a
  starter set of ~8 stylized face textures (matching the pack's palette);
  creator "Eyes"+"Mouth" picks become face-texture picks for quaternius-
  family heads (core-family heads keep their primitive eyes/mouths).
  Mood expressions reuse the same generator (angry/happy variants of the
  chosen face replace the floating emote billboard for the new family —
  fixing session-7 finding #1 as a side effect; logged).
- `src/app/main.cpp` — default pack switch; avatar arm/hand tint keeps
  working (palette colors still apply as tints over the cel shader).
- Tests: catalog well-formedness (mesh parts declare sockets/bounds),
  pack-filter tests (default pool = quaternius; core still resolvable),
  look back-compat (stored core looks still valid), proportion window
  re-checked for the new family.

## Implementation Order
1. **Acquisition + provenance** — fetch script entry (pinned/sha256),
   LICENSES.md record, import checklist doc; fallback = KayKit characters
   already on disk, noted if used. *Committable.*
2. **Cel shader + mesh outlines** — banded lighting composed with fog in
   one character shader; applied to the EXISTING rigged models first as
   the shader testbed (screenshot before any modular work). *Committable.*
3. **Mesh part family** — PartDef.meshName + mesh recipe branch + the
   quaternius catalog rows (measured bounds → sockets/sizes, logged);
   creator/randomizer/fallback default to the new pack; core stays
   loadable. Exhaustive combo test covers the new family. *Committable.*
4. **Flat faces** — face-quad rendering + starter texture set + creator
   integration + mood variants replacing emote billboards for the new
   family. *Committable.*
5. **Unified rollout** — NPCs/creator/avatar all on the new family (the
   pool default from step 3 does most of it; persona looks re-authored to
   quaternius equivalents; old looks demote gracefully). *Committable.*
6. **Tier B animation attempt** — rigged locomotion + bone-attached parts;
   timeboxed; exception-logged if it doesn't land. *Committable or
   cleanly absent.*
7. **Visual QA gate (per the brief)** — side-by-side screenshot: created
   character + roaming NPC + player avatar with new assets, cel shader,
   outlines; judged for (a) cross-consumer consistency and (b) nothing
   uncanny (no realism islands). Report + paths in OVERNIGHT_REPORT.md.

## Acceptance Criteria
- [ ] Pack fetched pinned+checksummed (or KayKit fallback, noted);
      provenance in LICENSES.md.
- [ ] No character renders with the default lit material — every
      character material goes through the cel shader (code-level assert
      or test on the material routing).
- [ ] Quaternius parts assemble through the EXISTING socket/contract
      systems; new-family looks pass the combo test; proportion window
      holds or its new values are logged.
- [ ] Faces are flat textures on quaternius heads; the creator picks
      them; moods show as face variants, not floating billboards (new
      family).
- [ ] Default pool (creator, randomize, persona fallback) = the new
      family; stored core looks still load and render.
- [ ] Aim/facing single-source invariants untouched (existing aim tests
      stay green — they are the guard).
- [ ] Visual-qa verdict PASS on consistency + no-uncanny, report filed.
- [ ] `make -C tests test` green; game builds; screenshots at each step.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Quaternius archive unfetchable/unpinnable | KayKit character pack fallback (already on disk), substitution logged in report + LICENSES.md. |
| A modular piece has no clean socket mapping | Ship the pieces that map; list exceptions in the report (brief's rule). |
| Pack proportions fight the 1.8u contract | Adjust size specs deliberately, log old→new values; never hand-scale meshes ad hoc. |
| Stored look references a core part | Loads and renders exactly as today (core recipes remain); only DEFAULTS change. |
| Mesh lacks proper normals for hull outlines | Per-part outline opt-out (the #103 escalation path), logged. |
| Tier B bone attachment jitters/desyncs | Revert to Tier A locomotion for that part/family; exception logged; no moonwalk regressions permitted. |
| Cel shader fights the day/night fog | They compose in ONE shader (bands then fog mix) — same uniforms the fog stage already receives. |

## Open Questions
None — the engine premise is corrected above with the mapping table; the
animation risk is tiered with a non-regressing fallback; everything else
follows the brief's own "simpler/consistent wins" rule.

## Suggested GitHub Issues
1. **chore(assets): fetch Quaternius modular pack, pinned + provenance** — acquisition, LICENSES.md, import checklist. (Concept: *reproducible asset provenance*.)
2. **feat(render): banded cel shader composed with fog + mesh outlines** — the one character shader. (Concept: *toon lighting bands in GLSL*.)
3. **feat(parts): mesh part family through the pack seam** — PartDef.meshName, quaternius rows, default-pool switch. (Concept: *a second content pack proving the seam*.)
4. **feat(faces): flat texture faces + creator picks + mood variants** — kills the emote billboard for the new family. (Concept: *decal faces vs geometric faces*.)
5. **feat(characters): unified rollout across NPCs/creator/avatar** — re-authored persona looks, back-compat demotion. (Concept: *default migration with legacy loadability*.)
6. **feat(anim): Tier-B rigged locomotion with bone-attached parts (timeboxed)** — exception-logged if unclean. (Concept: *bone-socket attachment in raylib*.)
7. **chore(qa): three-consumer side-by-side gate** — consistency + no-uncanny verdict. (Concept: *style-consistency review criteria*.)
