# Plan: Issue #65 — road tiles with lane markings
Date: 2026-07-05 (overnight session 3, autonomous)
Status: READY FOR IMPLEMENTATION
Estimated complexity: S

## Facts from code/assets
- All four road tiles (straight/crossing/junction/corner) are exactly
  2x2x0.1, origin-centered on XZ, base at y=0 (probed from gltf accessors).
- Streets: 16 wide, centered x/z = +-32, city spans +-110 (halfSize).
- Junctions at (+-32, +-32). Tile pitch 16 with centers at multiples of 16
  puts all four junctions exactly on tile centers: t = -96..96 (13 tiles,
  covering +-104), plus a 6-unit compressed end-cap tile at +-107 to reach
  the map edge (Fill contract handles non-square fits).
- Assets currently loads road_straight + road_straight_crossing but NOT
  road_junction — add it to the stems list. road_corner is not needed
  (streets run edge to edge, no L-turns).

## Design
- New file-local helper `drawRoadTile(model, cx, cz, worldX, worldZ,
  rotate90)` in RaylibRenderer.cpp: scales the origin-centered tile in model
  space (swapping axes when rotated) to the requested world footprint,
  thickness pinned to 0.05 (a real 8x scale of the 0.1-thick model would be
  a 0.8-unit curb), base floated 0.01 above the grass plane.
- Replace the asphalt DrawCube strips with the tiled grid; KEEP the strips
  as the fallback when road models are missing (assets-not-downloaded path
  stays functional).
- Zebra crossings (road_straight_crossing) on the four arm tiles adjacent
  to every junction (t = +-16, +-48); junction tile drawn once (vertical
  pass draws it, horizontal pass skips).
- Lane orientation at identity is unknown — screenshot decides the rotate
  flag; flip if lanes run across the street.

## Acceptance
- [ ] Screenshot: lane markings + zebra crossings on all four streets,
      junction art at the four crossings.
- [ ] Suite green (renderer-only change, no City/collision edits).
- [ ] Smoke run clean; tile count ~60 draws total (cheap).
