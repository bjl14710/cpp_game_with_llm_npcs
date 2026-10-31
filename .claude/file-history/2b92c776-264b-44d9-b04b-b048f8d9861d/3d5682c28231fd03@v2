# Plan: raylib Renderer Swap + Low-Poly Visual Overhaul
Date: 2026-07-05
Status: READY FOR IMPLEMENTATION
Estimated complexity: XL

## The Idea (one paragraph)
The game currently draws its city as flat-colored boxes through a hand-rolled
~570-line legacy OpenGL 2.1 renderer sitting on SFML. This plan replaces that
entire presentation layer with [raylib](https://www.raylib.com) (simple,
C-style, modern-3D-capable, actively maintained) and restyles the whole game
with professionally-made CC0 low-poly asset packs — real buildings, streets,
props, and rigged characters instead of boxes. The engine-agnostic heart of the
game (`src/core/`: LLM, NPCs, world, dialogue, multiplayer — 96 unit tests)
survives byte-for-byte untouched; only the `src/app/` shell (~1,400 lines) is
rewritten. Full feature parity before merge: dialogue with streaming text,
menus and rebinding, mood expressions, arrests, and multiplayer host/join all
work in the new renderer.

## Goal
A player launches the same game and walks a genuinely good-looking low-poly
city — modeled buildings, dressed streets, animated characters with visible
moods — with every existing feature (talking, instructing, multiplayer)
working exactly as before.

## Out of Scope (this version)
- Godot/Unreal or any full engine migration (decided: raylib library swap).
- Custom-authored art, character customization, or paid asset packs — CC0 only.
- Gameplay changes of any kind: no new NPC abilities, no new world size,
  no new multiplayer features. Same game, new body.
- Advanced rendering: no PBR, no dynamic shadows beyond raylib's basics, no
  post-processing stack in v1 (fog + tinted lighting is the v1 mood budget).
- Skeletal animation *blending*; v1 uses each model's baked idle/walk clips
  switched hard (walk when moving, idle otherwise).
- Touching `src/core/` beyond what compiles (it should not change at all).

## Decisions Already Made
- **raylib via CMake FetchContent**, pinned to a release tag — reproducible on
  macOS/Linux/Windows, no system install needed, and the sfml@2 keg-only
  headache disappears with SFML itself. (raylib replaces SFML for windowing,
  input, text, and 2D overlay too — SFML is fully removed.)
- **Asset sources (all CC0 / public domain, commercial-safe, no attribution
  required — verify license text when downloading):**
  - Kenney — *City Kit (Commercial/Suburban/Roads)*, kenney.nl
  - Kay Lousberg — *KayKit City Builder Bits + Character packs*, kaylousberg.com
  - Quaternius — *Ultimate Modular Buildings, Animated Characters*, quaternius.com
  - Format: glTF (`.glb`) preferred — raylib loads it natively with animations.
- **Moods on 3D characters**: keep the game's signature expressive faces by
  rendering the existing procedural face (brows/eyes/mouth by `NpcMood`) onto a
  small texture and mapping it to the character head's face material — the
  low-poly packs use flat face textures, so a 128×128 generated face texture
  per mood slots right in. Fallback if a pack's UVs fight us: a subtle
  floating emote billboard above the head.
- **Base branch**: this work starts *after* the multiplayer PR stack (#28-#34)
  merges — full parity includes multiplayer rendering, and both rewrite
  `src/app/main.cpp`. Merging order: multiplayer first, then this.

## Affected Areas
- `src/app/Renderer3D.{hpp,cpp}` — **deleted**, replaced by:
  - `src/app/RaylibRenderer.{hpp,cpp}` — camera, city scene, characters,
    nameplates (raylib `DrawBillboard`/`GetWorldToScreen`), sky/fog/lighting.
  - `src/app/Assets.{hpp,cpp}` — loads `.glb` models/animations + generated
    face textures once; maps building `facadeKind`/`spotId` and NPC index →
    model + tint (same stable-index trick the current renderer uses).
  - `src/app/FaceTexture.{hpp,cpp}` — ports the existing procedural
    brow/eye/mouth drawing (currently GL immediate mode) to a raylib
    `RenderTexture2D` per `NpcMood`.
- `src/app/main.cpp` — same loop and modes, but raylib window/input/timing
  replaces SFML events (`WindowShouldClose`, `IsKeyPressed`, `GetMouseDelta` —
  which also deletes the macOS cursor-disassociation workaround, raylib's
  relative mouse handles it).
- `src/app/Menu.{hpp,cpp}`, `src/app/DialogUI.{hpp,cpp}`, `src/app/InputMap.{hpp,cpp}` —
  same logic/layout, SFML draw calls → raylib (`DrawRectangleRounded`,
  `DrawTextEx`, `MeasureTextEx`); InputMap re-tables key names to raylib codes
  (KeyBindings' names/config format unchanged, so saved bindings still load).
- `CMakeLists.txt` — FetchContent raylib; drop SFML/OpenGL find_packages;
  bundle a font (raylib has no system-font lookup — pick one CC0 font, e.g.
  Kenney's, into `assets/fonts/`) replacing `findSystemFont()`.
- `run.sh` / `run.ps1` / `run.bat` — remove the sfml@2 CMAKE_PREFIX_PATH logic.
- New `assets/` tree (`models/`, `fonts/`) + `assets/LICENSES.md` recording
  each pack's origin and CC0 statement.
- New `tools/fetch_assets.sh` — downloads/unzips the pinned asset packs so the
  repo doesn't carry 100+ MB of binaries (assets gitignored; script committed).
- `docs/DEVELOPER.md`, `README.md` — build instructions, asset workflow.
- **Untouched**: everything in `src/core/`, `tests/` (suite must stay green),
  `personas/`, `config/`.

## Implementation Order
1. **Build skeleton** — CMake FetchContent raylib (pinned tag), empty raylib
   window opens alongside... rather, on a branch: SFML removed, app builds and
   opens a raylib window rendering nothing. `make -C tests test` still green
   (proves core untouched). Launch scripts updated.
2. **Input + modes** — InputMap→raylib keys, mouse look, WASD collision walk,
   AppMode machine, menu/dialogue mode switching (UI still ugly/plain text).
3. **City scene** — `Assets` + `RaylibRenderer`: ground, roads, asset-pack
   buildings mapped by facadeKind/spotId, props, sky + fog + light tint.
   The city reads as a real place at this step's end.
4. **Characters** — animated glTF characters per NPC (stable index → model
   variant), idle/walk clip switching driven by position deltas, gestures
   (wave/raise-hand approximated with the packs' clips), remote-player avatars.
5. **Faces/moods** — `FaceTexture` renders the six `NpcMood` faces; applied to
   character heads (or emote billboard fallback). Brow-tilt regression from
   commit 6298566 visually re-verified.
6. **UI parity** — DialogUI (streaming transcript, input line), Menu (all
   pages incl. Multiplayer host/join text entry), nameplates, HUD prompts,
   jail countdown, crosshair.
7. **Multiplayer parity pass** — host/join between two instances; replicated
   NPC poses/moods render through the same Assets path; snapshot-driven
   remote players use character models.
8. **Polish + cleanup** — delete Renderer3D and all SFML remnants, fog/palette
   tuning, `assets/LICENSES.md`, docs, screenshots in README.

Each step is one commit/PR-sized unit; the game runs at every step.

## Acceptance Criteria
- [ ] `make -C tests test` passes unchanged (96 cases) — `src/core/` untouched.
- [ ] `cmake --build build` succeeds on macOS with no SFML installed at all.
- [ ] Given a fresh clone + `tools/fetch_assets.sh`, the game builds and shows
      the asset-pack city (no hand-modeled boxes anywhere).
- [ ] All 10 NPCs appear as animated low-poly characters at their spots, with
      nameplates, and walk animations when following/chasing.
- [ ] Talking works end-to-end: prompt, streaming reply, directives
      (follow/wave/arrest) visibly obeyed, mood change visible on the face.
- [ ] Menu parity: rebind a key (persists), switch pages, host a game, join
      `127.0.0.1:40605` from a second instance, see both avatars + shared NPC
      mood update — all in the raylib build.
- [ ] Jail flow: arrest teleports to the station with countdown HUD.
- [ ] 60 fps on the dev Mac in the plaza with all NPCs visible (raylib default
      vsync; no long frame hitches from asset loads mid-game — everything
      preloads).
- [ ] `assets/LICENSES.md` lists every pack with its CC0 statement.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| `assets/` missing (script not run) | Game starts with a clear on-screen + stderr message naming `tools/fetch_assets.sh`; falls back to colored-box primitives rather than crashing. |
| A model fails to load / bad glb | Log the path, substitute the fallback primitive for that entity only. |
| Asset pack URL rots | fetch script pins versions and checks hashes; failure prints the manual-download URL. |
| Font missing | Bundled in `assets/fonts/` and fetched with the packs; same fallback message path. |
| Window resize / HiDPI | raylib handles DPI scaling (`FLAG_WINDOW_HIGHDPI`); UI layout already derives from window size. |
| Old saved keybindings | Names are portable (`KeyBindings` format unchanged); unknown names fall back to defaults exactly as today. |
| Multiplayer between old (SFML) and new builds | Protocol version is unchanged so they *would* connect — bump `kNetProtocolVersion` only if the wire changes; otherwise fine since visuals are client-local. |

## Open Questions
- Which specific pack per city zone (Kenney vs KayKit vs Quaternius have
  different silhouettes) — decide in step 3 by dropping candidates into the
  scene and eyeballing; the plan deliberately doesn't pre-commit.
- Face-texture UV mapping varies per character pack — step 5 confirms
  texture-swap vs emote-billboard per pack actually chosen.
- Whether `run.command`'s launch flow needs Gatekeeper notes once SFML is gone
  (likely simpler, verify on first run).

## Suggested GitHub Issues
1. `build(raylib): replace SFML with FetchContent raylib and a blank window` — step 1
2. `feat(app): input, camera, and mode machine on raylib` — step 2
3. `feat(render): low-poly city scene from CC0 asset packs` — step 3 (+ fetch script + LICENSES)
4. `feat(render): animated glTF characters for NPCs and players` — step 4
5. `feat(render): mood face textures on character models` — step 5
6. `feat(ui): DialogUI, Menu, and HUD parity in raylib` — step 6
7. `test(mp): multiplayer parity pass on the raylib build` — step 7 (manual verify checklist)
8. `chore(render): delete legacy GL renderer and SFML remnants; polish + docs` — step 8
