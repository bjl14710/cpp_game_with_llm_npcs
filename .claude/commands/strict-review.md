---
description: Strict adversarial review for milestones, releases, or risky changes
---

This is a high-stakes review. Use the strict-reviewer subagent.

1. Determine what's being reviewed:
   - If $1 is provided, review that branch or PR
   - Otherwise, review all uncommitted changes plus recent commits on
     this branch (`git diff main` if on a feature branch)

2. Invoke the strict-reviewer subagent

3. Run all available tests, lints, type checks, and security scans
   (pytest, npm test, mypy, eslint, bandit, etc. — whatever applies)

4. Report the strict-reviewer's full output verbatim

5. Add a final summary:
   - Test results
   - Lint results
   - Overall recommendation: SHIP / HOLD / REJECT

Do NOT commit, merge, or push anything. I make the final call.
