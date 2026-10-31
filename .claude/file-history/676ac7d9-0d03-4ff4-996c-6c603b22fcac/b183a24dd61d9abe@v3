---
name: github-auth-setup
description: "GitHub auth for Silmulator — username bjl14710, real token lives in TOKEN (not always GH_TOKEN), gh CLI installed via brew"
metadata: 
  node_type: memory
  type: project
  originSessionId: 83ba6f4a-e295-487a-af36-6212fe2d18e8
---

GitHub setup for the Silmulator repo (github.com/bjl14710/Silmulator, default branch `master`):

- Username: **bjl14710** (`GITHUB_USERNAME` exported in ~/.zshrc line 6).
- The real GitHub token (`ghp_…`, 40 chars) is read from `~/.gitlab-api.key` (misleading filename — it is a GitHub token) into both `TOKEN` and `GH_TOKEN` by ~/.zshrc lines 2 and 5.
- A session's inherited `GH_TOKEN` may be the stale literal string "TOKEN" (old typo'd export; ~/.zshrc only loads in interactive shells). Before trusting it, compare: if `GH_TOKEN` ≠ `TOKEN`, prefix gh calls with `GH_TOKEN="$TOKEN" gh …`.
- `gh` CLI installed 2026-07-05 via Homebrew (v2.96.0); verified `gh auth status` shows bjl14710.
- The token is a classic PAT with very broad scopes (admin:org, delete_repo, …) — flagged to the user that a fine-grained token would be safer.
- Branch layout: the mainline is `master`. An orphan `main` stub (empty tree, no shared history) used to exist and caused PR-targeting failures; deleted 2026-07-08 with the user's approval. When the user says "main", they mean `master`.
