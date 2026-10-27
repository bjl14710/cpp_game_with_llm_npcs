---
name: comprehension-check
disable-model-invocation: true
description: Find concepts in recently merged code that Brandon hasn't learned yet (not in docs/learning/GLOSSARY.md) and queue them for /teach and /drill-me, so automation doesn't outpace his understanding of his own codebase.
argument-hint: Optional window, default "7 days"
---

Load the `routine-guardrails` skill first.

Window: "$ARGUMENTS" (default 7 days).

1. Collect what changed: `git log --since=<window> --merges` plus merged PRs in the window (`gh pr list --state merged`), including automation-authored ones. Read the diffs, not just titles.
2. Extract concepts: libraries, algorithms, language features, patterns, protocols, and domain ideas the code now depends on. Ignore trivia (renames, formatting).
3. Compare against `docs/learning/GLOSSARY.md` and `docs/learning/PROGRESS.md` if they exist locally (they are gitignored, so in a remote session they usually won't — then treat everything as unlearned and say so).
4. Rank by: how central the concept is to code Brandon will have to maintain, then by how often it appears. Keep up to 8.
5. Output one issue labeled `teach` titled `teach: queue YYYY-MM-DD` (dedupe: if an open `teach` issue exists, comment on it instead). For each concept: one-line definition, where it appears (file:line, PR link), why it matters for this repo, and the exact command to run, e.g. `/teach "pybind11 GIL release"` or `/drill-me "purged k-fold CV"` (drill if it's already in PROGRESS.md as taught).
6. If run locally and `docs/learning/PROGRESS.md` exists, append a "Queued from comprehension-check YYYY-MM-DD" section listing the concepts. Never commit docs/learning/.
