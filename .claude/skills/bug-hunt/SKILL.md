---
name: bug-hunt
disable-model-invocation: true
description: Actively hunt for bugs — fresh-eyes review of the riskiest code plus adversarial tests, fuzzing, and mutation testing. Files up to 5 `bug` issues, each with a reproducer.
argument-hint: Optional path or module to focus on (default: highest churn × complexity)
---

Load the `routine-guardrails` skill first and follow it throughout.

Your job is to find real bugs, prove each one, and file it. A suspected bug with no reproducer is a note in "Also noticed", not an issue.

Focus: "$ARGUMENTS" (if blank, pick the 2–3 files with the highest churn × size over the last 90 days, plus anything changed by the last 7 days of nightly runs).

## 1. Pick targets
- Read the context layer. Note the hard constraints in DIRECTION.md; violations of those are the highest-severity bugs.
- List the target files and why each was chosen.

## 2. Fresh-eyes review
Spawn the `fresh-eyes-reviewer` subagent once per target file. Give it ONLY the file path, the public interface it implements, and DIRECTION.md's hard constraints — not your theories, so it isn't anchored. Ask for: correctness bugs, unhandled edge cases, concurrency/ordering issues, resource leaks, and tests that pass without testing anything.

## 3. Prove it
For each candidate from the review or your own reading, try to make it fail:
- Write a minimal failing test in a scratch location (`.claude/reports/bughunt-scratch/`, gitignored) and run it. Do not add it to the repo's test suite in this routine.
- Where a property or fuzz harness fits (parsers, schema validators, SCPI command handling, ITCH decoding, money arithmetic), run a short Hypothesis or libFuzzer pass (time-boxed to ~5 minutes per target).
- For test-suite quality, run a short mutation-testing pass on one module (mutmut for Python, or a manual mutate-and-run on 5–10 lines in C++). Surviving mutants mean tests that don't catch anything.
- Sanitizers: if a C++ build target exists, build once with `-fsanitize=address,undefined` and run the tests.

## 4. File
For each proven bug (max 5): label `bug`, `proposed`, `size:*`, lane. The issue body follows guardrails §7 and must include:
- The reproducer test code inline and the exact command that fails, with its output [ran].
- Severity: crash / wrong result / constraint violation / flaky / test-quality.
- A suggested fix in 1–3 sentences (do not implement it).

## 5. Summary
Per guardrails §8. Include counts: candidates reviewed, proven, filed, surviving mutants.
