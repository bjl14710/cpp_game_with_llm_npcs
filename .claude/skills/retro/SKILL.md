---
name: retro
disable-model-invocation: true
description: Weekly retro on the automation itself — what the nightly runs and routines got wrong, and a draft PR of skill/CLAUDE.md/command fixes so every future run improves.
argument-hint: Optional window, default "7 days"
---

Load the `routine-guardrails` skill first and follow it throughout.

Window: "$ARGUMENTS" (default 7 days).

## 1. Collect
- Nightly output: OVERNIGHT_REPORT.md files if present locally, PRs opened by automation in the window (`gh pr list --state all --search "created:>=YYYY-MM-DD"`), their review comments, and CI results.
- Routine output: issues created by routines in the window and what Brandon did with them (approved / rejected with reason / ignored).
- Reverts and fix-up commits: `git log --since=... --grep="revert\|fixup\|fix:"`.

## 2. Diagnose
Look for patterns, not one-offs. Typical ones:
- Rejection reasons that repeat → a missing non-goal in DIRECTION.md or a weak filter in a routine.
- PRs that over-built → minimal-build-ladder / code-economy not being applied, or tickets sized too big.
- Stalled or abandoned tickets → vague acceptance criteria.
- Same lint/test failure across PRs → a standards skill gap.
- Proposals ignored for 2+ weeks → too many proposals, or the wrong kind; recommend lowering a cap in guardrails §6.
- Model-routing surprises → check `CLAUDE_CODE_SUBAGENT_MODEL`, pinned IDs, effort settings.

## 3. Fix
For the top 1–3 patterns, make concrete edits to `.claude/skills/`, `.claude/commands/`, `CLAUDE.md`, or `.claude/project/DIRECTION.md` on branch `chore/retro-YYYY-MM-DD`, and open a draft PR labeled `tooling`. The PR description lists each pattern, its evidence, and why the edit should prevent it. Never edit the golden eval set or test expectations to make things pass.

Edits to user-level files in `~/.claude/` can't go in a repo PR: write those as a patch in the PR description for Brandon to apply.

## 4. Summary
Per guardrails §8, plus a one-line scorecard: proposals filed / approved / rejected / ignored; previews merged / promoted / rejected.
