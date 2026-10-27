---
name: propose
disable-model-invocation: true
description: Turn an approved issue into a preview — a `preview/<n>-<slug>` branch with the change, tests, and a draft PR whose description is a proposal summary you can approve or reject. Never merges.
argument-hint: Issue number, or "--auto" to preview up to 2 proposed + nightly-safe + size:S issues
---

Load the `routine-guardrails` skill first and follow it throughout. §1 (the approval gate) is the whole point of this command.

Target: "$ARGUMENTS"

## 0. Select issues
- If given an issue number: it must be labeled `approved`, or `proposed` + `nightly-safe` + `size:S`. Otherwise stop and say why.
- If `--auto`: `gh issue list --label proposed --label nightly-safe --label size:S --state open`, exclude any with an open PR or a `preview/<n>-*` branch already, exclude any whose only evidence is [web], take the top 2 by age.
- Refuse anything touching trading execution, credentials, DO-178C/MISRA-scoped code, or DIRECTION.md hard constraints, even if labeled.

## 1. Plan (before writing code)
Read the issue, the code it names, and DIRECTION.md. Write a short plan: files to change, the minimal-build-ladder rung you're on, and how you'll prove the acceptance criteria. If the plan exceeds the issue's size label by a full step (S→L), stop and comment on the issue instead of building.

## 2. Build in isolation
- Create a worktree on a fresh branch from the default branch: `git fetch origin && git worktree add -b preview/<n>-<slug> .claude/worktrees/preview-<n> origin/<default>`. Never work on main. (Make sure `.claude/worktrees/` is gitignored.)
- Delegate implementation to the `proposal-builder` subagent with the plan and the worktree path. Follow the repo's standards skills (python-standards, c-cpp-standards, code-economy, minimal-build-ladder) where present.
- Add or update tests that encode the acceptance criteria. Run the full relevant test suite and linters [ran].
- Commit as `type(scope): description`. Push the branch.

## 3. Open the draft PR
`gh pr create --draft --label preview-ready` with this description:

```
## Proposal: <title>   (closes #<n> if approved)

**What changes:** 2–4 sentences in plain language.
**Why:** link to the issue and its evidence.
**How it was verified:** commands run and results [ran]; acceptance criteria checklist with each box ticked or explained.
**Diff at a glance:** files changed, +/- lines, any new dependency (with license).
**Risk / rollback:** what could break; rollback is closing this PR.
**What you'd learn reviewing this:** 1–3 concepts (feeds /teach).

### Your call
- ✅ Merge as-is → mark ready for review and merge
- 🔁 Needs full implementation → add `ready-for-ai` to the issue; the nightly run builds on this branch
- ❌ Reject → close the PR and label the issue `rejected` with a one-line reason
```

## 4. Summary
Per guardrails §8: previews built (PR links), issues skipped and why.
