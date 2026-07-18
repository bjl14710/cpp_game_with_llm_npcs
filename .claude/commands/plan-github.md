---
description: Reads CLAUDE.md and the current codebase to plan and create GitHub issues for personal projects. Creates milestone groupings (equivalent to epics), issues with full acceptance criteria, and labels them ready-for-ai. No human drafting required.
argument-hint: Optional focus area e.g. "oscilloscope device family" or "phase 2" (blank = full project analysis)
---

Read .claude/skills/github/SKILL.md and .claude/skills/code-economy/SKILL.md
before doing anything. The GitHub skill covers auth and issue creation.
The code-economy skill defines what makes a good, reviewable change.

---

You are a backlog planning agent for a personal GitHub project.
Your job is to read the current project state and create well-formed
GitHub issues that the overnight session can work through autonomously.

Every issue you create must be:
- Self-contained (completable in one overnight session)
- Specific (clear acceptance criteria, not vague goals)
- Verifiable (a test to run or output to check)
- Code-economy compliant (won't produce bloated PRs)
- Labelled ready-for-ai and assigned to the repo owner

---

## STEP 1 — PREREQUISITES

```python
import os
from github import Github

# Verify auth
token = os.environ.get("GH_TOKEN")
username = os.environ.get("GITHUB_USERNAME")
if not token or not username:
    raise SystemExit(
        "Set GH_TOKEN and GITHUB_USERNAME in your shell config first."
    )

gh = Github(token)
repo = get_repo(gh)   # auto-detects from git remote
print(f"Planning issues for: {repo.full_name}")

# Read existing open issues to avoid duplicates
existing = list_issues(repo, state="open")
existing_titles = {i["title"].lower() for i in existing}
print(f"Existing open issues: {len(existing)}")
```

---

## STEP 2 — READ PROJECT CONTEXT

Read all of these before planning any issues:

**CLAUDE.md** — the project purpose, stack, rules, and current state.
This is the source of truth for what the project is trying to be.

**README.md** — user-facing description and roadmap if any.

**Current codebase:**
```bash
# What exists right now
find src/ lib/ app/ -type f 2>/dev/null | head -50 || \
  find . -name "*.py" -o -name "*.cpp" -o -name "*.ts" \
  | grep -v node_modules | grep -v build | head -50

# Where work is heading
git log --oneline -20

# Known gaps and TODOs
grep -rn "TODO\|FIXME\|HACK\|NotImplemented\|raise NotImplementedError" \
  src/ lib/ app/ 2>/dev/null | head -30

# Test coverage gaps (uncovered files)
pytest --co -q 2>/dev/null | head -30 || true
```

---

## STEP 3 — CREATE MILESTONE (GITHUB EQUIVALENT OF EPIC)

Group related issues under a milestone:

```python
def create_milestone(repo, title: str, description: str) -> int:
    """Create a GitHub milestone. Returns the milestone number."""
    milestone = repo.create_milestone(
        title=title,
        description=description
    )
    print(f"Created milestone: {milestone.title} (#{milestone.number})")
    return milestone.number
```

Create one milestone per theme of work. For "$1" focus, one milestone
is enough. For full project analysis, create 2-4 milestones.

Good milestone names:
- "SCPI Device Family — Oscilloscopes"
- "NPC Tier 1: Structured Output"
- "Data Pipeline — Market Data Ingestion"

---

## STEP 4 — CREATE ISSUES

For each issue, use this exact body template so the overnight session
has everything it needs:

```python
ISSUE_TEMPLATE = """## What to change
{what}

## Where it is
{where}

## How to verify it's done
{verify}

## Constraints
{constraints}

## Code economy note
{economy_note}

## Branch name
`feature/issue-{{number}}-{slug}`

## Concept for learning materials
{concept}
"""
```

The **concept** field is critical — it tells the overnight session what
to teach/drill after completing this issue. It must be:
- A technical concept, not a feature name
- Specific enough to produce a useful lesson
- Different from concepts already in docs/learning/PROGRESS.md

```python
def plan_and_create_issues(repo, milestone_number: int,
                           specs: list[dict]) -> list[dict]:
    """
    Create a batch of issues. Each spec is a dict with keys:
        title, what, where, verify, constraints, economy_note,
        slug, concept

    Returns list of created issue dicts with number and URL.
    """
    username = os.environ["GITHUB_USERNAME"]
    created = []

    # Ensure overnight labels exist
    create_overnight_labels(repo)

    milestone = repo.get_milestone(milestone_number)

    for spec in specs:
        # Skip if similar issue already exists
        if spec["title"].lower() in existing_titles:
            print(f"Skipping (exists): {spec['title']}")
            continue

        body = ISSUE_TEMPLATE.format(
            what=spec["what"],
            where=spec["where"],
            verify=spec["verify"],
            constraints=spec["constraints"],
            economy_note=spec["economy_note"],
            slug=spec["slug"],
            concept=spec["concept"],
            number="N"    # placeholder — updated after creation
        )

        issue = repo.create_issue(
            title=spec["title"],
            body=body,
            labels=["ready-for-ai"],
            assignees=[username],
            milestone=milestone
        )

        # Update branch name now that we have the number
        updated_body = body.replace(
            "feature/issue/N",
            f"feature/issue-{issue.number}"
        )
        issue.edit(body=updated_body)

        print(f"  Created #{issue.number}: {spec['title']}")
        print(f"  Concept: {spec['concept']}")
        created.append({
            "number":  issue.number,
            "title":   spec["title"],
            "concept": spec["concept"],
            "url":     issue.html_url
        })

    return created
```

---

## STEP 5 — ECONOMY NOTE GUIDANCE

Every issue should include an `economy_note` that pre-constrains the
implementation. This prevents the overnight session from over-building.

Good economy notes:
```
Use stdlib re for command pattern matching — no regex library needed.
Follow the existing BaseDevice pattern exactly — no new abstractions.
One new file maximum: src/devices/psu_e3631a.py plus its test file.
```

Bad economy notes (too vague):
```
Keep it simple.
Don't over-engineer.
```

The note should name the specific stdlib/pattern to use, or the
specific constraint on scope.

---

## STEP 6 — VERIFY THE QUEUE

After creating all issues:

```python
print("\n=== Tonight's ready-for-ai queue ===")
queue = list_issues(repo, label="ready-for-ai")
for issue in queue:
    print(f"  #{issue['number']:3} {issue['title']}")
    print(f"       URL: {issue['url']}")

print(f"\nTotal: {len(queue)} issues queued")
print("Run nightly-all.sh tonight to work through them.")
```

---

## STEP 7 — WRITE LOCAL PLAN SUMMARY

Save a planning summary locally (not committed):

```python
summary = f"""# Issue Planning Summary
Date: {datetime.now().strftime('%Y-%m-%d %H:%M')}
Repo: {repo.full_name}
Focus: {"$1" or "full project analysis"}

## Issues Created Tonight
"""
for issue in created_issues:
    summary += f"- #{issue['number']}: {issue['title']}\n"
    summary += f"  Concept: {issue['concept']}\n"
    summary += f"  URL: {issue['url']}\n\n"

summary += """
## What Was NOT Planned
[things seen in codebase not turned into issues and why]
"""

Path(".claude/memory/planning-session.md").write_text(summary)
print("\nPlan saved to .claude/memory/planning-session.md")
```

---

## CONCEPT SELECTION GUIDE

The `concept` field in each issue is what drives learning generation.
Choose well — this is what you'll study after the overnight session.

| Issue type | Good concept | Bad concept |
|------------|-------------|-------------|
| New SCPI device | "Multi-channel PSU channel selection in SCPI" | "E3631A support" |
| New Qt block | "Qt drag-and-drop hardware block pattern" | "New block" |
| Bug fix | "SCPI ErrorQueue per IEEE 488.2" | "Fixed a bug" |
| New NPC | "LLM character constraint and knowledge domain" | "Added blacksmith" |
| Backtest fix | "Lookahead bias in rolling feature windows" | "Fixed backtest" |
| Data pipeline | "Point-in-time market data and survivorship bias" | "Data ingestion" |

Before finalizing a concept, check docs/learning/PROGRESS.md to avoid
re-teaching something already well covered.

---

## RUNNING THIS COMMAND

```
/plan-github                        full project analysis
/plan-github "oscilloscope family"  focus on one area
/plan-github "phase 2"             focus on next milestone
```

Re-run after completing a milestone to plan the next set of work.
It reads existing open issues first and skips duplicates.
