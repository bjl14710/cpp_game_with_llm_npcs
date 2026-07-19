---
description: Use this skill whenever you need to interact with GitLab — reading issues, creating merge requests, managing labels, reading MR comments, or CI/CD pipelines. Read before writing any GitLab commands.
---

# GitLab Skill

This skill covers everything needed to interact with GitLab using
the glab CLI. Read it fully before running any GitLab commands.
glab auto-detects the GitLab instance from your git remote.

## Install and Auth

Mac:    brew install glab
Ubuntu: sudo apt install glab

Authenticate:
  glab auth login                              (interactive, has browser)
  glab auth login --hostname host --stdin < token.txt  (headless/EC2)
  export GITLAB_TOKEN="your-pat"               (env var, auto-used)

Required token scopes: api and write_repository
Create at: gitlab.company.com/-/user_settings/personal_access_tokens

Shell config (~/.bashrc or ~/.zshrc):
  export GITLAB_TOKEN="your-pat-here"
  export GITLAB_USERNAME="your-gitlab-username"

Set default host for self-managed (do once):
  glab config set host gitlab.company.com

## KEY DIFFERENCES FROM GITHUB

GitHub (gh)             GitLab (glab)
Pull Request            Merge Request (MR)
gh pr                   glab mr
gh pr edit --add-label  glab mr update --label
gh run                  glab pipeline / glab ci

## READING ISSUES

# List ready-for-ai issues
glab issue list --label "ready-for-ai" --output json

# Read one issue
glab issue view 42 --output json

# List your assigned issues
glab issue list --assignee @me --output json

## CREATING ISSUES

glab issue create \
  --title "Add DS1054Z oscilloscope SCPI support" \
  --description "$(cat issue-body.md)" \
  --label "ready-for-ai" \
  --assignee @me

## CREATING MERGE REQUESTS

CRITICAL: Push the branch to remote BEFORE creating the MR.

  git push origin feature/issue-42-ds1054z

  glab mr create \
    --title "fix(#42): add DS1054Z SCPI support" \
    --description "$(cat mr-description.md)

  Closes #42" \
    --label "ai-completed" \
    --assignee @me \
    --target-branch main \
    --draft \
    --remove-source-branch

"Closes #42" in the description auto-closes the issue on merge.

## READING MR COMMENTS (for address-review-comments)

# Get MR overview
glab mr view 42 --output json

# Get all discussion threads (inline review comments)
glab api "projects/:fullpath/merge_requests/42/discussions" \
  --paginate | python3 -m json.tool

# Get all general notes/comments
glab api "projects/:fullpath/merge_requests/42/notes" \
  --paginate | python3 -m json.tool

Discussions = inline code review comments (have file + line info)
Notes = general MR conversation comments

## REPLYING TO MR COMMENTS

# Reply to a discussion thread
glab api "projects/:fullpath/merge_requests/42/discussions/DISC_ID/notes" \
  --method POST \
  --field "body=Fixed in latest commit."

# Resolve a discussion thread
glab api "projects/:fullpath/merge_requests/42/discussions/DISC_ID" \
  --method PUT \
  --field "resolved=true"

# Add general MR comment
glab mr comment 42 --body "Review comments addressed. Ready for another look."

## MANAGING LABELS

# Swap labels on an issue
glab issue update 42 --unlabel "ready-for-ai" --label "ai-in-progress"

# Add label to MR
glab mr update 42 --label "ai-completed"

Create overnight labels (one-time per project):
  for name in "ready-for-ai:0075ca" "ai-in-progress:e4e669" "ai-completed:0e8a16" "ai-blocked:d93f0b" "needs-human:cc317c"; do
    label="${name%%:*}"
    color="${name##*:}"
    glab api "projects/:fullpath/labels" --method POST \
      --field "name=$label" --field "color=#$color"
  done

## OVERNIGHT LABEL WORKFLOW

Starting:
  glab issue update N --unlabel "ready-for-ai" --label "ai-in-progress"
  glab issue comment N --body "Starting overnight implementation."

Completed:
  glab issue update N --unlabel "ai-in-progress" --label "ai-completed"
  glab issue comment N --body "Implemented. MR: !MR_NUMBER"
  glab issue close N --note "Implemented in MR above."

Blocked:
  glab issue update N --unlabel "ai-in-progress" --label "ai-blocked"
  glab issue comment N --body "Blocked: [reason]. Needs human review."

Too vague:
  glab issue update N --unlabel "ready-for-ai" --label "needs-human"
  glab issue comment N --body "Skipped: too vague. Please clarify."

## CI/CD PIPELINE COMMANDS

# Check pipeline status for current branch
glab pipeline list --ref $(git branch --show-current) --output json

# View job logs (strip ANSI color codes)
glab pipeline job-log JOB_ID | sed 's/\x1b\[[0-9;]*m//g'

# Retry a failed pipeline
glab pipeline retry PIPELINE_ID

## AUTOMATION RULES

- Always use --output json in scripts (plain text is not parseable)
- Prefer native glab commands over glab api (handles URL encoding)
- Strip ANSI codes from logs: | sed 's/\x1b\[[0-9;]*m//g'
- Never use glab ci view in scripts (TUI crashes in non-interactive env)
- glab auto-detects repo — only use -R owner/repo outside a repo dir

## ERROR REFERENCE

401 Unauthorized    → token expired, re-run glab auth login
404 Not Found       → wrong project path, check git remote -v
Wrong host          → glab config set host gitlab.company.com
MR not found        → in a fork, use glab mr view -R namespace/project
TUI crash           → use glab pipeline list --output json instead
