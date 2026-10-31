---
name: "source-command-review"
description: "Routine review of current uncommitted changes"
---

# source-command-review

Use this skill when the user asks to run the migrated source command `review`.

## Command Template

1. Run `git status` and `git diff` to see uncommitted changes
2. Invoke the reviewer subagent on these changes
3. Run any relevant tests that exist in the project
4. Report findings and test results
5. Recommend whether the changes are ready to commit

Do NOT commit anything yourself. I will review and commit manually.
