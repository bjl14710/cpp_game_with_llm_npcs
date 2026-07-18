---
description: Apply fixes from a strict-review, scoped by severity
---

Improve code based on a prior review's findings.

Input "$1" controls scope:
  - "must"        → fix only MUST-DO / blocker severity
  - "all"         → fix MUST-DO, then suggested improvements
  - (a file/path) → re-review that target first, then refine

1. Locate the most recent review findings. If a /strict-review or
   /review ran this session, use those ranked findings. If none exist,
   run /review on the target first to generate them.
2. Group the work by the severity the review assigned. Never silently
   promote a "suggestion" to a fix unless scope is "all".
3. For each item, state WHAT and WHY in one line before editing. Work
   MUST-DO items first, in impact order.
4. Preserve behavior — improvement, not redesign. Flag any fix that
   would change behavior as out-of-scope and leave it for a plan.
5. Run tests after. Report what was fixed and what was deliberately
   skipped (and why).

Engineer voice. The review judged severity already — respect it, don't
re-litigate it.
