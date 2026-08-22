# Issue Planning Summary
Date: 2026-08-21
Repo: bjl14710/cpp_game_with_llm_npcs
Focus: NPC dialogue measurement (two loops)
Plan: .claude/plans/npc-dialogue-measurement.md

## Milestone 34 — NPC dialogue measurement: telemetry + golden set

| # | Title | Concept |
|---|-------|---------|
| 299 | plumb per-turn latency and token cost through the reply path | Measuring latency and token cost at the backend boundary of a streamed request |
| 300 | DirectiveCheck — one schema verdict, two callers | Why a forgiving parser and a strict validator need the same scanner but different verdicts |
| 301 | prompt provenance — template version plus per-turn fingerprint | Provenance for non-deterministic outputs: a stable fingerprint versus a portable hash |
| 302 | TelemetryLog — SQLite capture of every dialogue turn | Append-only diagnostic capture kept structurally separate from game state |
| 303 | ENABLE_DEV_TOOLS gates telemetry and the rating hotkeys | Compile-time capability gating, and why a runtime flag does not satisfy it |
| 304 | persona_prompt --state renders the full prompt for a scripted state | Scripted-state fixtures: exercising real prompt assembly instead of reimplementing it |
| 305 | golden set v1 — format, loader and freeze manifest | Freezing a calibration standard: why a measurement instrument must not be editable in place |
| 306 | eval_dialogue — deterministic secrecy and schema gates | Ground-truth assertions versus judged scoring, and when a judge is the wrong instrument |
| 307 | scorecard statistics, baselines and version discipline | Baseline-relative reporting and keeping a trend line comparable across instrument versions |
| 309 | curate_candidates — the review bridge and its contamination refusal | Train/test contamination in a hand-curated few-shot loop, and why the check must be tooling |
| 308 | [needs-human] docs/learning, OVERNIGHT_REPORT.md and .claude/memory are tracked, not ignored | n/a — repository hygiene decision |

Dependency order: 299/300/301 are independent and come first. 302 needs all
three. 303 needs 302. 304 needs 300 (its --parse half only). 305 is
independent. 306 needs 304+305. 307 needs 306. 309 needs 302+305.

## What was NOT planned, and why

- **#200 (implement the role leak probe).** Already queued and well designed.
  An earlier draft of the plan said to absorb and delete
  `tools/role_leak_probe.py`; that was wrong and has been corrected in the plan
  and in the eval_dialogue scaffold. It asks a DISTRIBUTIONAL question about a
  whole cast (is the killer separable from the secret-keepers by mood?); a
  golden probe asks an EXACT question about one turn. Neither subsumes the
  other. #306 depends on the shared conventions and does not modify that file.
- **Converging tools/eval_lines.py and tools/probe_lib.py.** eval_lines.py is
  working code that gates what ships into banks/*.bank. This milestone should
  not be able to break it. The helpers are re-homed in probe_lib.py and
  converging the two callers is deliberately deferred, not overlooked — the
  note is in probe_lib.py's docstring so a third copy does not appear.
- **Fine-tuning, auto-promotion, threshold auto-adjustment.** Explicitly out of
  scope. This milestone builds the instrument that would inform that decision;
  a system that tuned itself from its own measurements could not be trusted to
  report a regression.
- **Multiplayer telemetry.** A guest's conversations are served host-side. The
  local process records only what it generated, reusing the `!joined` guard the
  F1/F2 rating path already applies.
