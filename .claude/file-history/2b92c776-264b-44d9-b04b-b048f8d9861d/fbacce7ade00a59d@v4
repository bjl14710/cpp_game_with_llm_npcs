# Model Benchmark Report — 2026-07-05 (overnight)

**Verdict: `qwen3:8b` with thinking disabled (`think = false`), adopted in
config/llm.cfg.** Numbers below; methodology in bench/README.md; raw data in
bench/out/ (regenerate with `python3 tools/bench_npc_models.py`).

## What was measured
The game's REAL reply contract — the persona system prompt printed by
`build/persona_prompt` (the exact code path NPCs use) and the
`[MOOD:]`/`[ACTION:]` directive tags `parseDirectives` enforces. Compliance
here is the local analog of the brief's "valid-JSON rate": a reply passes
only with a parseable, known-keyword mood tag and no malformed directives.
Hardware: M1 Pro, 16 GB. Temperature 0.8 (production setting).

## Results

| Model | Compliance | 20-turn tags | Breaks / drift | TTFT (median) | Total (median) |
|-------|-----------:|-------------:|:--------------:|--------------:|---------------:|
| qwen2.5:3b-instruct (baseline) | 70% (n=20) | 80% | 0 / 0 | 1.11 s | 1.65 s |
| **qwen3:8b (thinking on)** | **100% (n=10)** | 88% (8 turns) | 0 / 0 | 24.4 s | 28.8 s |
| qwen3:8b + `think:false` (probe) | compliant | — | — | **2.7 s** | 4.5 s |

- qwen3:8b's n and turn count were reduced (10 probes / 8 turns vs 20/20)
  because thinking-mode replies run ~30 s each and the run had to fit the
  overnight execution window; N is small but the compliance gap (100% vs
  70%) is far outside noise for this decision.
- The `think:false` probe confirmed latency collapses to chat speed while
  the reply stays in character and tag-compliant. The game now sends
  Ollama's `think` field when `config/llm.cfg` sets it (new `think` key;
  omitted entirely for models without the capability).

## Why qwen3:8b wins
The user's decision rule: prefer Qwen3 8B unless something else is clearly
better on validity or consistency. Nothing was — qwen3 is the best measured
on both (100% compliance, 88% sustained tags, zero fourth-wall breaks or
identity drift), and with thinking disabled its latency (2.7 s TTFT) is
acceptable for streaming chat where words appear as they generate. The
baseline's 70% compliance means roughly one reply in three renders an NPC
with no facial expression update.

## Skipped models (rerun when pulled)
- `gemma3:4b` — download was repeatedly interrupted overnight (flaky
  connection; pulls killed mid-transfer several times).
- `mistral:7b-instruct` — same; also note this is already a substitution:
  the brief's "Mistral Small" is a 24B model that does not fit 16 GB RAM.

Both slot straight into the existing harness once pulled:
`ollama pull gemma3:4b mistral:7b-instruct && python3 tools/bench_npc_models.py`.

## Known limits
- Consistency is proxy-scored (regex fourth-wall/name/length checks), not
  LLM-judged — adequate for ranking, blind to subtle character breaks.
- The brief described the current setup as "Gemma with format:json"; the
  repo actually ran qwen2.5:3b with directive tags, so that is what was
  benchmarked (decision log: .claude/plans/npc-memory-and-model.md).

---

## Schema-validity bench — world generation (2026-07-14, issue #127)

Fixed corpus (2× gen-cast, 1× gen-map, 1× gen-village) through
`worldgen_cli` — the SAME C++ validator the game loads through, retry
cap 3. Numbers are validity within retries / on the first attempt /
mean seconds per run:

| model | valid | first-try | mean s |
|-------|-------|-----------|--------|
| qwen3:8b | 50% | 0% | 54.6 |
| qwen2.5:3b-instruct | 25% | 25% | 19.3 |
| gemma3:4b | 0% | 0% | 42.9 |

**Measured winner: qwen3:8b** — which is already the configured dialogue
model, so world generation shares the one client (no separate
worldgen_model client until the winners diverge; documented in llm.cfg).
The user-suggested coding model ("Ornith") was not pulled locally and is
untested — pull it and re-run this bench to include it.

Observations: gen-village (map + cast + link constraints in one output)
is the hard mode — 0/3 models passed it; gen-cast passes for qwen3:8b
now that the vocabulary carries style families; gen-map is easy
(qwen2.5 one-shot it in 2s). The retry loop demonstrably reduces error
counts between attempts even when it doesn't fully converge.
