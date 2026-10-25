---
name: triage
disable-model-invocation: true
description: Your decision session — walk the queue of proposals and previews one at a time and record approve / reject / defer / promote decisions as labels. The only routine that applies Brandon-owned labels, and only on his explicit answer.
argument-hint: Optional — "previews" or "proposals" to review only one kind; "all-repos" to walk every repo in ~/.claude/routines/repos.txt
---

This is an interactive session with Brandon. Never run it headless. Load `routine-guardrails` for the label rules; here, and only here, you may apply `approved`, `ready-for-ai`, and `rejected` — but only when Brandon chooses that option for that specific item in this session.

Scope: "$ARGUMENTS"

## 1. Build the queue
- Previews: `gh pr list --draft --label preview-ready`
- Proposals: `gh issue list --label proposed --state open` (exclude ones already `approved`)
- Sort: previews first (work is already done), then `bug`, then `proposed` by size S→L, then `research`/`design`.
- Show a one-screen overview: counts per type, oldest item age. Anything older than 30 days is a candidate for bulk-defer; offer that once.

## 2. One item at a time
For each item, show at most ~10 lines: title, why it matters, evidence summary, size/lane, and for previews the verification results and diff stat. Then ask one question with these options:
- Previews: **Merge** (mark ready + merge, only if checks pass) / **Promote to nightly** (`ready-for-ai` on the issue) / **Reject** / **Skip for now**
- Proposals: **Approve** (`approved`, so `/propose` builds it) / **Approve + preview now** (run /propose on it after triage) / **Reject** / **Defer** (comment "deferred YYYY-MM-DD") / **Skip**

On Reject, ask for a one-line reason (offer 3 likely reasons as options), then close with label `rejected` and the reason as the closing comment. That comment is what stops routines re-proposing it.

## 3. Wrap up
- Print what was decided.
- If any "Approve + preview now" items, run `/propose` on each.
- If Brandon rejected 3+ items for the same reason, suggest a DIRECTION.md non-goal or a guardrail tweak that would have prevented them.
- Offer a /teach on any concept from a merged preview he didn't recognize.
