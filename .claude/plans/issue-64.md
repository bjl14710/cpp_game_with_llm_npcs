# Plan: Issue #64 — proper plaza fountain from primitives
Date: 2026-07-05 (overnight session 3, autonomous)
Status: READY FOR IMPLEMENTATION
Estimated complexity: S

## The Idea
The "fountain" obstacle (8x8 AABB at (60..68, 56..64) in the park, authored
in City with collision) currently renders as the flat KayKit "base" tile — a
pancake. The pack has no fountain model, so build one from raylib primitives:
tiered stone cylinders plus a translucent blue water disc, dispatched by id
in the buildings pass.

## Key constraint discovered in code
`Assets::modelForBuilding` falls back to a HASHED GENERIC BUILDING for any id
without a curated entry — so simply dropping the "fountain"->"base" mapping
would render a random building there. The fountain dispatch must happen
BEFORE the model lookup in `RaylibRenderer::drawCity`.

## Sizing (decision logged)
The existing SizeSpec entry {"fountain", Uniform 1.2f} remains the single
sizing source per the issue: drawFountain reads `assets_.sizeSpecFor(b)
.worldHeight` for its total height and derives radii from the authored AABB
footprint (radius = 0.5 * footprint width * tier fraction). No new constants
outside the tier-proportion table.

## Changes
- `src/app/Assets.cpp`: remove `{"fountain", "base"}` from curated_ (SizeSpec
  entry stays — it is the height source).
- `src/app/RaylibRenderer.cpp`: in drawCity's buildings pass, `if (b.id ==
  "fountain") { drawFountain(b); continue; }`; new file-local helper
  `drawFountain(const Building&, float worldHeight)`: 3 stacked stone
  cylinders (basin wall, middle tier, top spout) + translucent blue water
  discs (alpha ~150) in the basin and middle tier. DrawCylinder only.

## Acceptance criteria
- [ ] Screenshot from the park side shows a tiered fountain with visible
      water surface at (64, 60).
- [ ] Collision unchanged: test_city.cpp green (no City change at all).
- [ ] Suite green (127/782 baseline); build + smoke clean.

## Out of scope
Water animation, particles, shaders (v1 explicitly primitives-only).
