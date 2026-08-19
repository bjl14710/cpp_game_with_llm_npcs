# Developer Guide

## Architecture: core / app split

The codebase is split so that all game logic builds and tests on a headless
machine, while graphics stay isolated:

```
src/core/   SFML-free, GL-free. Everything here is unit-tested.
src/app/    SFML + OpenGL. Only builds where graphics libs exist (Windows).
external/   Vendored single-header deps: httplib, nlohmann/json, doctest.
tests/      doctest suites + a plain-make runner for cmake-less containers.
personas/   One .persona file per NPC (identity + world placement).
config/     llm.cfg (model/host/latency) and keybindings.cfg.
```

CMake builds the `llm_npc_core` static library unconditionally; the game
executable is added only when `SFML_FOUND AND OPENGL_FOUND`. Keep it that
way: **nothing in `src/core/` may include SFML or OpenGL headers.**

### Core modules

| Module            | Responsibility |
|-------------------|----------------|
| `LlmClient`       | One worker thread for all NPCs. Streams Ollama `/api/chat` (NDJSON); main thread polls `drainDeltas()` / `drainReplies()` per frame. `warmUp()` preloads the model; `keep_alive` keeps it resident. |
| `StreamAssembler` | Reassembles NDJSON lines from arbitrary TCP chunk boundaries; `parseOllamaChunk` maps one line to delta/done/error. |
| `Persona`         | Static NPC identity, rendered into the system prompt. |
| `PersonaLoader`   | Parses `personas/*.persona` (key=value header, `---`, free-form directives) including world placement. |
| `Npc`             | Per-NPC chat history (bounded), pending-request tracking, world placement. History only grows on successful replies. |
| `City`            | Building AABBs + circle collision with axis-separated slide (`resolveMovement`). |
| `World`           | City + NPC roster; `nearestNpcWithin` drives the talk prompt. |
| `DialogueSession` | Roaming → Talking → WaitingReply → Streaming state machine; routes streamed deltas by request id. |
| `KeyBindings`     | Action → key-name map with swap-on-conflict rebinding and file persistence. Names are translated to SFML codes only in `src/app/InputMap`. |
| `Config`          | Tiny key=value reader + `llm.cfg` loader. |

### App modules

- `Renderer3D` — legacy GL 2.1 immediate mode, procedural textures, the
  first-person camera, and `worldToScreen` for SFML nameplates.
- `main.cpp` — mode machine (Playing / Dialogue / Menu) and the frame loop:
  GL 3D pass → `pushGLStates()` → SFML overlay → `popGLStates()` → `display()`.
  Always restore that order; mixing raw GL with SFML drawing outside the
  push/pop pair corrupts state.
- `DialogUI` — chat overlay with live streaming line; `swallowNextTextEntered()`
  keeps the talk key's character out of the input box.
- `Menu` — mouse-driven pause menu and key rebinding.
- `InputMap` — portable key-name ↔ `sf::Keyboard::Key` table.

## Building and testing

### Container / Linux (no SFML needed)

```sh
make -C tests test          # builds core + all unit tests with g++, runs them
```

The make runner is the primary gate: run it before every commit. It compiles
`src/core/*.cpp` + `tests/test_*.cpp` with `-Wall -Wextra -pthread`.

Tests run fully offline: `tests/FakeOllama.hpp` spins an in-process
`httplib::Server` on a random port that speaks the Ollama streaming protocol,
including error modes and chunk boundaries that split JSON lines mid-token.

Live end-to-end tests (real Ollama on localhost) are opt-in:

```sh
OLLAMA_LIVE=1 make -C tests test            # uses qwen2.5:3b-instruct
OLLAMA_TEST_MODEL=llama3.2:1b OLLAMA_LIVE=1 make -C tests test
```

### Windows (full game)

```bat
run.bat        # double-clickable: checks MSYS2 + Ollama, cmake, build, play
```

or the equivalent `run.ps1`. Requires MSYS2 UCRT64 with `mingw-w64-ucrt-x86_64-{gcc,cmake,sfml}` and an Ollama install with the model pulled.

```sh
cmake -S . -B build -G "MinGW Makefiles" && cmake --build build -j
ctest --test-dir build        # same doctest suites via cmake
```

## Latency playbook

Findings applied from the sibling games and the original companion demo:

1. **Stream replies** (`"stream": true`): first words render ~10x sooner than
   waiting for the full completion.
2. **`keep_alive=10m`** (config/llm.cfg): the model stays in RAM between
   conversations; without it every chat pays a multi-second reload.
3. **`warmUp()` at boot**: an empty-messages request forces the load while
   the player is still walking around.
4. **One worker thread, non-blocking drains**: the render loop never waits on
   the network; 60 FPS is independent of Ollama health.
5. Small default model (`qwen2.5:3b-instruct`) — swap via `config/llm.cfg`.

## Adding an NPC

1. Create `personas/<id>.persona` (see existing files; `name` is required,
   `position`/`facing`/`spot` place them in the world).
2. Pick a free spot — `tests/test_world.cpp` has a placement-sanity pattern;
   NPCs must not stand inside a building AABB.
3. That's it: the roster is whatever `personas/` contains, sorted by filename.
   Palette color comes from roster order (`Renderer3D::kNpcPalettes`).

## Conventions

- Commits: `type(scope): description`, small (2-3 files), pushed to the
  feature branch — never to main.
- Every public function carries a doc comment; no TODOs in committed code.
- New core logic requires doctest coverage in the same or the following
  commit.
