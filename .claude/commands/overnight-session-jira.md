---
description: Work through tonight's ready-for-ai Jira tickets in a GitLab repo. Each ticket gets its own branch and draft MR containing only code changes. Tickets move to In Review (not Done) when the MR opens — Done is set by you after merging. Learning materials are written locally and never committed.
argument-hint: Optional focus area (blank = all ready-for-ai tickets)
---

You are running an overnight autonomous development session on this
repository. Use the overnight-coordinator agent to manage the session.

Before doing anything else, read these skills:
- .claude/skills/jira/SKILL.md       — all Jira ticket operations
- .claude/skills/gitlab/SKILL-gitlab.md — all GitLab MR operations
- .claude/skills/code-economy/SKILL.md — implementation standards

This is a Jira + GitLab environment:
- Issues live in Jira — use the Jira skill (Python jira library)
- MRs live in GitLab — use glab commands
- Never use gh — this is not a GitHub repo

---

## IMPORTANT — WHAT GETS COMMITTED AND WHAT DOES NOT

On feature branches: source code and tests ONLY.

Never committed anywhere:
- docs/learning/  (lessons, drills, vocab, index)
- OVERNIGHT_REPORT.md
- .claude/memory/ files
- .claude/plans/ files

Verify .gitignore covers these and add if missing:
```bash
for path in "docs/learning/" "OVERNIGHT_REPORT.md" ".claude/memory/" ".claude/plans/"; do
  grep -qF "$path" .gitignore || echo "$path" >> .gitignore
done
git diff --name-only | grep -q ".gitignore" && \
  git add .gitignore && \
  git commit -m "chore: exclude learning docs and overnight artifacts from git" && \
  git push origin main || true
```

---

## TICKET STATE RULES

Claude moves tickets through these states:
  To Do → In Progress → In Review

Claude NEVER moves a ticket to Done.
Done is set by YOU after reviewing and merging the MR.

```
To Do          ← you labelled ready-for-ai to queue it
    ↓
In Progress    ← Claude transitions here when starting work
    ↓
In Review      ← Claude transitions here when draft MR is open
    ↓
Done           ← YOU set this after reviewing and merging
```

---

## SETUP (run once at session start)

```bash
# 1. Start on main, up to date
git checkout main && git pull

# 2. Rebuild knowledge graph if installed
graphify update . --force 2>/dev/null || true

# 3. Resume prior run if checkpoint exists
cat .claude/memory/checkpoint.md 2>/dev/null || echo "Fresh session"
```

Read tonight's Jira ticket queue using the Jira skill:
```python
from jira import JIRA
import os

jira = get_jira_client()
project = os.environ["JIRA_PROJECT"]

tickets = list_issues(jira,
    f'project = {project} AND labels = "ready-for-ai" AND status = "To Do"')

print(f"Tonight's queue: {len(tickets)} tickets")
for t in tickets:
    print(f"  {t['key']} {t['summary']}")
```

Also check for plan documents that match tonight's tickets:
```bash
ls .claude/plans/ 2>/dev/null || echo "No plan docs"
```

Write session start to .claude/memory/checkpoint.md:
```
# Checkpoint — [timestamp]
Status: IN PROGRESS
Tickets queued: [PROJ-N, PROJ-N ...]
Tickets completed: []
Tickets blocked: []
Tickets skipped (needs-human): []
Learning concepts covered: []
```

---

## FOR EACH TICKET — repeat A through L in order

---

### A. READ AND ASSESS

```python
ticket = read_issue(jira, "PROJ-N")
print(ticket["summary"])
print(ticket["description"])
print(ticket["comments"])
```

Also check for a matching plan document:
```bash
# Look for a plan that matches this ticket's topic
ls .claude/plans/ | grep -i "[slug from ticket title]" 2>/dev/null || true
```

If a plan doc exists — read it. It contains YOUR intent for this ticket.
The ticket is the what; the plan doc is the why and the full context.

Skip immediately if:
- Requires a design decision not in the ticket or plan doc
- Depends on another ticket not yet complete
- Scope is too vague to produce a verifiable result
- Requires credentials or external access not available

```python
# If skipping:
mark_needs_human(jira, "PROJ-N",
    "Cannot attempt overnight: [specific reason]. Clarify before re-queuing.")
# move to next ticket
```

---

### B. ORIENT — git-historian agent

"Use the git-historian agent to understand the code area this ticket touches."
Read .claude/memory/history-brief.md when done.

---

### C. MARK IN PROGRESS

```python
mark_in_progress(jira, "PROJ-N")
# transitions ticket to In Progress, adds comment, swaps labels
```

---

### D. CREATE FEATURE BRANCH

Use the branch name from the ticket body if specified,
otherwise derive from the ticket key and title:

```bash
git checkout main
git checkout -b feature/PROJ-N-[short-slug-from-title]
```

---

### E. PLAN

Check .claude/plans/ for a matching plan document first.
If one exists, use it as the implementation spec — do not re-derive.
If none exists, run /plan-feature [ticket title] and save to .claude/plans/PROJ-N.md

Do not write any code until you have a plan.

---

### F. IMPLEMENT

Read CLAUDE.md agent routing rules and invoke the appropriate
specialist agents based on what the ticket requires.

Apply the code-economy skill throughout:
- Run the Decision Ladder before any new code or dependency
- Write complete code — no placeholders, no TODOs
- If blocked after 2 attempts on the same problem, skip

---

### G. TEST

```bash
# Read CLAUDE.md for the exact test commands for this repo
cat CLAUDE.md | grep -A10 "Test Commands"
```

Run the full test suite. Must pass before committing.

If tests fail after 2 fix attempts:
```python
mark_blocked(jira, "PROJ-N",
    "Tests failing after 2 attempts: [exact failure]. Needs human review.")
```
```bash
git checkout main
git branch -D feature/PROJ-N-[slug]
# move to next ticket
```

---

### H. COMMIT CODE ONLY — no docs

```bash
# Stage source and test files only
git add src/ tests/ lib/ app/ 2>/dev/null || true

git status --short | grep -E "^\s*(M|A)" | \
  grep -vE "docs/|OVERNIGHT_REPORT|\.claude/|\.log$" | \
  awk '{print $2}' | xargs git add 2>/dev/null || true

# VERIFY before committing — only code should appear
echo "=== Files being committed ==="
git diff --cached --name-only
# If docs/learning or OVERNIGHT_REPORT appears: git reset HEAD <file>

git commit -m "fix(PROJ-N): [ticket title shortened to 60 chars]

[2-3 bullet points of what was done]"

git push origin feature/PROJ-N-[slug]
```

---

### I. OPEN DRAFT MR

Branch must be on remote before creating the MR (push in step H does this).

Generate the MR description with /mr-description, then:

```bash
glab mr create \
  --title "fix(PROJ-N): [ticket title]" \
  --description "$(cat .claude/plans/mr-description-PROJ-N.md 2>/dev/null || \
    echo 'Implements PROJ-N: [ticket title]')" \
  --target-branch main \
  --source-branch feature/PROJ-N-[slug] \
  --label "ai-completed" \
  --assignee @me \
  --draft \
  --remove-source-branch
```

Get the MR URL from the output, then update the Jira ticket:

```python
# Transition to In Review — NOT Done
# Done is set by you after reviewing and merging
mark_completed(jira, "PROJ-N",
    pr_url="https://gitlab.company.com/.../merge_requests/N",
    branch="feature/PROJ-N-[slug]")
```

mark_completed transitions the ticket to "In Review" and adds a comment
with the MR URL. It does NOT transition to Done.

---

### J. GENERATE LEARNING MATERIALS (local only — never committed)

These files are written to docs/learning/ on disk. Gitignored.
They will NOT appear in the MR diff. They are for you to open in the morning.

The lesson must be grounded in the ACTUAL CHANGES made in this ticket —
not the ticket title, not a generic concept. You want to understand what
changed, why it changed that way, and what you need to know to maintain
or extend it. The diff is the source of truth.

**Step 1 — Read the actual diff.**
```bash
# See exactly what changed in this commit
git diff main..feature/PROJ-N-[slug] --stat
git diff main..feature/PROJ-N-[slug]
```

From the diff, extract:
- Which files changed and why each one changed
- What new patterns, structures, or APIs were introduced
- What decisions were made and what alternatives were rejected
- What would break if someone changed this without understanding it
- Any non-obvious choices (why THIS approach, not a simpler one)

**Step 2 — Write a change summary before teaching.**
Write to `.claude/memory/change-summary-PROJ-N.md`:

```markdown
# Change Summary — PROJ-N: [ticket title]
Date: [date]
Branch: feature/PROJ-N-[slug]

## What Changed
[Files changed and one-line description of each change]

## Why Each Change Was Made
[For each file: the reasoning behind the approach taken]

## Key Decisions
[What non-obvious choices were made and why]
[What alternatives were considered and rejected]

## What Could Break
[What a future developer could accidentally break if they
don't understand this change]

## The Concept to Teach
[The most important technical concept illustrated by these
changes — specific enough to produce a useful lesson]
```

The concept must come from the diff, not the ticket title.
Examples of diff-derived concepts:
- The diff adds a state machine for reload timing
  → teach "C++ state machines and timed transitions"
- The diff introduces an ADF description wrapper for Jira API
  → teach "Atlassian Document Format and why Jira v3 rejects plain strings"
- The diff adds pytest-xvfb for headless Qt testing
  → teach "Virtual framebuffers and headless GUI testing on Linux"

Bad concept derivation (ticket-title based):
- Ticket says "Add weapon reload" → concept is "weapon reload" ← wrong
- Ticket says "Fix Jira integration" → concept is "Jira" ← too broad

**Step 3 — Generate the lesson (REQUIRED).**

Pass the change summary as context to /teach:

```
/teach [concept from Step 2]

Context for this lesson — base it on these actual changes:
[paste the change summary from .claude/memory/change-summary-PROJ-N.md]

The lesson must:
- Show the actual changed files and explain what they do
- Explain the decisions made (not just what, but why)
- Show what would break without understanding this
- Ground examples in the real code, not generic examples
```

→ Creates docs/learning/lessons/NNNN-[concept].html

**Step 4 — Generate the drill (REQUIRED).**

```
/drill-me [concept]

Ground the drill in the actual changes from PROJ-N.
Include questions that test:
- Understanding of WHY the approach was taken
- Recognition of the pattern in the real changed files
- What would break if a key part were changed
```

→ Creates docs/learning/drills/NNNN-[concept].html

**Step 5 — Generate vocabulary (recommended).**
Run: /terms [concept]
→ Creates docs/learning/vocab/NNNN-[concept].html

**Step 6 — Verify nothing got staged.**
```bash
git status --short | grep "docs/learning"
# Must be ?? (untracked) — if M or A: git reset HEAD docs/learning/
git reset HEAD docs/learning/ 2>/dev/null || true
```

---

### K. UPDATE CHECKPOINT

```
Ticket PROJ-N: COMPLETED
  MR: !MR_NUMBER — [MR URL]
  Branch: feature/PROJ-N-[slug]
  Jira status: In Review (awaiting your review and merge)
  Concept taught: [concept]
  Lesson: docs/learning/lessons/NNNN-[concept].html
  Drill:  docs/learning/drills/NNNN-[concept].html
```

---

### L. RETURN TO MAIN

```bash
git checkout main
```

Repeat A through L for the next ticket.

---

## END OF ALL TICKETS

---

### 1. REBUILD THE LEARNING INDEX (local only)

Run: /index
→ Regenerates docs/learning/index.html

```bash
git status --short | grep "docs/learning/index.html"
# Must be ?? — if staged: git reset HEAD docs/learning/index.html
```

---

### 2. WRITE THE MORNING REPORT (local only — never committed)

Write OVERNIGHT_REPORT.md to the repo root. Gitignored. Do not commit.

```markdown
# Overnight Build Report
Date: [date]
Repo: [repo name]
Jira Project: [JIRA_PROJECT]
Duration: [start] → [finish]
Tickets attempted: [N]

## Summary
[2-3 sentences: what was accomplished overall]

## Completed ✅ (In Review — awaiting your review and merge)
| Ticket | Summary | MR | Branch | Concept Taught |
|--------|---------|----|----|----------------|
| PROJ-N | [summary] | !MR | feature/PROJ-N-slug | [concept] |

## Blocked ❌ (tests failed or stuck)
| Ticket | Summary | Failure |
|--------|---------|---------|
| PROJ-N | [summary] | [specific actionable description] |

## Needs Human ⏭ (too vague or requires decision)
| Ticket | Summary | What Is Needed |
|--------|---------|----------------|
| PROJ-N | [summary] | [exact question or clarification] |

## Learning Materials Created Tonight
| Concept | Lesson | Drill | Vocab |
|---------|--------|-------|-------|
| [concept] | ✅ | ✅ | ✅ |

## Your Morning Checklist
1. Read this report
2. open docs/learning/index.html        ← study tonight's changes
3. glab mr list --assignee @me          ← see draft MRs to review
4. glab mr view N --web                 ← open specific MR in browser
5. glab mr merge N --squash             ← merge if satisfied
6. [then in Jira] move ticket to Done   ← after merging
7. glab issue list --label ai-blocked   ← fix and re-queue blocked tickets

NOTE: Tickets are In Review in Jira, not Done.
Move them to Done yourself after reviewing and merging each MR.
```

```bash
git status --short | grep OVERNIGHT_REPORT
# Must be ?? — if staged: git reset HEAD OVERNIGHT_REPORT.md
```

---

### 3. FINAL CHECKPOINT UPDATE

```
Session completed: [timestamp]
Status: DONE
Summary: [N] in-review, [N] blocked, [N] skipped
All tickets in Jira are In Review — human review and merge required.
```

---

## COMMIT POLICY

| Path | Committed? | Why |
|------|-----------|-----|
| src/, tests/, lib/ | ✅ YES | The work |
| docs/learning/ | ❌ NEVER | Local study only |
| OVERNIGHT_REPORT.md | ❌ NEVER | Morning reading |
| .claude/memory/ | ❌ NEVER | Session state |
| .claude/plans/ | ❌ NEVER | Planning artifacts |
| .gitignore update | ✅ ONCE | On main, first run only |

When in doubt: git diff --cached --name-only before every commit.
Only source code and tests. Everything else gets reset.

---

## FAILURE POLICY

Never stop the run because one ticket failed.
Never silently skip — always transition and comment the ticket.
Never commit docs, reports, or memory files.
Never transition a ticket to Done — that is always a human action.

If a situation is genuinely ambiguous or dangerous:
1. Stop work on that ticket
2. Call mark_needs_human(jira, key, reason)
3. Move to the next ticket
