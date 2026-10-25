---
name: weekly-digest
disable-model-invocation: true
description: Cross-repo Sunday digest — gather every routine's output across all repos in ~/.claude/routines/repos.txt into one ranked list with the 3–5 things most worth your time this week.
argument-hint: Optional — number of days, default 7
---

Load the `routine-guardrails` skill first. Run this from any directory; it works across repos via `gh`.

1. Read `~/.claude/routines/repos.txt`, or in a cloud session `.claude/project/repos.txt` (one `owner/repo` per line; `#` comments allowed). A cloud run can only reach the repos selected on its routine, so list exactly those.
2. Delegate collection to the `digest-writer` subagent: for each repo, in the last $ARGUMENTS days (default 7): new `proposed`/`bug`/`research`/`design`/`teach` issues, open `preview-ready` draft PRs, merged PRs, the latest `digest` issue's health table, and any failing CI on the default branch.
3. Rank across all repos into one list:
   - **Decide now** (≤5): previews ready to merge/reject, bugs with constraint violations, anything blocking a DIRECTION.md phase exit criterion.
   - **Worth a look**: the best remaining proposals.
   - **Learn this week**: top 3 concepts from `teach` issues → exact /teach or /drill-me commands.
   - **Health**: one line per repo with trend arrows.
   - **Stale**: proposals >30 days old (suggest bulk-defer in /triage).
4. Post it as an issue titled `digest: week of YYYY-MM-DD` in the repo listed first in repos.txt (your "home" repo), labeled `digest`, and print it. Close the previous week's digest issue with a link to the new one.
5. End with the exact command for your decision session: `/triage all-repos`.
