---
description: Run a full feature-development pipeline using the specialist subagents, with checkpoints for my approval
---

Develop the feature "$1" using a disciplined pipeline. You orchestrate the
specialist subagents in sequence. Pause at the marked checkpoints for my
approval — do not run the whole thing unattended unless I say so.

Specialists this pipeline uses (each runs in its own context):
- requirements-analyst — turns the request into a testable spec
- explorer — maps relevant existing code
- architect — designs non-trivial structure
- edge-case-hunter — finds inputs that break things
- tester — writes and runs tests
- test-auditor — judges whether the tests are trustworthy
- verifier — proves the feature works by running it
- debugger — isolates and fixes failures (clean context)
- security-reviewer — checks the security lens
- reviewer / strict-reviewer — final quality gate


## Stage 1 — Align (no code yet)
- Invoke the requirements-analyst subagent to turn the feature request into
  a clear spec: scope, acceptance criteria, edge cases, interfaces, and
  open questions. It saves the spec to .claude/plans/spec-$1.md
- If the analyst surfaces blocking open questions, run the /grill-me flow
  or put those questions to me directly before proceeding.
- Summarize the agreed spec.
- CHECKPOINT: wait for my approval of the spec before building.

## Stage 2 — Plan
- Enter plan mode. Use the explorer subagent to map relevant code.
- Use the architect subagent for any non-trivial design.
- Produce an implementation plan: files, order, tests, risks.
- Save to .claude/plans/$1.md
- CHECKPOINT: wait for my approval of the plan before implementing.

## Stage 3 — Build
- Implement the plan. Write complete code, no placeholders.
- Commit incrementally with clear messages.
- Flag any point where reality diverged from the plan.

## Stage 4 — Find the Gaps
- Invoke the edge-case-hunter subagent on the new code.
- Report its findings.
- Address the unhandled edge cases that matter (ask me if any are
  judgment calls).

## Stage 5 — Test
- Invoke the tester subagent to write and run tests, including the
  edge cases surfaced in Stage 4.
- Then invoke the test-auditor subagent to judge whether those tests are
  actually trustworthy or just create false confidence.
- Address the test-auditor's findings — have the tester add or fix tests
  as needed. Repeat until the audit verdict is TRUSTWORTHY.
- Report results.

## Stage 6 — Verify
- Invoke the verifier subagent to PROVE the feature works by running it,
  not just reading it.
- Report the verification verdict.
- If it does not work, invoke the debugger subagent to find and fix the
  cause, then re-verify.

## Stage 7 — Harden
- If the feature handles user input, external data, auth, or sensitive
  operations, invoke the security-reviewer subagent.
- Address CRITICAL and HIGH findings.

## Stage 8 — Final Review
- Invoke the reviewer subagent (or strict-reviewer if this is a milestone)
  for a final pass.
- Address blocking issues.
- CHECKPOINT: present the final state and recommend whether to merge.
  I make the merge/push call.

## Throughout
- Keep .claude/memory/scratchpad.md updated with progress.
- If you get stuck or blocked, say so clearly rather than guessing.
- Note any reusable patterns that emerged as candidates for /extract-skill.

At the end, give me a short summary: what was built, what each specialist
found, what's left, and your honest assessment of whether it's ready.
