# Mii-Style Rendering & Content-Pack Conventions

Read this before implementing any mii-style-visual-overhaul issue so every
step makes the same choices. Plan: `.claude/plans/mii-style-visual-overhaul.md`.

## The three invariants

1. **One shared pool.** NPCs, the creator, and the player avatar all draw
   from `partCatalog()`/`paletteCatalog()` via `drawCompositeCharacter`.
   Never add a parts source or a renderer variant for one consumer — that
   reintroduces the pool-separation bug PR #99 removed.
2. **Style is data.** Proportions = `localSize`/sockets on catalog rows.
   Colors = palette rows. Pack membership = the `pack` tag. If a change
   requires editing assembly code (`assembleLook`) or picker logic to look
   different, the design is wrong — stop and re-read the plan.
3. **Contracts hold.** Sockets resolve unscaled, ONE uniform scale to the
   1.8u height contract afterward; style tags (round/blocky/any) gate every
   combination; `hair_none`-style explicit zero-size parts instead of
   missing slots.

## Graphics-pack seam (step 1)

A pack is: catalog rows (`pack: "tag"`) + palettes (`pack: "tag"`) +
renderer recipes for its part ids. Nothing else. The recipe dispatch in
`RaylibRenderer.cpp` must be a single function with signature roughly
`drawPartRecipe(const PartDef&, Vec3 at, Vec3 dim, const Colors&)` so a
future pack extends one dispatch site. The generic declared-box fallback
stays the safety net for any part without a bespoke recipe.

## Inverted-hull outlines (step 3)

- Re-issue the same part draws FIRST, scaled ~1.05 about each part's
  center, in near-black (`{30, 30, 36}` family), with
  `rlSetCullFace(RL_CULL_FACE_FRONT)`; restore `RL_CULL_FACE_BACK` after.
- `rlgl` state calls flush the batch — group the outline pass, then the
  color pass, not interleaved per part.
- Boxes are the known risk (corner gaps). Escalation order: bump hull
  scale → per-recipe outline opt-out flag → (logged, last resort) skip
  outlines on box parts only. Never abandon outlines globally without a
  screenshot comparison in OVERNIGHT_REPORT.md.
- Screenshot-verify on BOTH a sphere-family and box-family character.

## Mii proportion targets (step 2)

Head ≈ 40–45% of standing height (Tomodachi reference). Bodies shorten to
~0.75–0.85u, heads grow to ~0.95–1.05u, sockets re-authored per part.
Yardstick: the cop close-up shot from PR #99
(`docs/qa/screenshots/shared-character-library/qa_shot_cop.png`) is the
"before"; the "after" head should read roughly twice as dominant.

## Mouth category + look format (step 4)

Land as ONE commit: enum + count bump, CategorySpec row (`Mouth`→`Head`
via `"mouth"`), `mouth` sockets on all heads, ≥4 mouth parts + recipes,
Menu `categoryLabel` case. Look format: `look = body, head, eyes, hair,
mouth, palette` (six items); the five-item form MUST keep parsing (mouth
via the deterministic fallback) so shipped persona files and stored looks
never break. `renderPersonaText` always emits six.

## Player avatar (step 6)

Reserved id `player_avatar` in the existing CharacterStore look table — no
new storage. The creator page runs in "avatar mode" (same picker/Randomize/
preview; Save writes the avatar row instead of spawning). First-person
visibility: tint the punch sleeve/fist and pistol hand from the avatar's
palette in `drawViewmodel`. Remote-player replication is OUT of scope
(logged seam; guests still see rigged meshes).

## Verification recipe

`caffeinate -u -t 3` then
`./build/cpp_game_with_llm_npcs --frames 90 shot.png --camera <x> <z> <yaw> --hour 12`
— the PNG lands in CWD regardless of path given. Read the PNG. Suite:
`make -C tests test` (un-skip the matching stub in `tests/test_style_pack.cpp`
in the same commit as each step).
