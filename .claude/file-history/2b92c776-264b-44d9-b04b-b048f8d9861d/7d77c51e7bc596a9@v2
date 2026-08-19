---
description: Conventions and SFML→raylib API mapping for the renderer migration. Read before implementing any raylib-visual-overhaul issue so all eight steps make the same choices.
---

# raylib Migration Skill

Plan: `.claude/plans/raylib-visual-overhaul.md`. Scope guard: `src/core/` and
`tests/` NEVER change in this migration — if a step seems to need a core
change, stop and reconsider (the suite passing unchanged is an acceptance
criterion).

## Ground rules

- raylib comes from CMake FetchContent, pinned to a release tag (block is
  commented in CMakeLists.txt; step 1 enables it and deletes the SFML/OpenGL
  find_packages). Never `brew install raylib` — builds must be reproducible
  from a clean clone.
- One frame = `BeginDrawing` → `renderer.beginFrame/drawCity/drawCharacter/endFrame`
  (3D) → 2D overlay (UI, nameplates) → `EndDrawing`. raylib has no
  pushGLStates/popGLStates dance — 2D after `EndMode3D()` just works.
- Assets load once at startup through `Assets`; nothing loads mid-frame.
  Every accessor has a primitive fallback so a missing `assets/` tree
  degrades gracefully (run `tools/fetch_assets.sh` message, never a crash).
- Keep `KeyBindings`' key *names* and config format unchanged — only
  `InputMap` re-tables name↔code for raylib. Saved bindings must still load.

## SFML → raylib mapping

| Concern | SFML (current) | raylib (target) |
|---------|----------------|-----------------|
| Window | `sf::RenderWindow window(...)` | `InitWindow(w, h, title)` + `SetTargetFPS(60)`; `SetConfigFlags(FLAG_VSYNC_HINT \| FLAG_WINDOW_HIGHDPI \| FLAG_MSAA_4X_HINT)` before |
| Main loop | `while (window.isOpen())` + pollEvent | `while (!WindowShouldClose())` (poll happens in EndDrawing) |
| Key press event | `sf::Event::KeyPressed` | `IsKeyPressed(KEY_*)` per frame — event loop becomes per-frame checks |
| Held key | `sf::Keyboard::isKeyPressed` | `IsKeyDown(KEY_*)` |
| Text entry | `sf::Event::TextEntered` (+ swallowNextTextEntered hack) | `GetCharPressed()` loop each frame; returns 0 when drained. The "swallow the talk key" trick: drain once on dialogue open |
| Mouse look | macOS CGAssociateMouseAndMouseCursorPosition workaround / manual recenter | `DisableCursor()` on entering play, `EnableCursor()` for menus, `GetMouseDelta()` — delete the whole `#ifdef __APPLE__` block |
| Mouse click/pos | `event.mouseButton` / `sf::Mouse::getPosition` | `IsMouseButtonPressed(MOUSE_BUTTON_LEFT)` / `GetMousePosition()` |
| Text draw | `sf::Text` + `sf::Font` | `DrawTextEx(font, text, pos, size, spacing, color)`; measure with `MeasureTextEx` |
| Rect/backdrop | `sf::RectangleShape` | `DrawRectangleRec` / `DrawRectangleRounded` |
| worldToScreen | hand-rolled matrix in Renderer3D | `GetWorldToScreen(pos, camera)` + own behind-camera dot check |
| Timing | `sf::Clock frameClock` | `GetFrameTime()` (already clamped? no — keep the 0.03s clamp) |
| Fonts | `findSystemFont()` path list | bundled font via `LoadFontEx` from `assets/fonts/` (no system lookup in raylib); keep a `GetFontDefault()` fallback |
| Vec conversions | `sf::Vector2f` | `Vector2` / `Vector3` — write tiny `toRl(Vec3)` helpers in RaylibRenderer.cpp; do NOT include raylib.h in core headers |

## Type-collision warning (learned the hard way with NpcPose)

raylib declares plain C names in the global namespace: `Camera3D`, `Model`,
`Font`, `Color`, `Rectangle`, `Vector2/3/4`, `Image`, `Texture2D`, and NO
namespaces. Known hazards in this codebase:

- `llm_npc::Menu` etc. are namespaced — fine.
- Windows headers + raylib both define `Rectangle`/`CloseWindow`/`ShowCursor`
  — on the Windows build, include raylib.h BEFORE winsock/windows headers, or
  isolate raylib includes to src/app/ compilation units only (preferred: core
  networking headers must not be included after raylib.h on Windows without
  checking; NetSocket.hpp pulls winsock2.h).
- Grep before renaming anything: the wire struct is already `NetNpcPose`
  because the renderer owned `NpcPose`.

## Character animation conventions

- Clips resolved by NAME at load time (`ModelAnimation::name`), stored as
  indexes in `Assets` (idle/walk/gesture). Pack clip names differ — resolve
  per pack, don't hardcode indexes.
- Per-entity animation clock advanced by `GetFrameTime()` in
  `drawCharacter`; `walking` switches idle↔walk hard (no blending in v1).
- Facing: models face +Z at identity to match `Npc::facingDeg()` (0 looks
  toward +Z). Rotate `DrawModelEx` by `facingDeg` around Y — verify each
  pack's forward axis once in step 4 and normalize in Assets, not per call.

## Faces / moods

The six-mood face set is baked to 128×128 textures by `FaceTexture::bake`
(port of the legacy `Renderer3D::drawFace`). Brow tilt is REGRESSION-SENSITIVE:
commit 6298566 fixed angry/sad reading swapped — compare against the legacy
renderer side by side before deleting it (that's why Renderer3D.cpp is deleted
in step 8, not step 1).

## Definition of done per step

Every step: game builds AND runs, `make -C tests test` green (96 cases,
untouched), and the step's slice is visible in-game. Steps are the 8 issues in
the plan — do not merge steps.
