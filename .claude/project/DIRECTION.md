# Direction: LLM NPC City (C++20 + raylib)
Last reviewed: 2026-10-25 — DRAFT seeded from README, docs/VISION.md, docs/DEVELOPER.md and the last ~60 commits. Lines marked (confirm) are inferred; fix them, then run /direction refresh.

## North star
A C++ game, by choice: the engine, game logic, networking, and LLM plumbing are hand-written C++ (C++20) on raylib, and staying in C++ is part of the point (learning and portfolio), not an accident to be optimized away.

A walkable first-person 3D town where every inhabitant is a live LLM character, heading toward "Character.ai in a 3D world": players describe a character in a sentence, a creation model turns it into a validated `.persona`, and a conversation model plays it in role. NPCs remember, gossip, keep to schedules, and act in the world through validated directive tags. Local Ollama stays the free, offline default; cloud models are BYO-key via OpenRouter. (confirm: portfolio piece first, sellable BYO-key game second?)

## Current phase (confirm)
VISION.md Phase B, "Cloud live + creator". Exit criteria:
- [ ] A real OpenRouter key path works end to end (streaming, action/mood tags, errors surfaced in UI)
- [ ] In-game creator: sentence → persona validated against the parser → NPC spawned and talkable
- [ ] Persona/worldgen schema-valid rate measured per model (tools/bench_schema_models.py) and above a target
- [ ] Turn latency (first token, full reply) measured local vs cloud

## Priorities this phase (confirm)
1. Dialogue quality and persona consistency per backend: local Ollama (qwen3:8b) vs cloud fast tier (Haiku / GPT-5 mini / Gemini Flash)
2. Structured-output reliability for creation and worldgen (validation + error-feedback retries already exist, so improve the rate)
3. Memory and gossip that pay off in play (FactStore, per-perspective summaries, Journal)
4. Visual polish on raylib (Mii-style composite characters, outlines, fog) without breaking the core/app split

## Non-goals (routines must not propose these)
- Rewriting any part of the game in another language (Rust, C#, Python, JS, ...) or moving to an engine (Godot, Unity, Unreal). The game stays C++.
- Replacing the hand-written C++ core with a framework that hides it (ECS frameworks, game engines, Python-scripted gameplay) unless it is a C++ library that clearly beats the current code on a measured problem
- Shipping any API key in the client or committed config (BYO-key or a server proxy only)
- LLM output that becomes game state, files, or actions without parser/validator checks
- Fine-tuning or LoRA before prompt conditioning plateaus (docs/design/TRAIT_FINE_TUNE_PATH.md: document, don't build)
- Built-in NAT punching or relay servers (Tailscale/port-forward is the documented answer)
- Hosted credits, billing, or marketplace work (Phase D) before Phase B/C ship

## Hard constraints
- `src/core/` never includes raylib or any rendering header; everything in core is headless-testable via `make -C tests test`
- Host-authoritative multiplayer: NetServer/NetClient threads never touch `World`, `Npc`, or `LlmClient`; state crosses only through value snapshots and drained queues
- Bump `kNetProtocolVersion` on any incompatible wire change
- Model output is parsed and validated (directive tags, persona format, worldgen schema) before it changes anything; failures fall back deterministically
- Cloud replies aren't reproducible: tests use FakeOllama and fakes, never live models
- Dependencies are pinned (raylib FetchContent tag, vendored single headers, checksummed CC0 asset packs); licenses checked for anything new (assets must stay CC0 or similarly permissive)
- Language: the game is C++ (C++20 on `dev`). Python is allowed only for offline tools and benchmarks in `tools/`.
- Any suggested library must be C++ (or C with a C++-friendly API), license-checked, and pullable as a vendored single header or a pinned CMake FetchContent dependency, like raylib and the headers in `external/`.
- Builds on Windows (MSYS2 UCRT64), macOS, and Linux

## Pivots under consideration (confirm)
- Cloud conversation model as default vs local-first: exploring; decided by quality/latency/cost per turn
- Prompt caching for the persona + protocol block on cloud providers: exploring; decided by measured cost and latency

## Open research questions (research-radar works on these)
- Best small local models (≤8B, Ollama) for in-character roleplay, tag discipline, and secret-keeping; what has replaced qwen3:8b since 2026-07?
- Structured output: Ollama `format` / JSON-schema constrained decoding vs OpenRouter `response_format`, applied to persona and worldgen generation
- NPC memory architecture: what other LLM-NPC projects do for retrieval, summarization, and gossip propagation (e.g. generative-agents descendants)
- Latency budgets for conversational NPCs: time-to-first-token targets, speculative "thinking" barks, streaming UX
- raylib ecosystem: 5.5 → newer releases, glTF animation, shaders for toon/outline, raygui for menus
- Voice: local TTS/STT options with permissive licenses that could fit later (research only)

## Quality bars / metrics we track
- Core unit tests: `make -C tests test`; all pass; target: all pass
- Worldgen/persona schema-valid rate: tools/bench_schema_models.py; qwen3:8b 50% (2026-07-14); target: ≥90% after retries (confirm)
- NPC turn latency, first token: bench/REPORT.md; qwen3:8b think=false ~2.7 s (2026-07-05); target: <1.5 s (confirm)
- Dialogue quality: in-game ratings (RatingLog curated files); target: TBD (confirm)

## What I want to learn from this project
- Networking (framing, server authority, request routing), threading and condition variables, LLM structured output and evaluation, real-time 3D rendering with raylib, and prompt and context engineering for characters. Concepts should flow to /teach and /drill-me (lessons live in docs/learning/).
