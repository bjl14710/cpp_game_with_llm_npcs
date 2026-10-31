# First-Person 3D City with 10 LLM NPCs (cpp_game_with_llm_npcs)

## Context

The repo currently has a working single-NPC Ollama chat demo (async `LlmClient` worker thread, `Persona`, `Npc`, SFML `DialogUI`). The user wants it turned into a first-person 3D city (style of `cpp_shooter_game`): walk with WASD + mouse-look, meet **10 NPCs** (baker, cop, taxi driver, barista, librarian, musician, hot-dog vendor, hardware store owner, tourist, retired teacher), press **T** near one to chat, Esc menu with **mouse-driven key rebinding**, unit tests run constantly, **small (~2-3 file) commits pushed after each** to `initial_npcs_and_world` (remote `bjl14710/cpp_game_with_llm_npcs`), player + developer docs, `run.bat` launcher, low latency. Priority: characters & communication first, world second, menu third, docs throughout.

User decisions: first-person 3D; install Ollama rootless in this container for end-to-end tests; default model stays `qwen2.5:3b-instruct`; push to current branch.

**Step 0 (before coding):** save the user's original prompt to `/home/node/work/normal_work/cpp_games/NPC_CITY_PROMPT.txt` (outside any git repo) so work is resumable after interruption/compaction.

## Environment facts (verified)

- Container: `g++`, `make`, `git`, `curl` present; **no cmake, no SFML, no GL headers, headless**. 823 GB free disk, 10 cores.
- httplib v0.15.3 (vendored): no streaming `Post()` overload — client streaming must use `httplib::Request` with `content_receiver` + `Client::send()` (httplib.h:1242). Fake server streaming via `Response::set_chunked_content_provider`.
- Shooter donor code (`cpp_shooter_game/src/main.cpp`): Vec3 math 36-66; textures 95-152; `setupPerspective`/`setupLookAt`/`drawTexturedQuad/Box/Cylinder` 158-264; `initOpenGlState` 361-386; WASD 517-526; `handleMouseLook` 533-544; dt clamp `std::min(0.03f, …)` 1093; GL context setup 1053-1075. NOTE: its `renderHud` (915-1012) shows the correct `pushGLStates/popGLStates` SFML-over-GL pattern but is dead code — we must wire it for real.

## Architecture: strict core/app split

**`src/core/`** — SFML-free, GL-free; compiles with plain g++ -pthread; ALL unit tests target this.
- Moved: `LlmClient.hpp/.cpp` (then extended), `Persona.hpp`, `Npc.hpp/.cpp` (+ `Vec3 pos`, `facingDeg`, `spotId`).
- New: `Math.hpp` (Vec3 ops, adapted from shooter, no SFML); `StreamAssembler.hpp` (NDJSON reassembly + `parseOllamaChunk`); `Config.hpp/.cpp` (key=value reader extracted from main.cpp:22-67); `PersonaLoader.hpp/.cpp` (parses `personas/*.persona`); `City.hpp/.cpp` (`Building` AABBs, `resolveMovement` axis-separated slide, `makeDowntown()` 3×3 blocks, 220×220 world); `World.hpp/.cpp` (`nearestNpcWithin(pos, radius)`); `KeyBindings.hpp/.cpp` (Action enum, portable key-name strings, load/save, conflict=swap); `DialogueSession.hpp/.cpp` (Roaming→Talking→WaitingReply→Streaming state machine, id-routed deltas).

**`src/app/`** — SFML+OpenGL, built only where graphics libs exist (Windows; CMake makes game target conditional on `SFML_FOUND AND OPENGL_FOUND`).
- `main.cpp` (rewrite of current src/main.cpp; `AppMode {Playing, Dialogue, Menu}`); `Renderer3D.hpp/.cpp` (adapted shooter functions; first-person camera eye=pos+1.7y, pitch ±75°, fov 70; city + NPC cylinder figures + `worldToScreen` for SFML nameplates); `DialogUI.hpp/.cpp` (moved; add `beginStreaming/setStreamingText/endStreaming`, `swallowNextTextEntered()` so the 't' keystroke doesn't leak into the input box); `InputMap.hpp/.cpp` (key name ↔ `sf::Keyboard::Key`); `Menu.hpp/.cpp` (Resume/Controls/Quit, mouse hit-testing via `FloatRect::contains`, rebind chips with "press a key…", swap toast, immediate save).

**Tests** — vendored `external/doctest.h`; `tests/Makefile` is the **primary container build** (g++ -std=c++17 -pthread, explicit source list); CMake `enable_testing()` path maintained for Windows. `tests/FakeOllama.hpp` = in-process httplib::Server on `bind_to_any_port` with canned streaming/error responses. Live-Ollama tests gated by `OLLAMA_LIVE=1` env.

**Frame order**: GL 3D pass → `pushGLStates()` → SFML overlay (nameplates ≤30u, hint, dialogue or menu) → `popGLStates()` → `display()`.

## Streaming LlmClient (latency)

- Body gains `"stream": true`, `"keep_alive": config.keepAlive` (new `keep_alive = 10m` in `config/llm.cfg`).
- `processOne`: `httplib::Request` with `response_handler` (capture status) + `content_receiver` feeding `StreamAssembler`; per chunk push `ChatDelta{id, text}` (guarded by reply mutex); accumulate full text; map errors (connect fail / non-200 body / `{"error":…}` line / stream ends without `done:true`).
- New API: `drainDeltas()` (non-blocking, like `drainReplies()`), `warmUp()` (internal empty-messages request that preloads the model; reply discarded via `ChatRequest::internal` flag).
- Main loop each frame: `drainDeltas()` → route by id to session/UI; `drainReplies()` → `Npc::onReplyArrived` (history only on success). Deltas with stale ids dropped; `pendingRoutes` (requestId→npcIndex) lands late replies in history even if the overlay was closed.

## Persona file format + roster

`personas/<id>.persona`: key=value header (`name, role, traits, style, knowledge, spot, position = x, z`, `facing`) then `---` and freeform extra directives. Loader returns persona + world placement. Delete `personas/companion.txt` when the 10 files land.

Roster: Marge Holloway (baker), Officer Dana Brooks (cop), Ray Okafor (taxi), Theo Park (barista), Ms. Adaeze Obi (librarian), Benny "Strings" Malone (musician), Gus Romano (hot-dog cart), Hal Jensen (hardware), Yuki Tanaka (tourist), Mr. Whitfield (retired teacher). Personas cross-reference city geography.

## Commit sequence (each pushed to `initial_npcs_and_world`; gate = green before push)

1. `build(tests): vendor doctest and add make-based test runner` — external/doctest.h, tests/test_main.cpp, tests/Makefile. Gate: `make -C tests test` exit 0.
2. `refactor(core): move LLM client and persona into src/core` — git mv ×3 + CMakeLists include dir.
3. `refactor(core): move Npc into src/core and extract core static library` — git mv ×2 + CMakeLists (`llm_npc_core` lib).
4. `feat(core): extract config reader from main` — Config.hpp/.cpp, main.cpp shrinks.
5. `feat(core): persona file loader with position metadata` — PersonaLoader + test_persona.cpp.
6. `feat(personas): ten city character files` — 10 .persona files, delete companion.txt; loader test finds 10.
7. `feat(core): NDJSON stream assembler for Ollama chunks` — StreamAssembler.hpp + test_stream.cpp (split-mid-line/done/error).
8. `feat(core): streaming LlmClient with keep_alive and warm-up` — LlmClient.hpp/.cpp + config/llm.cfg.
9. `test(core): fake Ollama server covering protocol and errors` — FakeOllama.hpp, test_llm_client.cpp, test_npc_history.cpp (request shape, streamed reassembly, HTTP 500, unreachable port, history trim).
10. `feat(core): city layout with AABB collision resolution` — Math.hpp, City.hpp/.cpp.
11. `test(core): collision and movement-slide coverage` — test_city.cpp (corner/slide/bounds).
12. `feat(core): NPC world positions and proximity query` — Npc.hpp, World.hpp/.cpp.
13. `test(core): proximity selection tests` — test_world.cpp.
14. `feat(core): rebindable key map with persistence and conflict swap` — KeyBindings + test_keybindings.cpp.
15. `feat(core): dialogue session state machine` — DialogueSession + test_dialogue_session.cpp.
16. `feat(app): 3D renderer with city, NPC figures, and projection` — Renderer3D.hpp/.cpp. (Windows visual check.)
17. `feat(app): first-person main loop with movement and talk hint` — git mv main.cpp → src/app/ (rewrite), InputMap, CMakeLists (SFML/OpenGL optional, game conditional).
18. `feat(app): streaming dialogue overlay` — DialogUI moved+extended, main.cpp wiring.
19. `feat(app): pause menu with mouse-driven key rebinding` — Menu.hpp/.cpp, main.cpp, config/keybindings.cfg.
20. `build(windows): run.bat launcher` — mirrors run.ps1 (msys64 check, PATH, Ollama curl check, cmake MinGW, run exe).
21. `docs: player README, controls reference, developer guide` — README.md rewrite, docs/CONTROLS.md, docs/DEVELOPER.md.

After commit 9: rootless Ollama install — `curl -L https://ollama.com/download/ollama-linux-amd64.tgz | tar -xz -C ~/.local/ollama`, `ollama serve &`, `pull qwen2.5:3b-instruct` (fallback 1.5b / llama3.2:1b; tests read `OLLAMA_TEST_MODEL`), then `OLLAMA_LIVE=1` live suite verifying incremental deltas + warm-up latency.

## Verification

- Container, every core commit: `make -C tests test` (offline; fake server).
- Container, post-#9: live Ollama streaming test as above.
- **Windows-only (user verifies; flag explicitly when delivering 16-20):** city renders + collision + hint (16/17); T opens overlay with live word streaming, Esc returns, mid-game Ollama-down error path (18); rebind + swap toast + persistence across restart (19); run.bat from Explorer (20).

## Key bindings config (`config/keybindings.cfg`)

`move_forward=W, move_backward=S, strafe_left=A, strafe_right=D, talk=T, menu=Escape`. Unknown/missing keys → defaults. Esc always works as escape hatch even if rebound.

## Risks / fallbacks

- httplib streaming quirk → upgrade vendored httplib in a dedicated `build(deps)` commit.
- 't' leaking into input → `swallowNextTextEntered()`.
- App code unverifiable in container → boundary discipline + user smoke tests; core stays green every commit.
- Token/usage interruption → NPC_CITY_PROMPT.txt + plan file + small pushed commits make any point resumable; on resume, reread prompt file and `git log` to find position.

## CLAUDE.md compliance

Docstring/comment on every public function; no TODOs; tests for all new core logic; `type(scope):` commits; push only to feature branch; don't touch docs/learning/llm.md or sibling games.
