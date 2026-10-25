---
name: scout
disable-model-invocation: true
description: Repo health sweep — run deterministic tools, review recent work, and file up to 5 evidence-backed `proposed` issues. Read-only on source code.
argument-hint: Optional focus, e.g. "test gaps" or "the networking layer"
---

Load the `routine-guardrails` skill first and follow it throughout.

You are scouting this repository for what is worth doing next. You find; you do not fix. Do not modify source files, create branches, or open PRs.

Optional focus: "$ARGUMENTS"

## 0. Orient (keep it short)
- Read the context layer (guardrails §2).
- `git log --since="14 days ago" --stat --oneline`; `gh pr list --state all --limit 20`.
- Identify which project this is from the Project Focus section below. If none matches, run the generic checks only.
- Note what this environment lacks and list skipped checks under "Not run here".

## 1. Deterministic signals first
Delegate the runs to the `scout-analyst` subagent and have it return counts and top offenders, not dumps. Install a missing tool only if it takes one quick command (`uv` for Python).
- Static analysis: ruff and mypy (Python); clang-tidy with the repo config (C++), plus an ASan/UBSan build if a target exists.
- Tests and coverage: `pytest --cov` or `ctest`. Failures and least-covered modules.
- Hotspots (churn × size): `git log --since=90.days --name-only --format= | sort | uniq -c | sort -rn | head -20`, cross-referenced with line counts.
- TODO / FIXME / HACK / XXX harvest with file:line.
- Dependencies: outdated or unpinned packages, lockfile age, vendored/submodule versions.
- Docs drift: things README/CLAUDE.md/DIRECTION.md reference that no longer exist.
- Metrics listed under "Quality bars" in DIRECTION.md: measure what you can here and compare to the last digest.

## 2. Project-specific checks
Run the matching Project Focus block below.

## 3. Recent-work review
- What stalled, got reverted, or needed several fix-up commits? What does that suggest about a skill, a CLAUDE.md rule, or ticket sizing? (Feed this to the run summary's tooling line.)
- New concepts introduced (libraries, patterns, algorithms) → list them for /comprehension-check.

## 4. Triage and file
Turn findings into candidates; keep at most 5 ranked by value ÷ effort; file them per guardrails §5–§7 with labels `proposed`, `size:*`, and `nightly-safe` or `needs-human`. Everything else goes in "Also noticed."

## 5. Summary
Per guardrails §8, and include a health snapshot table: metric | value | trend vs. last digest.

---

## Project Focus

### LLM-NPC game (C++17 + raylib: bjl14710/cpp_game_with_llm_npcs)
- Tests: `make -C tests test` (doctest, headless; builds all of `src/core/` + `tests/`). Record pass/fail counts and the slowest suites. CMake + raylib also configures here, but the game window doesn't run.
- Core/app split: `grep -rn "raylib\|rlgl\|raymath" src/core/` must be empty.
- Model output as contract: trace every path from an LLM reply (`LlmClient` drain → `DialogueSession`/`GroupSession` → `NpcAction` directive tags, `PersonaLoader`, `WorldGen`/`WorldGenValidate`) to world state. Flag anything that reaches state unparsed or unvalidated, and any validation failure without a deterministic fallback.
- Host authority and threading: NetServer/NetClient threads must never touch `World`, `Npc`, or `LlmClient` (docs/DEVELOPER.md thread rule). Also flag nondeterminism in replicated state (unseeded RNG, wall-clock time, unordered-map iteration feeding snapshots) and wire-format changes without a `kNetProtocolVersion` bump.
- Secrets: no API key in code, `config/*.cfg`, or history (`config/secrets.cfg` stays gitignored); keys only from env or secrets.cfg.
- Prompt context: memory/fact/gossip injection should be gated in code (`FactStore`, `Gossip`, `ConversationStore`), not only by prompt wording.
- Vendored deps in `external/` (httplib, json.hpp, doctest, sqlite3) and the raylib FetchContent tag: report versions vs. latest upstream.
- `src/app/main.cpp` size and churn (it's the largest file); propose extractions only with a test seam.
- Not runnable in the cloud: Ollama models (bench_npc_models.py, bench_schema_models.py, test_llm_live), the raylib window and screenshots, OpenRouter (no key).

### Algorithmic trading platform
- Leakage audit: future data in features, fitting/scaling before the split, missing purge/embargo.
- Every reported strategy result carries its Deflated Sharpe Ratio and trial count.
- Execution layer: dry-run guard intact, no path to live orders. Anything here is `needs-human`.
- Reproducibility: seeds, data versioning, pinned calendars.
- Strategy Arena: strategies missing tests or acceptance criteria.

### HFT practice repo
- Latency benchmarks vs. last recorded numbers; flag >5% regressions with a cloud-VM-noise caveat.
- Hot path: allocations, exceptions, virtual calls, locks in parse/book-update code.
- ITCH parser: malformed messages, endianness, whether fuzzing exists.
- pybind11: GIL released around long C++ calls; copies vs. zero-copy views.
- Research layer: multiple-testing correction on lead-lag/Granger results.

### Silmulator
- SCPI coverage vs. README/spec claims; `SYST:ERR?` error-queue behavior.
- Simulation determinism, timing model, GUI thread separate from model thread.
- pytest coverage per instrument block, headless with `QT_QPA_PLATFORM=offscreen`.

### Gmail finance tracker
- Secrets: OAuth tokens/credentials not committed; .gitignore covers them.
- Parser robustness: fixtures per bank email format; behavior on unparseable mail.
- Playwright: brittle selectors; OTP codes never logged.
- SQLite inserts idempotent on re-run.

### Claude Code tooling (any repo with `.claude/`)
- `CLAUDE_CODE_SUBAGENT_MODEL` overrides, floating aliases where pinned IDs are expected, Fable in standing configs.
- Skills/commands referencing missing files or skills.
