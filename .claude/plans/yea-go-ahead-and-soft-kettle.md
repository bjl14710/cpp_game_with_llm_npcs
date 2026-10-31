# Delete the orphan `main` branch

## Context
The repo `bjl14710/Silmulator` has two "mainline" branches: `master` (the GitHub
default branch, full app history) and `main` (an orphan stub). The orphan `main`
has repeatedly caused confusion — overnight-session issues said "base: main",
and yesterday GitHub refused a PR into it ("no history in common"), forcing a
retarget to `master`. The user confirmed `master` is the real mainline and asked
to delete the orphan `main`.

## Safety checks (already done, all clean)
- `origin/main` = single commit `c7ac201` ("Initial commit", 2026-05-21) with an
  **empty tree** — zero files, nothing to preserve.
- No PRs (open or closed) target or originate from `main`.
- `main` is not protected, and it is not the default branch (`master` is).
- No `.github/` workflows exist anywhere, so no CI references `main`.
- No local `main` branch exists — only the remote-tracking ref `origin/main`.

## Steps
1. Delete the remote branch: `git push origin --delete main`
   (run with `GH_TOKEN`-independent git auth — the remote is HTTPS; if git
   prompts for credentials, fall back to
   `gh api -X DELETE repos/bjl14710/Silmulator/git/refs/heads/main`
   with `GH_TOKEN="$TOKEN"` per the memory note).
2. Prune the stale remote-tracking ref locally: `git fetch --prune origin`.
3. Update the memory file
   `~/.claude/projects/.../memory/github-auth-setup.md` — change the
   branch-layout gotcha line to record that orphan `main` was deleted
   2026-07-08 and `master` is the only mainline.

## Verification
- `git branch -a` shows no `origin/main`.
- `GH_TOKEN="$TOKEN" gh api repos/bjl14710/Silmulator/branches/main` returns 404.
- `gh repo view --json defaultBranchRef` still reports `master`.
- PRs #12 and #24 remain OPEN and MERGEABLE (untouched by the deletion).
