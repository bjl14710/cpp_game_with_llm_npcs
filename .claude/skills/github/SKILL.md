---
description: Use this skill whenever you need to interact with GitHub — reading issues, creating issues, opening draft PRs, managing labels, or commenting. Read this before writing any GitHub API code.
---

# GitHub Skill

This skill covers everything needed to read from and write to GitHub
using the PyGithub library. Read it fully before writing any GitHub
code — there are non-obvious requirements (branch must exist on remote
before PR creation, label replace needs add+remove not set, etc.)
that cause failures if you don't know them.

Install if not already present:
```bash
pip install PyGithub
```

---

## Auth and Client Setup

```python
import os
import subprocess
from github import Github, GithubException

def get_github_client() -> Github:
    """
    Build an authenticated GitHub client from environment variables.

    Required env vars:
        GH_TOKEN    Personal access token with repo, read:org, gist scopes
                    (same token used by the gh CLI)

    Optional env vars:
        GITHUB_USERNAME    your GitHub login — used for assignee
        GITHUB_REPO        owner/repo string e.g. "brandon/Silmulator"
                           if not set, auto-detected from git remote
    """
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")

    if not token:
        raise EnvironmentError(
            "Missing GH_TOKEN env var.\n"
            "Add to ~/.zshrc or ~/.bashrc:\n"
            "  export GH_TOKEN='ghp_your_token_here'\n"
            "Create a token at: github.com/settings/tokens\n"
            "Required scopes: repo, read:org, gist"
        )

    return Github(token)


def get_repo(gh: Github, repo_slug: str = None):
    """
    Get a repository object. Auto-detects from git remote if not provided.

    repo_slug format: "owner/repo-name"  e.g. "brandon/Silmulator"

    Example:
        gh = get_github_client()
        repo = get_repo(gh)              # auto-detect from git remote
        repo = get_repo(gh, "brandon/Silmulator")  # explicit
    """
    if repo_slug:
        return gh.get_repo(repo_slug)

    # Auto-detect from git remote
    env_slug = os.environ.get("GITHUB_REPO")
    if env_slug:
        return gh.get_repo(env_slug)

    # Parse from git remote URL
    try:
        result = subprocess.run(
            ["git", "remote", "get-url", "origin"],
            capture_output=True, text=True, check=True
        )
        url = result.stdout.strip()
        # Handle both SSH and HTTPS formats
        # git@github.com:owner/repo.git → owner/repo
        # https://github.com/owner/repo.git → owner/repo
        slug = url.replace("git@github.com:", "") \
                  .replace("https://github.com/", "") \
                  .rstrip("/") \
                  .removesuffix(".git")
        return gh.get_repo(slug)
    except Exception as e:
        raise EnvironmentError(
            f"Could not detect repo from git remote: {e}\n"
            "Set GITHUB_REPO env var: export GITHUB_REPO='owner/repo'"
        )
```

**Shell config** — add to `~/.zshrc` or `~/.bashrc`:
```bash
export GH_TOKEN="ghp_your_token_here"
export GITHUB_USERNAME="your-github-username"
export GITHUB_REPO="your-username/repo-name"   # optional — auto-detected if not set
```

**Token scopes required:** `repo`, `read:org`, `gist`
Create at: github.com/settings/tokens → Generate new token (classic)

**Enterprise GitHub:** use your enterprise URL:
```python
from github import Github
gh = Github(base_url="https://github.company.com/api/v3", auth=token)
```

---

## READING ISSUES

### Read a Single Issue

```python
def read_issue(repo, issue_number: int) -> dict:
    """
    Read full details of one issue including all comments.

    Example:
        gh = get_github_client()
        repo = get_repo(gh)
        issue = read_issue(repo, 42)
        print(issue["title"])
        print(issue["body"])
        print(issue["labels"])
    """
    issue = repo.get_issue(issue_number)

    return {
        "number":    issue.number,
        "title":     issue.title,
        "body":      issue.body or "",
        "state":     issue.state,          # "open" or "closed"
        "labels":    [l.name for l in issue.labels],
        "assignees": [a.login for a in issue.assignees],
        "created":   str(issue.created_at),
        "updated":   str(issue.updated_at),
        "comments":  [
            {
                "author": c.user.login,
                "body":   c.body,
                "created": str(c.created_at)
            }
            for c in issue.get_comments()
        ],
        "url":       issue.html_url
    }
```

### List Issues (batch read)

```python
def list_issues(repo, label: str = None,
                state: str = "open",
                limit: int = 50) -> list[dict]:
    """
    List issues, optionally filtered by label.

    Common label filters:
        "ready-for-ai"    tonight's work queue
        "ai-blocked"      needs your attention
        "needs-human"     Claude couldn't attempt these
        "ai-completed"    done, PRs open for review

    Example:
        gh = get_github_client()
        repo = get_repo(gh)

        # Tonight's queue
        issues = list_issues(repo, label="ready-for-ai")

        # Everything blocked
        issues = list_issues(repo, label="ai-blocked")

        # All open issues
        issues = list_issues(repo)

        for i in issues:
            print(i["number"], i["title"], i["labels"])
    """
    kwargs = {"state": state}
    if label:
        kwargs["labels"] = [repo.get_label(label)]

    results = repo.get_issues(**kwargs)

    output = []
    for issue in results:
        # Skip pull requests (GitHub returns PRs in issues endpoint)
        if issue.pull_request:
            continue
        output.append({
            "number":  issue.number,
            "title":   issue.title,
            "labels":  [l.name for l in issue.labels],
            "state":   issue.state,
            "url":     issue.html_url
        })
        if len(output) >= limit:
            break

    return output
```

### List Draft PRs (for morning review)

```python
def list_draft_prs(repo) -> list[dict]:
    """
    List all open draft PRs — your morning review queue.

    Example:
        prs = list_draft_prs(repo)
        for pr in prs:
            print(pr["number"], pr["title"], pr["url"])
    """
    return [
        {
            "number": pr.number,
            "title":  pr.title,
            "head":   pr.head.ref,       # branch name
            "base":   pr.base.ref,       # target branch
            "draft":  pr.draft,
            "url":    pr.html_url,
            "body":   pr.body or ""
        }
        for pr in repo.get_pulls(state="open")
        if pr.draft
    ]
```

---

## CREATING ISSUES

```python
def create_issue(repo, title: str,
                 body: str,
                 labels: list[str] = None,
                 assignees: list[str] = None) -> dict:
    """
    Create a new GitHub issue. Returns a dict with number and URL.

    body: plain markdown string — GitHub renders it natively.
    labels: must already exist in the repo. Create them first if needed.
    assignees: list of GitHub usernames (not display names).

    Example:
        gh = get_github_client()
        repo = get_repo(gh)
        username = os.environ["GITHUB_USERNAME"]

        issue = create_issue(
            repo,
            title="Add Rigol DS1054Z oscilloscope SCPI support",
            body='''## What to change
New OscilloscopeDS1054Z class in src/devices/

## Where it is
Follow pattern in src/devices/psu.py

## How to verify
pytest tests/devices/test_ds1054z.py passes.
Short forms work. *RST restores defaults.

## Constraints
IDN string: "RIGOL TECHNOLOGIES,DS1054Z,SIM000001,00.04.04"

## Branch name
`feature/issue-42-rigol-ds1054z`
''',
            labels=["ready-for-ai"],
            assignees=[username]
        )
        print(f"Created issue #{issue['number']}: {issue['url']}")
    """
    kwargs = {"title": title, "body": body}
    if labels:
        kwargs["labels"] = labels
    if assignees:
        kwargs["assignees"] = assignees

    issue = repo.create_issue(**kwargs)
    print(f"Created issue #{issue.number}: {issue.title}")
    return {"number": issue.number, "url": issue.html_url}
```

---

## MANAGING LABELS

### Create Repo Labels (one-time setup)

```python
def create_overnight_labels(repo):
    """
    Create the five overnight workflow labels in a repo.
    Safe to run multiple times — skips labels that already exist.

    Run once per repo before first overnight session.
    """
    labels = [
        ("ready-for-ai",   "0075ca", "Scoped and clear — safe for overnight AI run"),
        ("ai-in-progress", "e4e669", "Claude is currently working on this"),
        ("ai-completed",   "0e8a16", "AI finished — awaiting human review"),
        ("ai-blocked",     "d93f0b", "AI attempted but could not complete"),
        ("needs-human",    "cc317c", "Requires a human decision before work starts"),
    ]

    for name, color, description in labels:
        try:
            repo.create_label(name=name, color=color, description=description)
            print(f"Created label: {name}")
        except GithubException as e:
            if e.status == 422:   # already exists
                print(f"Label exists (skipped): {name}")
            else:
                raise
```

### Add / Remove Labels on an Issue

```python
def add_label(repo, issue_number: int, label: str):
    """Add a label to an issue without touching existing labels."""
    issue = repo.get_issue(issue_number)
    issue.add_to_labels(label)
    print(f"Added label '{label}' to #{issue_number}")


def remove_label(repo, issue_number: int, label: str):
    """Remove a label from an issue."""
    issue = repo.get_issue(issue_number)
    try:
        issue.remove_from_labels(label)
        print(f"Removed label '{label}' from #{issue_number}")
    except GithubException as e:
        if e.status == 404:
            print(f"Label '{label}' not on #{issue_number} — skipping")
        else:
            raise


def replace_label(repo, issue_number: int,
                  remove: str, add: str):
    """
    Swap one label for another — the overnight session label workflow.

    Example:
        # When Claude starts working on an issue
        replace_label(repo, 42, "ready-for-ai", "ai-in-progress")

        # When Claude completes it
        replace_label(repo, 42, "ai-in-progress", "ai-completed")

        # When Claude gets blocked
        replace_label(repo, 42, "ai-in-progress", "ai-blocked")
    """
    remove_label(repo, issue_number, remove)
    add_label(repo, issue_number, add)
```

---

## COMMENTS

```python
def add_comment(repo, issue_number: int, body: str):
    """
    Add a comment to an issue or PR (both use the same endpoint).

    Example:
        add_comment(repo, 42,
            "Starting overnight implementation. "
            "Branch: feature/issue-42-rigol-ds1054z")
    """
    issue = repo.get_issue(issue_number)
    issue.create_comment(body)
    print(f"Commented on #{issue_number}")
```

---

## CREATING DRAFT PRs

### CRITICAL: Branch Must Exist on Remote First

The branch MUST be pushed to GitHub before calling `create_pull`.
If you try to create a PR for a branch that only exists locally, you
get a 422 error: "head sha can't be blank".

```bash
# Always push the branch before creating the PR
git push origin feature/issue-42-rigol-ds1054z
```

Then create the PR:

```python
def create_draft_pr(repo, title: str, body: str,
                    head_branch: str,
                    base_branch: str = "main") -> dict:
    """
    Create a draft PR. Branch must already be pushed to remote.

    head_branch: the feature branch name e.g. "feature/issue-42-slug"
    base_branch: target branch, almost always "main"

    Example:
        pr = create_draft_pr(
            repo,
            title="fix(#42): add Rigol DS1054Z oscilloscope SCPI support",
            body=mr_description_output,
            head_branch="feature/issue-42-rigol-ds1054z"
        )
        print(f"Draft PR #{pr['number']}: {pr['url']}")
    """
    pr = repo.create_pull(
        title=title,
        body=body,
        head=head_branch,
        base=base_branch,
        draft=True             # creates as draft — human must approve to merge
    )
    print(f"Created draft PR #{pr.number}: {pr.html_url}")
    return {
        "number": pr.number,
        "url":    pr.html_url,
        "title":  pr.title
    }
```

### Closing the Issue When the PR Merges — READ THIS FIRST

GitHub only honours closing keywords (`Closes`, `Fixes`, `Resolves`) when
the PR's base is the repo's **DEFAULT branch**. In this repo every feature
PR targets `dev`, so the keyword **silently does nothing** — it reads as a
cross-reference and nothing more. This is not hypothetical: no issue in
this repo had ever auto-closed, and a 2026-08 audit found six issues whose
work was long since merged still sitting open in the queue (#165 #166 #199
#216 #256 #258), plus four of thirty "ready" issues that were already
built. An open queue that lies is worse than no queue.

So: **after a PR merges to dev, close its issue by hand, with evidence.**

```python
def close_merged_issue(repo, issue_number: int, merge_commit: str,
                       pr_number: int, evidence: str = ""):
    """
    Close an issue whose implementing PR has merged to dev, leaving an
    evidence trail. Call ONLY after verifying the merge commit is on
    origin/dev — a file merely existing is not proof the work is done
    (a scaffold with no caller looks identical to a finished feature).
    """
    issue = repo.get_issue(issue_number)
    issue.create_comment(
        f"Closing: merged to `dev` via PR #{pr_number} "
        f"(merge commit {merge_commit}). {evidence}")
    issue.edit(state="closed")
```

`Closes #N` in the PR body is still worth writing — as a visible
cross-reference linking PR to issue — just never as the close mechanism:

```python
def make_pr_body(mr_description: str, closes_issue: int) -> str:
    """
    Append an issue reference to a PR body. On a dev-based PR this LINKS
    the issue; it does NOT close it — use close_merged_issue after merge.
    """
    return f"{mr_description}\n\n---\nCloses #{closes_issue}"
```

---

## OVERNIGHT SESSION WORKFLOW HELPERS

These wrap the label/comment operations for the overnight session:

```python
def mark_in_progress(repo, issue_number: int, branch: str):
    """Called when Claude starts working on an issue."""
    replace_label(repo, issue_number, "ready-for-ai", "ai-in-progress")
    add_comment(repo, issue_number,
        f"Overnight session starting. Branch: `{branch}`")


def mark_completed(repo, issue_number: int,
                   pr_number: int, pr_url: str, branch: str):
    """Called when Claude opens a draft PR for the issue.

    NOTE: this does not end the issue's life. Because PRs here base on
    dev, `Closes #N` never fires — once the owner merges the PR, call
    close_merged_issue (see the PR section above) or the issue sits open
    forever looking like unfinished work.
    """
    replace_label(repo, issue_number, "ai-in-progress", "ai-completed")
    add_comment(repo, issue_number,
        f"Implemented overnight. Draft PR: #{pr_number} ({pr_url})\n"
        f"Branch: `{branch}`\n"
        f"Awaiting your review.")


def mark_blocked(repo, issue_number: int, reason: str):
    """Called when Claude fails after 2 attempts."""
    replace_label(repo, issue_number, "ai-in-progress", "ai-blocked")
    add_comment(repo, issue_number,
        f"Blocked overnight: {reason}\n\n"
        f"Needs human review before re-queuing.")


def mark_needs_human(repo, issue_number: int, reason: str):
    """Called when an issue is too vague to attempt."""
    replace_label(repo, issue_number, "ready-for-ai", "needs-human")
    add_comment(repo, issue_number,
        f"Skipped overnight — could not attempt autonomously:\n\n"
        f"{reason}\n\n"
        f"Clarify and re-label `ready-for-ai` to re-queue.")
```

---

## COMPLETE WORKING EXAMPLE

```python
#!/usr/bin/env python3
"""
Example: plan and create three GitHub issues for the Silmulator,
all labelled ready-for-ai and assigned to you.
Then verify they appear in the queue.
"""
import os
from github import Github, GithubException

# (paste helper functions from above here, or import them)

def main():
    gh    = get_github_client()
    repo  = get_repo(gh)
    me    = os.environ["GITHUB_USERNAME"]

    # One-time setup — create labels if they don't exist
    create_overnight_labels(repo)

    # Create three issues for the oscilloscope epic
    issues_to_create = [
        {
            "title": "Add Rigol DS1054Z oscilloscope SCPI support",
            "body": """## What to change
New `OscilloscopeDS1054Z` class in `src/devices/`.

## Where it is
Follow the pattern in `src/devices/psu.py`.

## How to verify
`pytest tests/devices/test_ds1054z.py` passes.
Short forms work (`MEAS:VOLT:MAX?` and `MEASure:VOLTage:MAXimum?` identical).
`*RST` restores all defaults.

## Constraints
IDN string: `"RIGOL TECHNOLOGIES,DS1054Z,SIM000001,00.04.04"`
Do not modify the base oscilloscope class.

## Branch name
`feature/issue-N-rigol-ds1054z`
"""
        },
        {
            "title": "Add oscilloscope trigger subsystem (TRIG commands)",
            "body": """## What to change
Add `TRIG` subsystem commands to the DS1054Z class.

## Key commands
- `TRIG:MODE EDGE|PULS|RUNT|WIND|NEDG|SLOP|VID|PATT`
- `TRIG:EDGE:SOUR CHAN1|CHAN2|CHAN3|CHAN4|EXT|AC`
- `TRIG:EDGE:SLOP POS|NEG|RFAL`
- `TRIG:EDGE:LEV <value>` and `TRIG:EDGE:LEV?`

## How to verify
`pytest tests/devices/test_ds1054z_trigger.py` passes.
Short forms work.

## Constraints
Requires `Add Rigol DS1054Z oscilloscope SCPI support` to be merged first.

## Branch name
`feature/issue-N-ds1054z-trigger`
"""
        },
        {
            "title": "Add oscilloscope GUI block for canvas",
            "body": """## What to change
New `OscilloscopeBlock` class in `src/gui/blocks/`.

## Where it is
Follow the pattern in `src/gui/blocks/psu_block.py`.

## How to verify
Block appears in the device palette.
Drag-drop onto canvas works.
`to_dict()` / `from_dict()` round-trip produces identical state.

## Constraints
Do not modify the base block class.
Block must have input trigger port and output waveform port.

## Branch name
`feature/issue-N-oscilloscope-gui-block`
"""
        }
    ]

    created = []
    for spec in issues_to_create:
        result = create_issue(
            repo,
            title=spec["title"],
            body=spec["body"],
            labels=["ready-for-ai"],
            assignees=[me]
        )
        # Update branch name in body now that we have the issue number
        issue = repo.get_issue(result["number"])
        updated_body = spec["body"].replace(
            "feature/issue-N",
            f"feature/issue-{result['number']}"
        )
        issue.edit(body=updated_body)
        created.append(result)

    # Verify they appear in the ready-for-ai queue
    print("\nVerifying tonight's queue:")
    queue = list_issues(repo, label="ready-for-ai")
    for item in queue:
        print(f"  #{item['number']} {item['title']}")
        print(f"  Labels: {item['labels']}")
        print(f"  URL: {item['url']}")
        print()

if __name__ == "__main__":
    main()
```

---

## ERROR REFERENCE

| Error | Cause | Fix |
|-------|-------|-----|
| 401 Unauthorized | Wrong or expired token | Check GH_TOKEN, regenerate if expired |
| 403 Forbidden | Missing scope | Regenerate token with repo, read:org, gist scopes |
| 403 on org repo | SSO not authorized | github.com/settings/tokens → Configure SSO → Authorize |
| 404 on repo | Wrong owner/repo slug | Check GITHUB_REPO env var format: "owner/repo" |
| 404 on label | Label doesn't exist | Run create_overnight_labels(repo) first |
| 422 on PR creation | Branch not on remote | Run git push origin branch-name before create_pull |
| 422 on PR creation | PR already exists for branch | Check repo.get_pulls() first |
| 422 on issue creation | Assignee not a collaborator | User must have repo access to be assigned |

---

## INSTALL

```bash
pip install PyGithub
```

Shell config (`~/.zshrc` or `~/.bashrc`):
```bash
export GH_TOKEN="ghp_your_token_here"
export GITHUB_USERNAME="your-github-login"
export GITHUB_REPO="owner/repo-name"    # optional — auto-detected from git remote
```

Token scopes: `repo`, `read:org`, `gist`
Create at: github.com/settings/tokens → Generate new token (classic)
