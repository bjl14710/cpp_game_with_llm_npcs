# NPC model benchmark

Compares local Ollama models against the game's real reply contract — the
persona system prompt (printed by `build/persona_prompt`, the exact code path
the game uses) and the `[MOOD:]`/`[ACTION:]` directive tags parsed by
`parseDirectives` (src/core/NpcAction.hpp).

```sh
cmake --build build -j            # builds persona_prompt
ollama pull qwen3:8b gemma3:4b mistral:7b-instruct   # once
python3 tools/bench_npc_models.py                    # ~30-60 min on M1 Pro
```

Metrics per model:
- **compliance** — share of single-shot replies with a parseable known-keyword
  mood tag and no malformed directives (the local analog of valid-JSON rate)
- **consistency** — a scripted 20-turn conversation scored for fourth-wall
  breaks, identity drift, sustained tag compliance, and style-length
  conformance (proxy scoring; an LLM judge would be better but requires cloud)
- **latency** — median time-to-first-token and total time per reply

Outputs land in `bench/out/results.json` (gitignored); the adopted verdict
lives in `bench/REPORT.md`.
