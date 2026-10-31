---
name: github-access-state
description: GitHub PAT lives in ~/.gitlab-api.key (misleading name); GH_TOKEN in profile is a stale placeholder; no gh CLI; repo is bjl14710/algo_trading_practice
metadata: 
  node_type: memory
  type: project
  originSessionId: dab334e7-6f0e-462a-b515-30f6fe7db987
---

GitHub auth in this project (verified working 2026-07-05):
- The real GitHub PAT (`ghp_...`, 40 chars) is stored in `~/.gitlab-api.key` —
  the filename says gitlab but it IS the GitHub token.
- The `GH_TOKEN` env var that tool shells inherit from the profile is a stale
  5-char placeholder (`TOKEN`). Load the real one per-command:
  `export GH_TOKEN=$(cat ~/.gitlab-api.key | tr -d '[:space:]')`
- `gh` CLI is NOT installed — use `requests` (already a project dep) against the REST API.
- `git push` over https works independently via the osxkeychain credential helper.
- Repo renamed: `bjl14710/HFT_practice` → `bjl14710/algo_trading_practice`
  (old URL redirects; API calls must use the new name). Local `origin` may still
  point at the old URL.

**Why:** Two sessions were degraded by assuming GH_TOKEN was valid, and one by
assuming a profile edit would reach tool shells — it doesn't; shells re-init
from the profile file, and the stale export there wins.
**How to apply:** Prefix any GitHub API command with the export line above.
Never print the token; check auth with a `/user` call first.
