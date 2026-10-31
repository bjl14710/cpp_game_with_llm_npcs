# Plan: One-click launch, pluggable LLM backends (cloud-ready), in-game model picker, and a product roadmap

## Context

Today the game only talks to a local Ollama server, hardwired in `LlmClient`, and
the launch scripts merely *check* Ollama is up and bail if it isn't. The user wants
three things, plus a long-horizon vision:

1. **One-click launch** — `open`/double-click/`./` that starts the LLM server *and*
   the game together.
2. **Pluggable models you can add later** — pick among installed models in-game; add
   more without code changes.
3. **Smarter characters via cloud APIs** (Claude / GPT / Gemini), cheaply — to test
   now and possibly sell later.
4. **Future product**: "Character.ai in a 3D world" — players create characters with
   one LLM (e.g. Gemini) and converse with them via another (e.g. Claude/GPT), sold
   with token costs passed through.

**Locked decisions (from Q&A):**
- **Scope = "Local now + cloud-ready" (no new required deps).** Ship one-click launch
  + in-game model picker for installed Ollama models, **refactor `LlmClient` into a
  pluggable backend interface** so a cloud backend is a drop-in, and write the full
  roadmap doc. Cloud goes live the moment OpenSSL + a key are added — no rewrite.
- **Cloud routing = OpenRouter.** One OpenAI-compatible endpoint
  (`https://openrouter.ai/api/v1/chat/completions`), one key, 315+ models (Claude,
  GPT, Gemini, …); switch model by a config string.

**Key technical findings (research):**
- **Ollama, OpenRouter, OpenAI, and Gemini all speak the same OpenAI
  `/v1/chat/completions` shape.** Ollama exposes it at `localhost:11434/v1`. So *one*
  OpenAI-compatible backend covers local **and** cloud — the only fork is HTTP (local)
  vs HTTPS (cloud → needs OpenSSL). This lets us **test the cloud code path locally
  against Ollama's `/v1` with zero new deps.**
- OpenAI SSE stream: lines `data: {json}` where text is `choices[0].delta.content`,
  terminated by `data: [DONE]`; errors arrive as a non-200 body or `{"error":...}`.
- httplib does HTTPS only when compiled with `CPPHTTPLIB_OPENSSL_SUPPORT` + OpenSSL.
- Launcher hooks: `ollama serve` (start), `GET /api/tags` (list installed),
  `ollama pull <model>` (install).

**Branch:** new `8-pluggable-llm-and-one-click`, branched from current HEAD
(`7-weapons-money-and-mayhem`) since the work is sequential and builds on the latest
game. Incremental `type(scope): description` commits; build + `make -C tests test`
green after each; inline review before each commit; no push (user's call).

**Constraints (CLAUDE.md):** all new logic is SFML-free `src/core/` with doctest
tests; renderer/Menu visuals eyeballed; **no committed secrets**; OpenSSL stays
*optional* (auto-detected, never required).

---

## Phase 1 — One-click launch  `chore(launch): auto-start Ollama and add a double-clickable launcher`

Harden the existing launchers so they bring the whole stack up; add a Finder-friendly
entry point.

- **`run.sh`** (macOS/Linux): when provider is local and the server isn't reachable,
  start it (`ollama serve` backgrounded, log to a temp file) and poll
  `http://HOST:PORT/api/tags` until ready (~20 s timeout). Read `model` from
  `config/llm.cfg`; if it's absent from `ollama list`, `ollama pull <model>`. Then the
  existing cmake build + run. Keep the install hint if the `ollama` binary is missing.
  Skip auto-start when `provider` is a cloud one.
- **`run.command`** (new, macOS): double-clickable wrapper that `cd`s to its dir and
  `exec ./run.sh`; marked executable. Satisfies "open / one click".
- **`run.ps1` / `run.bat`** (Windows): same — `Start-Process ollama serve` if not
  reachable, pull the model if missing, then build + run.

Files: `run.sh`, new `run.command`, `run.ps1`, `run.bat`. Verified by launch.

## Phase 2 — Pluggable LLM backend + config + secrets  `refactor(llm): pluggable backend interface with an OpenAI-compatible cloud path`

Split `LlmClient` into **threading/queue glue (unchanged)** + a swappable **backend**
that owns the HTTP + parse. The worker thread, `submit`/`drainDeltas`/`drainReplies`/
`busy()` and the public API stay identical.

- **New `src/core/LlmBackend.hpp`** — interface:
  `BackendResult chat(const BackendRequest&, const std::function<void(const std::string& delta)>& onDelta)`,
  plus `std::vector<std::string> listModels()`, `setModel`, `model()`. `BackendRequest`
  carries systemPrompt/history/userMessage/internal; `BackendResult{ok,content,error}`.
- **`src/core/OllamaBackend.{hpp,cpp}`** — the *existing* `processOne` body
  (`/api/chat` NDJSON + `parseOllamaChunk`) moved verbatim behind the interface;
  `listModels()` reads `GET /api/tags`. This is the default — behavior-identical today.
- **`src/core/OpenAiBackend.{hpp,cpp}`** — POST `{base_url}/chat/completions`,
  `Authorization: Bearer <key>`, `stream:true`; parse SSE via a new
  **`src/core/OpenAiChunk.hpp`** (`parseOpenAiChunk` → `{delta,done,error}`,
  header-only and unit-testable like `parseOllamaChunk`). Works over **http** (local
  Ollama `/v1`) with no SSL; over **https** only when built with OpenSSL — guarded by
  `#ifdef CPPHTTPLIB_OPENSSL_SUPPORT`, otherwise a clear "cloud needs an OpenSSL build"
  error and fall back to Ollama.
- **`LlmClient` change**: holds a `std::unique_ptr<LlmBackend>`; `processOne` builds a
  `BackendRequest`, calls `backend_->chat(...)` with an `onDelta` that pushes to the
  existing delta queue, maps the result to `ChatReply`. A factory
  `makeBackend(const LlmConfig&)` selects Ollama vs OpenAI-compatible by `provider`.
- **Config** (`LlmConfig` in `LlmClient.hpp` + `loadLlmConfig` in `Config.cpp`): add
  `provider` (default `ollama`), `base_url` (default derived; OpenRouter →
  `https://openrouter.ai/api/v1`), `api_key_env` (default `OPENROUTER_API_KEY`). Key
  resolution order: env var named by `api_key_env` → `config/secrets.cfg` `api_key` →
  empty (cloud disabled with a clear message). **No key ever in version control.**
- **Secrets**: add `config/secrets.cfg` to `.gitignore`; ship
  `config/secrets.cfg.example`. (No `/secrets` dir is touched; this is the safe BYO-key
  pattern.)
- **CMake**: `find_package(OpenSSL QUIET)` — if present, add
  `CPPHTTPLIB_OPENSSL_SUPPORT` + link it (cloud "just works", no config change); if
  absent, builds exactly as today. **OpenSSL stays optional.** Add the new core `.cpp`
  files to the `llm_npc_core` source list (tests auto-glob them).
- **Tests**: extend `tests/test_stream.cpp` for `parseOpenAiChunk` (`data:` prefix,
  `[DONE]`, content delta, `{"error"}`, blank/`: keep-alive` lines); extend
  `tests/test_config.cpp` for the new keys + key-resolution precedence; add a
  `FakeBackend` (mirroring `tests/FakeOllama.hpp`) so `tests/test_llm_client.cpp`
  drives the queue/threading against a fake — no network.

Files: new `src/core/LlmBackend.hpp`, `OllamaBackend.{hpp,cpp}`,
`OpenAiBackend.{hpp,cpp}`, `OpenAiChunk.hpp`; modified `LlmClient.{hpp,cpp}`,
`Config.{hpp,cpp}`, `config/llm.cfg` (documented new keys), new
`config/secrets.cfg.example`, `.gitignore`, `CMakeLists.txt`, the three test files.

## Phase 3 — In-game model picker  `feat(llm): switch models from the pause menu`

- **Persist a single key without clobbering comments**: add `setKvValue(path, key,
  value)` to `Config.{hpp,cpp}` (rewrites only the matching line; mirrors `readKv`),
  used to save the chosen `model` back to `config/llm.cfg`.
- **Menu**: add a `Page::Model` (following the existing `Page`/`Hit`/`layout()`
  pattern in `Menu.{hpp,cpp}`) that lists clickable model names; clicking selects one.
  Inject the choices via small hooks from `main.cpp` (current model + list +
  `onSelect`) so Menu stays free of `LlmClient`.
- **main.cpp wiring**: fetch the installed-model list from `backend_->listModels()`
  (Ollama `/api/tags`); on select, `setModel` on the live client (next requests use it;
  in-flight finish on the old) and `setKvValue` to persist. Models are *added* out of
  band via `ollama pull` (launcher/CLI), which the picker then shows.

Files: `src/core/Config.{hpp,cpp}`, `src/app/Menu.{hpp,cpp}`, `src/app/main.cpp`,
`tests/test_config.cpp` (cover `setKvValue`).

## Phase 4 — Product roadmap doc  `docs(vision): Character.ai-in-3D architecture, cost model, and roadmap`

New **`docs/VISION.md`** (the "plan it out for the future" deliverable), grounded in
the research:

- **Vision & today** — the 3D shell where players populate a city with custom
  characters; what this branch already lays down (pluggable backends, one-click,
  picker).
- **Product architecture** — OpenRouter as the single gateway; **two LLM roles**:
  *Creation* (cheap one-shot, e.g. Gemini Flash-Lite: short description → a
  `.persona` file in the existing format → spawns an NPC, reusing `PersonaLoader` +
  `renderSystemPrompt`) and *Conversation* (streaming, e.g. Haiku / GPT-5-mini).
  In-game character-creator flow; shareable persona packs.
- **Cost model** (per-1M input/output, with a ~800-in/60-out NPC turn and prompt
  caching as the lever): Gemini 2.5 Flash $0.30/$2.50 ≈ **$0.0004/msg (~2,500/$)**;
  GPT-5 mini $0.125/$1.00 ≈ **$0.00016/msg (~6,000/$)**; Claude Haiku 4.5 $1/$5 ≈
  **$0.0011/msg (~900/$)**; character creation ≈ **<$0.0015 each**. → thousands of
  messages per dollar; BYO-key is trivially affordable, resale has margin.
- **Monetization** — (a) BYO-key (simplest, no key liability), (b) hosted
  credits/token packs (**requires a server-side proxy** so your key never ships in the
  client), (c) tiered free-local-Ollama + paid-cloud.
- **Staged roadmap** — A: this branch. B: cloud live + AI character creator. C: BYO-key
  product. D: hosted credits/proxy + persona marketplace. Each with scope + main risk.
- **Risks** — key security (never client-side for resale), content moderation of
  user-made characters, cloud latency vs local, cost runaway (budgets/rate limits),
  packaging/signing.
- **Sources** — the pricing/API links gathered during research.

Files: new `docs/VISION.md`; brief pointers added to `README.md` and
`docs/DEVELOPER.md`.

---

## Out of scope (now)
Adding OpenSSL as a *required* dep; a hosted billing/proxy backend; the in-game AI
character creator implementation (designed in VISION.md, built in a later phase);
per-provider native SDKs (OpenRouter covers all); content moderation; appearance
generation from text. The cloud backend ships **compiled but dormant** until a key +
OpenSSL build are present.

## Risks / notes
- The public `LlmClient` API and Ollama behavior must stay **byte-for-byte identical**
  after the refactor — `OllamaBackend` is the old code path moved, not rewritten;
  `test_llm_client`/`test_llm_live`/`test_stream` are the guardrail.
- Model switching mid-session affects only new requests (documented).
- Menu visuals and the SSE HTTP wiring are eyeballed (can't screenshot the GL window);
  you verify UI/latency, I tune.

## Verification
- `make -C tests test` green (new `parseOpenAiChunk`, config keys + `setKvValue`,
  `FakeBackend` client test).
- `cmake --build build -j` **without** OpenSSL → local behavior unchanged; `./run.sh`
  (or `run.command`) auto-starts Ollama, pulls the model if missing, builds, runs.
- **Cloud path, zero new deps**: set `provider=openai`,
  `base_url=http://localhost:11434/v1` → NPCs answered through the OpenAiBackend
  against Ollama (proves the cloud code path end to end).
- Pause menu → switch model → next reply uses it; choice persists in `config/llm.cfg`.
- **Later (user's step)**: install OpenSSL, set `OPENROUTER_API_KEY`,
  `provider=openrouter`, `model=anthropic/claude-haiku-4.5` → real cloud NPC.
- Inline review before each commit; offer `/code-review` over the batch at the end.
