# Dialogue scorecards and baselines

Output of `tools/eval_dialogue.py`. Plan:
`.claude/plans/npc-dialogue-measurement.md`, steps 8-10.

**SCAFFOLD** — this documents what steps 8-10 produce. No baselines exist yet;
the first real run writes one.

- `baselines/golden-vN.json` — **committed.** The reference a later run is
  compared against. One per golden-set version.
- `reports/` — **gitignored.** Per-run scorecards, machine- and
  moment-specific. The conclusions get committed, the traces do not — the same
  rule `docs/perf/` already follows.

## Two classes of result, and the line between them

**Blocking (deterministic).** No judge model anywhere near these. The harness
put the secret in the prompt or it did not, and it knows the token, so the
ground truth is exact:

- **secrecy** — gate closed: the token must not appear. Gate open: it must.
- **consistency** — a `stay_consistent_with` probe must not contradict the
  named probe across save-branches.
- **schema validity rate** — `DirectiveCheck`, must not fall below baseline.

**Advisory (statistical).** Reported against baseline, never blocking on its
own: distinct-1/distinct-2, self-BLEU on a sample, response-length mean and
variance, em-dashes per 1k characters, sycophancy-phrase flag rate, and
paraphrase semantic stability — the only judged number in the system.

Every advisory metric carries `"advisory": true` and **its own `n`**. That is
`tools/role_leak_probe.py`'s honesty requirement, inherited verbatim: a probe
that hides its own sample size is worse than no probe.

## Three exit states, all distinguishable

| State | Exit | Meaning |
|-------|------|---------|
| `PASS` | 0 | Every blocking assertion held |
| `BASELINE ESTABLISHED` | 0 | No baseline for this version; one was written. **A first run does not fail** — a harness that fails when it has nothing to compare against teaches people to pass `--force`, and then it never gates anything again |
| `FAIL` | non-zero | A blocking assertion broke. Every failure is listed, not just the first |
| `CANNOT EVALUATE` | non-zero | The model was unreachable or the set is not frozen. **Not a pass.** A gate that could not run has not passed, and the output must never let the two be confused |

## Version comparability

A run against `golden-v2` reports **both** the v2 total and the score over the
subset of probes that existed in v1. Reporting only the current-version total
is how a version bump quietly resets a trend line.

## What this never does

No promotion of responses into prompts, no threshold auto-adjustment, no
self-tuning of any kind. A regression is reported and stays reported; fixing it
is a person's job. That separation is the only reason the numbers mean
anything.
