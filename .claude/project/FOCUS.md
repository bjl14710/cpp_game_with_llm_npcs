# Project focus: LLM NPC City (bjl14710/cpp_game_with_llm_npcs)

Project-specific knowledge for the shared routines (routine-guardrails §2). This is a
C++17 + raylib game. It is NOT the Godot NPC game, so ignore the kit's built-in
"LLM-NPC game (Godot 4)" focus and lens blocks for this repo.

## Scout checks
- Tests: `make -C tests test` (doctest, headless; builds all of `src/core/` + `tests/`, ~80 s). Baseline 2026-10-25: 218 cases passed, 1 skipped. Record pass/fail counts.
- Static analysis: clang-tidy over `src/core/*.cpp` with `-std=c++17 -Isrc/core -Iexternal`; report counts by check and the top files.
- Core/app split: `grep -rn "raylib\|rlgl\|raymath" src/core/` must be empty.
- Model output as contract: trace every path from an LLM reply (`LlmClient` drain → `DialogueSession`/`GroupSession` → `NpcAction` directive tags, `PersonaLoader`, `WorldGen`/`WorldGenValidate`) to world state. Flag anything that reaches state unparsed or unvalidated, and any validation failure without a deterministic fallback.
- Host authority and threading: NetServer/NetClient threads never touch `World`, `Npc`, or `LlmClient` (docs/DEVELOPER.md thread rule). Flag nondeterminism in replicated state (unseeded RNG, wall-clock time, unordered-map iteration feeding snapshots) and wire changes without a `kNetProtocolVersion` bump.
- Secrets: no API key in code, `config/*.cfg`, or history; `config/secrets.cfg` stays gitignored.
- Prompt context: memory/fact/gossip injection gated in code (`FactStore`, `Gossip`, `ConversationStore`), not only by prompt wording.
- Dependencies: vendored `external/` headers (httplib, json.hpp, doctest, sqlite3) and the raylib FetchContent tag (5.5) vs. latest upstream.
- `src/app/main.cpp` (~2,000 lines, largest file): size and churn; propose extractions only with a test seam.

## Bug-hunt hot spots
- `NetServer`, `NetClient`, `NetFraming`, `HostChatRouter`: threads, queues, framing limits, reconnects.
- `LlmClient`, `StreamAssembler`, `OllamaBackend`, `OpenAiBackend`: chunk boundaries, error/timeout paths, request-id routing.
- `PersonaLoader`, `WorldGenValidate`, `SandboxMap`, `Config`: parsers fed by hand-edited files or model output.
- `FactStore`, `ConversationStore`, `CharacterStore`: persistence, corrupt files, SQLite use.

## Design lens
- Emergent NPC behavior, memory and gossip that pay off in play, the in-game character creator (VISION.md Phase B), multiplayer moments, visual charm on raylib.
- Every idea is a C++ implementation in this codebase. No rewrites, engine moves, or scripting-language gameplay.
- Constraints for the skeptic to press on: core/app split, host-authoritative threading rule, validated model output, local-first with BYO-key cloud.

## Research notes
- The game stays C++ (see DIRECTION.md). Ideas from any language are fine as inspiration, but recommendations must be implementable in C++, and libraries must be C/C++, license-checked, and usable as a vendored header or a pinned CMake FetchContent dependency.
- Key dependencies: raylib 5.5, cpp-httplib, nlohmann/json, doctest, SQLite amalgamation, Ollama (qwen3:8b default), OpenRouter's OpenAI-compatible API.
- Python is only for offline tools/benchmarks in `tools/`.

## Cloud session: runs / doesn't run
- Runs: `make -C tests test`, clang-tidy, g++/cmake (raylib configures via FetchContent), git history, web research.
- Not here: Ollama models (tools/bench_npc_models.py, tools/bench_schema_models.py, test_llm_live), the game window and screenshots (no display/GPU), OpenRouter (no key).
