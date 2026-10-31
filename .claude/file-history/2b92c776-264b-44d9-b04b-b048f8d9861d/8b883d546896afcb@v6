# Developer Guide

> Long-term product direction — cloud models, the in-game character creator,
> cost model, and monetization — lives in [VISION.md](VISION.md).

## Architecture: core / app split

The codebase is split so that all game logic builds and tests on a headless
machine, while graphics stay isolated:

```
src/core/   Rendering-free. Everything here is unit-tested.
src/app/    raylib (windowing, input, 3D, UI overlay).
external/   Vendored single-header deps: httplib, nlohmann/json, doctest.
tests/      doctest suites + a plain-make runner for cmake-less containers.
personas/   One .persona file per NPC (identity + world placement).
assets/     CC0 model packs, fetched by tools/fetch_assets.sh (gitignored).
config/     llm.cfg (provider/model/host/latency), keybindings.cfg, secrets.cfg.example.
```

CMake fetches a pinned raylib via FetchContent at configure time — a clean
clone builds with no graphics packages installed. Keep the split absolute:
**nothing in `src/core/` may include raylib (or any rendering) headers.**

### Core modules

| Module            | Responsibility |
|-------------------|----------------|
| `LlmClient`       | One worker thread for all NPCs; owns the request/reply queues. Delegates the HTTP + wire format to a swappable `LlmBackend`. Main thread polls `drainDeltas()` / `drainReplies()` per frame; `warmUp()` preloads the model; `model()`/`setModel()`/`availableModels()` drive the in-game picker. |
| `LlmBackend`      | Provider interface (`chat()` + model accessors). `makeBackend()` picks by `provider`: `OllamaBackend` (local `/api/chat` NDJSON + `/api/tags`) or `OpenAiBackend` (OpenAI-compatible `/v1/chat/completions` SSE — OpenRouter/OpenAI/Gemini or a local `/v1`; HTTPS needs an OpenSSL build). |
| `StreamAssembler` | Reassembles newline-delimited lines from arbitrary TCP chunk boundaries; `parseOllamaChunk` / `parseOpenAiChunk` map one line to delta/done/error for the two wire formats. |
| `Persona`         | Static NPC identity, rendered into the system prompt. |
| `PersonaLoader`   | Parses `personas/*.persona` (key=value header, `---`, free-form directives) including world placement. |
| `Npc`             | Per-NPC chat history (bounded), pending-request tracking, world placement. History only grows on successful replies. |
| `City`            | Building AABBs + circle collision with axis-separated slide (`resolveMovement`). |
| `World`           | City + NPC roster; `nearestNpcWithin` drives the talk prompt. |
| `DialogueSession` | Roaming → Talking → WaitingReply → Streaming state machine; routes streamed deltas by request id. |
| `KeyBindings`     | Action → key-name map with swap-on-conflict rebinding and file persistence. Names are translated to SFML codes only in `src/app/InputMap`. |
| `Config`          | Tiny key=value reader/writer: `loadLlmConfig` (incl. provider/base_url and env-or-`secrets.cfg` API-key resolution) and `setKvValue` (in-place single-key edit, used to persist the picked model). |
| `NetMessage`      | Multiplayer wire protocol: type-tagged JSON envelope (`encodeMessage`/`decodeMessage`, nullopt on garbage), `kNetProtocolVersion` handshake constant, pose serialization helpers. |
| `NetFraming`      | `FrameAssembler`: 4-byte big-endian length prefix + payload over TCP; rejects prefixes over 1 MiB (drop the connection — the stream can't resync). The TCP sibling of `StreamAssembler`. |
| `NetSocket`       | Header-only winsock/POSIX shims: `sendAll`, recv timeouts, `SO_NOSIGPIPE` on macOS. |
| `NetServer`       | Authoritative host. Threads: accept → per-connection reader (handshake: version, join code, `kMaxPlayers`) → tick (snapshot broadcast + connection reaping). Its threads touch only sockets and queues; the host loop feeds `setHostPose`/`publishNpcPoses` in and drains `drainChatEvents()` out, so `World`/`LlmClient` stay single-threaded. |
| `NetClient`       | Joining side: `connect` (4s timeout + handshake), reader thread → `poll()` drain on the main loop (the `drainReplies` shape). Never simulates; renders what snapshots say. |
| `HostChatRouter`  | Host-loop glue routing remote NPC chat: per-NPC queues serialize concurrent askers, deltas/replies return to the requesting player only, `NpcSpeechBubble`/`NpcMoodUpdate` broadcast to all. |

### App modules

- `RaylibRenderer` — the 3D pass: asset-pack city fitted to the collision
  AABBs, animated glTF characters with per-entity clip clocks, mood emote
  billboards, and `worldToScreen` for nameplates.
- `Assets` — loads every model/animation once at startup; maps building ids
  and character variant seeds to models; bakes the six mood faces. Missing
  packs degrade to primitives with an on-screen fetch_assets.sh hint.
- `FaceTexture` — bakes the procedural mood faces to textures (brow-tilt
  semantics preserved from the legacy renderer).
- `main.cpp` — mode machine (Playing / Dialogue / Menu) and the frame loop:
  BeginDrawing → 3D pass → 2D overlay (nameplates, HUD, dialog/menu) →
  EndDrawing. `--frames N [shot.png]` runs headless-style smoke checks.
- `DialogUI` — chat overlay with live streaming line; per-frame `pollInput()`
  (raylib is polled, not event-driven); `swallowPendingText()` keeps the talk
  key's character out of the input box.
- `Menu` — mouse-driven pause menu, key rebinding (GetKeyPressed capture),
  and the Multiplayer host/join page (callbacks injected from `main.cpp` via
  `MultiplayerHooks`, so the menu never sees networking types).
- `InputMap` — portable key-name ↔ raylib key-code table (same names as the
  original SFML build, so saved keybindings.cfg files still load).

### Multiplayer at a glance

One process hosts: its `World` and `LlmClient` stay the single source of
truth, exactly as in solo play. `NetServer` fans state out at ~12 Hz
(`WorldSnapshot`: player poses + NPC pose/mood/behavior) and joined clients
send `PlayerInput` up. Remote NPC chat rides `ChatOpen`/`ChatLine` up and
`ChatDelta`/`ChatReply` back down to the asker only, with
`NpcSpeechBubble`/`NpcMoodUpdate` broadcast so bystanders see effects.

Wire format: every frame is `[4-byte big-endian length][JSON object]`, and
every JSON object carries a `"type"` tag. Bump `kNetProtocolVersion`
(NetMessage.hpp) whenever the format changes incompatibly — the handshake
refuses mismatched builds instead of desyncing.

Thread rule that keeps this safe: **no NetServer/NetClient thread ever
touches `World`, `Npc`, or `LlmClient`.** Game state crosses the boundary
only through value snapshots (`publishNpcPoses`) and drained queues
(`drainChatEvents`, `poll`), all pumped by the host/client main loop.
`tests/test_net_loopback.cpp` runs the full stack — server, clients, router,
FakeOllama — over 127.0.0.1 with no real network or Ollama:

```sh
make -C tests test    # includes framing, protocol, and loopback suites
```

Both machines should run the same build and the same `personas/` roster
(clients label NPC nameplates from their local persona files).

## Building and testing

### Container / Linux (headless tests)

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

or the equivalent `run.ps1`. Requires MSYS2 UCRT64 with `mingw-w64-ucrt-x86_64-{gcc,cmake}` (raylib is fetched by CMake) and an Ollama install with the model pulled. Run `tools/fetch_assets.sh` (or download the packs per assets/LICENSES.md) for the full look.

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
