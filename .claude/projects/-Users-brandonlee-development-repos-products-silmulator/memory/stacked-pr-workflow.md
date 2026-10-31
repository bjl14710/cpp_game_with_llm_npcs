---
name: stacked-pr-workflow
description: "How Brandon merges stacked PRs (top-down into base branches, sometimes live during overnight sessions) and the conflict traps in this repo's shared docs files"
metadata: 
  node_type: memory
  type: project
  originSessionId: 676ac7d9-0d03-4ff4-996c-6c603b22fcac
---

Brandon merges stacked PRs **top-down**: he clicks-merges child PRs into their
base branches (e.g. #33→#32→#31→#30→#29 on 2026-07-17), collapsing the stack
into the bottom branch, then merges the single bottom PR into `master`. He may
do this LIVE while an overnight session is running — always `git fetch`
immediately before any push to a stack branch and expect non-fast-forward
rejections; fold his merges in rather than forcing.

**Why:** shared append-files (`docs/learning/PROGRESS.md`, `GLOSSARY.md`,
`OVERNIGHT_REPORT.md`) conflict whenever two branches append at the same
anchor, and auto-merges can *silently* violate PROGRESS.md's invariant
(session headings unique + monotonically increasing + chronological) —
2026-07-17 produced a duplicate "Session 4" with exit code 0. After any merge
touching PROGRESS.md, assert the invariant. `docs/learning/index.html` is a
derived file: never hand-merge it, regenerate with
`scripts/build_learning_index.py`.

Related: [[github-auth-setup]]
