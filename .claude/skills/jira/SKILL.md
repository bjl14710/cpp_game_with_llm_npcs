---
description: Use this skill whenever you need to interact with Jira — reading tickets, creating tickets, updating status, adding comments, or linking epics. Read this before writing any Jira code.
---

# Jira Skill

This skill covers everything needed to read from and write to Jira
using the official Python `jira` library. Read it fully before writing
any Jira code — the API has non-obvious requirements that cause silent
failures if you don't know them.

Install if not already present:
```bash
pip install jira
```

---

## Auth and Client Setup

```python
import os
from jira import JIRA

def get_jira_client() -> JIRA:
    """
    Build an authenticated Jira client from environment variables.
    Required env vars:
        JIRA_SERVER      e.g. https://company.atlassian.net
        JIRA_EMAIL       your work email (Jira Cloud only)
        JIRA_API_TOKEN   from id.atlassian.com/manage-profile/security/api-tokens
    Optional:
        JIRA_PROJECT     default project key, e.g. SILM
        JIRA_ACCOUNT_ID  your accountId for assignee fields
    """
    server = os.environ.get("JIRA_SERVER", "").rstrip("/")
    email  = os.environ.get("JIRA_EMAIL", "")
    token  = os.environ.get("JIRA_API_TOKEN", "")

    missing = [k for k, v in {
        "JIRA_SERVER": server,
        "JIRA_EMAIL": email,
        "JIRA_API_TOKEN": token
    }.items() if not v]

    if missing:
        raise EnvironmentError(
            f"Missing env vars: {', '.join(missing)}\n"
            "Add to ~/.zshrc (Mac) or ~/.bashrc (Linux):\n"
            "  export JIRA_SERVER='https://company.atlassian.net'\n"
            "  export JIRA_EMAIL='you@company.com'\n"
            "  export JIRA_API_TOKEN='your-token'"
        )

    # Jira Cloud: basic_auth with email + API token
    return JIRA(server=server, basic_auth=(email, token))

    # For on-premise Data Center with Personal Access Token, use instead:
    # return JIRA(server=server, token_auth=token)
```

**Shell config** — add to `~/.zshrc` or `~/.bashrc`:
```bash
export JIRA_SERVER="https://your-company.atlassian.net"
export JIRA_EMAIL="your@email.com"
export JIRA_API_TOKEN="your-token-here"
export JIRA_PROJECT="YOUR-PROJECT-KEY"
export JIRA_ACCOUNT_ID="your-account-id"   # see "Finding Your Account ID" below
```

---

## READING TICKETS

### Read a Single Issue (full detail)

```python
def read_issue(jira: JIRA, issue_key: str) -> dict:
    """
    Read full details of one ticket.
    Returns a clean dict with all the fields you care about.

    Example:
        issue = read_issue(jira, "SILM-42")
        print(issue["summary"])
        print(issue["description"])
        print(issue["status"])
    """
    issue = jira.issue(issue_key)
    f = issue.fields

    return {
        "key":         issue.key,
        "summary":     f.summary,
        "description": _extract_text(f.description),
        "status":      f.status.name,
        "issuetype":   f.issuetype.name,
        "priority":    f.priority.name if f.priority else None,
        "assignee":    f.assignee.displayName if f.assignee else None,
        "reporter":    f.reporter.displayName if f.reporter else None,
        "labels":      [l for l in f.labels] if f.labels else [],
        "created":     str(f.created),
        "updated":     str(f.updated),
        "comments":    [
            {
                "author": c.author.displayName,
                "body":   c.body,
                "created": str(c.created)
            }
            for c in (jira.comments(issue_key) or [])
        ],
        "url": f"{jira.server_url}/browse/{issue.key}"
    }


def _extract_text(description) -> str:
    """
    Extract plain text from an ADF description object or return as-is
    if it's already a string (older Jira API versions).
    """
    if description is None:
        return ""
    if isinstance(description, str):
        return description
    # ADF format — walk the content tree
    parts = []
    def walk(node):
        if isinstance(node, dict):
            if node.get("type") == "text":
                parts.append(node.get("text", ""))
            for child in node.get("content", []):
                walk(child)
        elif isinstance(node, list):
            for item in node:
                walk(item)
    walk(description)
    return "\n".join(parts).strip()
```

### List Issues (batch read)

```python
def list_issues(jira: JIRA, jql: str, max_results: int = 50) -> list[dict]:
    """
    Search for issues using JQL and return a list of clean dicts.

    Common JQL patterns:
        Ready for overnight:
            'project = SILM AND labels = "ready-for-ai" AND status = "To Do"'
        My open tickets:
            'project = SILM AND assignee = currentUser() AND status != Done'
        Recently updated:
            'project = SILM AND updated >= -7d ORDER BY updated DESC'
        Everything open in project:
            'project = SILM AND status NOT IN ("Done", "Closed")'
        Blocked by Claude:
            'project = SILM AND labels = "ai-blocked"'

    Example:
        issues = list_issues(jira,
            'project = SILM AND labels = "ready-for-ai"')
        for i in issues:
            print(i["key"], i["summary"], i["status"])
    """
    results = jira.search_issues(jql, maxResults=max_results)
    return [
        {
            "key":       issue.key,
            "summary":   issue.fields.summary,
            "status":    issue.fields.status.name,
            "issuetype": issue.fields.issuetype.name,
            "assignee":  issue.fields.assignee.displayName
                         if issue.fields.assignee else None,
            "labels":    list(issue.fields.labels),
            "url":       f"{jira.server_url}/browse/{issue.key}"
        }
        for issue in results
    ]
```

### Get Available Transitions for an Issue

```python
def get_transitions(jira: JIRA, issue_key: str) -> list[dict]:
    """
    Get the status transitions available for an issue.
    Use this when you're not sure what status names your project uses.

    Example:
        transitions = get_transitions(jira, "SILM-42")
        for t in transitions:
            print(t["id"], t["name"])
        # 11  To Do
        # 21  In Progress
        # 31  Done
    """
    return [
        {"id": t["id"], "name": t["name"]}
        for t in jira.transitions(issue_key)
    ]
```

### Get Issue Types for a Project

```python
def get_issue_types(jira: JIRA) -> list[str]:
    """
    Get valid issue type names for your project.
    Use this if create_issue fails with a 400 on issuetype.

    Example:
        types = get_issue_types(jira)
        print(types)
        # ['Epic', 'Story', 'Task', 'Sub-task', 'Bug']
    """
    project_key = os.environ["JIRA_PROJECT"]
    project = jira.project(project_key)
    return [it.name for it in jira.project_issue_types(project_key)]
```

---

## CREATING TICKETS (UPLOADING)

### CRITICAL: Description Format

Jira API v3 requires Atlassian Document Format (ADF) for descriptions.
Plain strings cause a 400 error. Always use these helpers:

```python
def make_description(text: str) -> dict:
    """Plain text → ADF. Use for simple single-paragraph descriptions."""
    return {
        "type": "doc",
        "version": 1,
        "content": [{
            "type": "paragraph",
            "content": [{"type": "text", "text": text}]
        }]
    }


def make_description_sections(sections: dict) -> dict:
    """
    Dict of heading → body text → structured ADF description.

    Example:
        make_description_sections({
            "What to change": "Add SCPI support for...",
            "Where it is":    "src/devices/",
            "How to verify":  "pytest tests/ passes",
        })
    """
    content = []
    for heading, body in sections.items():
        content.append({
            "type": "heading",
            "attrs": {"level": 3},
            "content": [{"type": "text", "text": heading}]
        })
        content.append({
            "type": "paragraph",
            "content": [{"type": "text", "text": body}]
        })
    return {"type": "doc", "version": 1, "content": content}
```

### Create an Epic

```python
def create_epic(jira: JIRA, summary: str,
                description: str = "") -> str:
    """
    Create an Epic. Returns the issue key e.g. 'SILM-10'.

    Example:
        key = create_epic(jira,
            summary="[Epic] SCPI oscilloscope device family",
            description="Add SCPI support for Rigol and Keysight scopes.")
    """
    issue = jira.create_issue(fields={
        "project":     {"key": os.environ["JIRA_PROJECT"]},
        "summary":     summary,
        "description": make_description(description),
        "issuetype":   {"name": "Epic"},
    })
    print(f"Created Epic: {issue.key} — {summary}")
    return issue.key
```

### Create a Story / Task

```python
def create_story(jira: JIRA, summary: str,
                 sections: dict = None,
                 description: str = "",
                 labels: list = None,
                 assignee_id: str = None) -> str:
    """
    Create a Story (or Task — same call, change issuetype name if needed).
    Returns the issue key.

    Use sections dict for structured descriptions:
        sections = {
            "What to change": "...",
            "Where it is":    "...",
            "How to verify":  "...",
            "Constraints":    "...",
            "Branch name":    "feature/SILM-10-slug",
        }

    Or use description string for simple cases.

    Example:
        key = create_story(jira,
            summary="Add Rigol DS1054Z oscilloscope SCPI support",
            sections={...},
            labels=["ready-for-ai"],
            assignee_id=os.environ.get("JIRA_ACCOUNT_ID"))
    """
    fields = {
        "project":     {"key": os.environ["JIRA_PROJECT"]},
        "summary":     summary,
        "description": (make_description_sections(sections)
                        if sections else make_description(description)),
        "issuetype":   {"name": "Story"},
    }

    if labels:
        fields["labels"] = labels

    if assignee_id:
        fields["assignee"] = {"accountId": assignee_id}

    issue = jira.create_issue(fields=fields)
    print(f"Created Story: {issue.key} — {summary}")
    return issue.key
```

### Create a Sub-task

```python
def create_subtask(jira: JIRA, parent_key: str,
                   summary: str, description: str = "") -> str:
    """
    Create a Sub-task under a parent Story or Task.

    IMPORTANT: The issuetype string must be exactly "Sub-task"
    Capital S, hyphen, lowercase t.
    "subtask", "Subtask", "sub-task" all cause a 400 error.

    Example:
        key = create_subtask(jira, "SILM-10",
            summary="Write unit tests for DS1054Z",
            description="Cover mandatory commands and short forms.")
    """
    issue = jira.create_issue(fields={
        "project":     {"key": os.environ["JIRA_PROJECT"]},
        "summary":     summary,
        "description": make_description(description),
        "issuetype":   {"name": "Sub-task"},   # exact — do not change
        "parent":      {"key": parent_key},
    })
    print(f"Created Sub-task: {issue.key} under {parent_key}")
    return issue.key
```

### Link a Story to an Epic

```python
def link_to_epic(jira: JIRA, epic_key: str, issue_key: str):
    """
    Add an issue to an Epic.
    Tries two methods — one for Cloud, one for on-premise.

    Example:
        link_to_epic(jira, "SILM-10", "SILM-11")
    """
    # Method 1: update Epic Link custom field (Jira Cloud standard)
    try:
        jira.issue(issue_key).update(
            fields={"customfield_10014": epic_key}
        )
        print(f"Linked {issue_key} → Epic {epic_key}")
        return
    except Exception:
        pass

    # Method 2: JIRA Software epic endpoint
    try:
        jira.add_issues_to_epic(epic_key, [issue_key])
        print(f"Linked {issue_key} → Epic {epic_key}")
    except Exception as e:
        print(f"Warning: could not link {issue_key} to {epic_key}: {e}")
        print("Link manually in Jira UI if needed.")
```

---

## UPDATING TICKETS

### Transition Status

```python
def transition_issue(jira: JIRA, issue_key: str, target_status: str):
    """
    Move an issue to a new status by name.
    Status names vary by project workflow — use get_transitions() to
    see what's available if this fails.

    Common status names: "To Do", "In Progress", "In Review", "Done"

    Example:
        transition_issue(jira, "SILM-42", "In Progress")
    """
    transitions = jira.transitions(issue_key)
    match = next(
        (t for t in transitions
         if t["name"].lower() == target_status.lower()),
        None
    )
    if not match:
        available = [t["name"] for t in transitions]
        print(f"'{target_status}' not available for {issue_key}.")
        print(f"Available transitions: {available}")
        return

    jira.transition_issue(issue_key, match["id"])
    print(f"Transitioned {issue_key} → {target_status}")
```

### Add a Comment

```python
def add_comment(jira: JIRA, issue_key: str, body: str):
    """
    Add a plain text comment to an issue.

    Example:
        add_comment(jira, "SILM-42",
            "Starting overnight implementation. Branch: feature/SILM-42-ds1054z")
    """
    jira.add_comment(issue_key, body)
    print(f"Commented on {issue_key}")
```

### Add a Label

```python
def add_label(jira: JIRA, issue_key: str, label: str):
    """
    Add a label without removing existing labels.

    Example:
        add_label(jira, "SILM-42", "ai-in-progress")
    """
    issue = jira.issue(issue_key)
    existing = [l for l in issue.fields.labels]
    if label not in existing:
        issue.update(fields={"labels": existing + [label]})
        print(f"Added label '{label}' to {issue_key}")


def replace_label(jira: JIRA, issue_key: str,
                  remove: str, add: str):
    """
    Swap one label for another — used by overnight session
    to move tickets through the label workflow.

    Example:
        replace_label(jira, "SILM-42", "ready-for-ai", "ai-in-progress")
    """
    issue = jira.issue(issue_key)
    labels = [l for l in issue.fields.labels if l != remove]
    if add not in labels:
        labels.append(add)
    issue.update(fields={"labels": labels})
    print(f"{issue_key}: '{remove}' → '{add}'")
```

---

## FINDING YOUR ACCOUNT ID

The assignee field requires an `accountId`, not your email. Get it once:

```python
jira = get_jira_client()
print("Your account ID:", jira.current_user())
```

Then add to your shell config:
```bash
export JIRA_ACCOUNT_ID="61234abcd5678ef90123456"   # yours will look like this
```

---

## OVERNIGHT SESSION LABEL WORKFLOW

The Jira ticket states map to the overnight workflow like this:

```
To Do          ← you label ready-for-ai to queue it
    ↓
In Progress    ← Claude transitions here when starting work
    ↓
In Review      ← Claude transitions here when draft MR is open
                  YOU review the MR and merge it
    ↓
Done           ← YOU transition here after reviewing and merging
                  Claude never moves a ticket to Done
```

Claude labels on the Jira ticket mirror the GitHub label workflow:
  ready-for-ai    → queued for overnight
  ai-in-progress  → Claude is working on it
  ai-completed    → draft MR open, waiting for your review
  ai-blocked      → Claude got stuck, needs human attention
  needs-human     → issue too vague to attempt

Convenience functions for the overnight session:

```python
def mark_in_progress(jira, key):
    replace_label(jira, key, "ready-for-ai", "ai-in-progress")
    transition_issue(jira, key, "In Progress")
    add_comment(jira, key, "Overnight session starting on this ticket.")

def mark_completed(jira, key, pr_url, branch):
    """
    Called when Claude opens a draft MR for review.
    Moves to In Review — NOT Done.
    Done is only set by a human after reviewing and merging the MR.
    """
    replace_label(jira, key, "ai-in-progress", "ai-completed")
    transition_issue(jira, key, "In Review")
    add_comment(jira, key,
        f"Draft MR open for review: {pr_url}\n"
        f"Branch: `{branch}`\n\n"
        f"Awaiting your review. Move to Done after merging.")

def mark_blocked(jira, key, reason):
    replace_label(jira, key, "ai-in-progress", "ai-blocked")
    add_comment(jira, key, f"Blocked overnight: {reason}. Needs human review.")

def mark_needs_human(jira, key, reason):
    replace_label(jira, key, "ready-for-ai", "needs-human")
    add_comment(jira, key,
        f"Skipped overnight — too vague to attempt: {reason}. "
        f"Please clarify and re-label ready-for-ai.")
```

---

## ERROR REFERENCE

| Error | Cause | Fix |
|-------|-------|-----|
| 401 Unauthorized | Wrong token or email | Check JIRA_API_TOKEN and JIRA_EMAIL |
| 404 Not Found | Wrong server URL | Check JIRA_SERVER |
| 400 on description | Plain string not ADF | Use make_description() |
| 400 on issuetype | Wrong type string | Use exact: `"Story"`, `"Epic"`, `"Sub-task"` |
| 400 on assignee | Passed email not accountId | Use JIRA_ACCOUNT_ID env var |
| 403 Forbidden | No project permission | Ask Jira admin for access |
| Epic link fails | Wrong custom field ID | Try `customfield_10014` or ask admin |
| Transition fails | Wrong status name | Call get_transitions() to see options |

---

## COMPLETE WORKING EXAMPLE

```python
#!/usr/bin/env python3
"""
Example: create an Epic with two Stories, both assigned to you
and labelled ready-for-ai for the overnight session to pick up.
"""
import os
from jira import JIRA

# paste all functions from above here, or import them

def main():
    jira = get_jira_client()
    my_id = os.environ.get("JIRA_ACCOUNT_ID")

    # Create the Epic
    epic_key = create_epic(
        jira,
        summary="[Epic] SCPI oscilloscope device family",
        description="Add SCPI simulation support for Rigol and Keysight oscilloscopes."
    )

    # Create Story 1
    story1 = create_story(
        jira,
        summary="Add Rigol DS1054Z oscilloscope SCPI support",
        sections={
            "What to change": "New OscilloscopeDS1054Z class in src/devices/",
            "Where it is":    "Follow pattern in src/devices/psu.py",
            "How to verify":  "pytest tests/devices/test_ds1054z.py passes",
            "Constraints":    'IDN: "RIGOL TECHNOLOGIES,DS1054Z,SIM000001,00.04.04"',
            "Branch name":    f"feature/{epic_key}-rigol-ds1054z",
        },
        labels=["ready-for-ai"],
        assignee_id=my_id
    )

    # Create Story 2
    story2 = create_story(
        jira,
        summary="Add Keysight DSOX1204G oscilloscope SCPI support",
        sections={
            "What to change": "New OscilloscopeKeysightDSOX1204G class in src/devices/",
            "Where it is":    "Follow pattern in src/devices/psu.py",
            "How to verify":  "pytest tests/devices/test_dsox1204g.py passes",
            "Constraints":    'IDN: "Keysight,DSOX1204G,SIM000002,02.60"',
            "Branch name":    f"feature/{epic_key}-keysight-dsox1204g",
        },
        labels=["ready-for-ai"],
        assignee_id=my_id
    )

    # Link both to the Epic
    link_to_epic(jira, epic_key, story1)
    link_to_epic(jira, epic_key, story2)

    # Verify they exist
    print("\nVerifying created tickets:")
    for key in [epic_key, story1, story2]:
        issue = read_issue(jira, key)
        print(f"  {issue['key']} [{issue['status']}] {issue['summary']}")
        print(f"    Labels: {issue['labels']}")
        print(f"    URL: {issue['url']}")

if __name__ == "__main__":
    main()
```
