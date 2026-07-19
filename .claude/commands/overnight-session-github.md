---
description: Overnight autonomous development session for personal GitHub repos. Reads ready-for-ai issues, creates branches, implements changes, runs tests, opens draft PRs, and generates one teach lesson plus one drill per completed issue. Learning materials are never committed.
argument-hint: Optional focus label or issue number (blank = all ready-for-ai issues)
---

Read these skill files before doing anything else:
- .claude/skills/github/SKILL.md       auth, issues, labels, PR creation
- .claude/skills/code-economy/SKILL.md economy, test requirements, readability
Use overnight-coordinator agent to manage the session.

---

## WHAT GETS COMMITTED AND WHAT DOES NOT

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

## SETUP (run once at session start)

```bash
# 1. Start clean on main
git checkout main && git pull

# 2. Rebuild knowledge graph if graphify is installed
graphify update . --force 2>/dev/null || true

# 3. Resume prior session if interrupted
cat .claude/memory/checkpoint.md 2>/dev/null || echo "Fresh session"
```

Read tonight's issue queue using the GitHub skill:
```python
gh = get_github_client()
repo = get_repo(gh)
issues = list_issues(repo, label="ready-for-ai")
print(f"Tonight's queue: {len(issues)} issues")
for i in issues:
    print(f"  #{i['number']} {i['title']}")
```

Write to .claude/memory/checkpoint.md:
```
# Checkpoint — [timestamp]
Status: IN PROGRESS
Issues queued: [numbers]
Issues completed: []
Issues blocked: []
Issues skipped: []
Concepts taught tonight: []
```

---

## FOR EACH ISSUE — steps A through L

---

### A. READ AND ASSESS

```python
issue = read_issue(repo, issue_number)
```

Read the full issue body. Look for:
- `## What to change` — the implementation target
- `## Where it is` — the files to touch
- `## How to verify it's done` — the acceptance criteria
- `## Constraints` — what must not break
- `## Concept for learning materials` — what to teach after

Skip immediately if:
- Issue is too vague to produce verifiable output
- Depends on another issue not yet completed
- Requires a decision not documented in the issue

```python
# If skipping:
mark_needs_human(repo, issue_number,
    "Cannot attempt overnight: [specific reason]. "
    "Clarify and re-label ready-for-ai.")
# move to next issue
```

---

### B. ORIENT — git-historian agent

Before touching any code:
"Use the git-historian agent to understand the code area this issue touches."

Read .claude/memory/history-brief.md when complete.

---

### C. MARK IN PROGRESS

```python
mark_in_progress(repo, issue_number,
    branch=f"feature/issue-{issue_number}-{slug}")
```

---

### D. CREATE BRANCH

```bash
git checkout main
git checkout -b feature/issue-[N]-[slug-from-issue-body]
```

The branch name comes from the `## Branch name` field in the issue body.

---

### E. PLAN

Run /plan-feature [concept from issue title]
Save plan to .claude/plans/issue-[N].md

Do not write code until you have a written plan.

---

### F. IMPLEMENT

Apply the code-economy skill throughout:
- Run the Decision Ladder before introducing any new code or dependency
- Write economy comments when making a deliberately minimal choice
- Follow the repo's existing patterns — no new abstractions unless the
  issue explicitly asks for one

Use specialist agents where available:
- scpi-implementer for Silmulator device classes
- npc-dialogue-designer for NPC game characters
- backtesting-verifier review for trading strategy changes

---

### G. TEST

Run tests. Must pass before committing.

```bash
# Python projects
pytest tests/ -v

# C++/CMake projects (Silmulator)
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && \
  make -j4 && ctest --output-on-failure && cd ..

# Both layers
cd build && cmake .. && make -j4 && ctest --output-on-failure && \
  cd .. && pytest tests/ -v
```

If tests fail after 2 fix attempts:
```python
mark_blocked(repo, issue_number,
    "Tests failing after 2 attempts: [exact failure description]. "
    "Needs human review.")
```
```bash
git checkout main
git branch -D feature/issue-[N]-[slug]
```
Move to next issue.

---

### H. COMMIT CODE ONLY

Verify staging before committing — never include docs or memory files.

```bash
# Stage source and test files only
git add src/ tests/ lib/ app/ 2>/dev/null || true

# Also catch any other changed source files
git status --short | grep -E "^\s*(M|A)" | \
  grep -vE "docs/|OVERNIGHT_REPORT|\.claude/|\.log$" | \
  awk '{print $2}' | xargs git add 2>/dev/null || true

# CRITICAL: verify nothing wrong is staged
echo "=== Files being committed ==="
git diff --cached --name-only

# If docs/learning or OVERNIGHT_REPORT appears above, unstage it:
# git reset HEAD docs/learning/ 2>/dev/null || true
# git reset HEAD OVERNIGHT_REPORT.md 2>/dev/null || true

git commit -m "fix(#[N]): [issue title shortened to 60 chars]

[2-3 bullet points summarising what was done]

Closes #[N]"

git push origin feature/issue-[N]-[slug]
```

---

### I. OPEN DRAFT PR

Generate PR description with /mr-description, then:

```python
pr = create_draft_pr(
    repo,
    title=f"fix(#{issue_number}): {issue['title']}",
    body=make_pr_body(mr_description_output, closes_issue=issue_number),
    head_branch=f"feature/issue-{issue_number}-{slug}"
)
```

The `make_pr_body()` helper appends `Closes #N` so GitHub closes the
issue automatically when the PR is merged.

Apply the code-economy skill to the PR description:
- Summary: 2 sentences max — what and why
- Changes by file: explain WHY each file changed, not just WHAT
- Testing section: name the specific tests that pass
- Economy note: mention any deliberate minimal choices made

---

### J. MARK COMPLETE

```python
mark_completed(
    repo,
    issue_number=issue_number,
    pr_number=pr["number"],
    pr_url=pr["url"],
    branch=f"feature/issue-{issue_number}-{slug}"
)
```

---

### K. GENERATE LEARNING MATERIALS (local only — never committed)

This step is mandatory for every completed issue.
Every merged PR must have at least one teach lesson and one drill.

**Step 1 — Identify the concept.**
Read the `## Concept for learning materials` field from the issue body.
If it is missing or too vague, derive one following these rules:
- Choose a technical concept, not a feature name
- "triple-channel SCPI channel selection" ✅  "E3631A support" ❌
- Check docs/learning/PROGRESS.md — skip if already well covered

**Step 2 — Generate the lesson (REQUIRED).**
```
/teach [concept]
```
→ Creates docs/learning/lessons/NNNN-[concept].html

In overnight automated mode:
- Generate the full HTML lesson
- Skip the in-chat knowledge check (no user present)
- Make the lesson thorough — it is the only output

**Step 3 — Generate the concept drill (REQUIRED).**
```
/drill-me [concept]
```
→ Creates docs/learning/drills/NNNN-[concept].html

Every quiz question in this drill must follow code-economy integrity:
- Correct answer is NOT the longest option
- All options are roughly equal length
- data-why explanations are detailed for EVERY option (not just correct)
- Options are shuffled on load (Fisher-Yates, data-correct attribute)

**Step 4 — Generate vocabulary (OPTIONAL but recommended).**
```
/terms [concept]
```
→ Creates docs/learning/vocab/NNNN-[concept].html
→ Updates docs/learning/GLOSSARY.md

**Step 5 — Verify nothing got staged.**
```bash
git status --short | grep "docs/learning"
# Must be ?? (untracked) — NOT M or A
# If staged, unstage immediately:
git reset HEAD docs/learning/ 2>/dev/null || true
```

---

### L. UPDATE CHECKPOINT

```
Issue #[N]: COMPLETED
  PR: #[PR number] — [PR URL]
  Branch: feature/issue-[N]-[slug]
  Concept taught: [concept]
  Lesson: docs/learning/lessons/NNNN-[concept].html ← local only
  Drill:  docs/learning/drills/NNNN-[concept].html  ← local only
```

```bash
git checkout main
```

Repeat A through L for the next issue.

---

## END OF ALL ISSUES

---

### 1. REBUILD THE LEARNING INDEX (local only)

```
/index
```
→ Rebuilds docs/learning/index.html linking all lessons, drills, and vocab

Verify not staged:
```bash
git status --short | grep "docs/learning/index.html"
# Must be ?? — if M or A, unstage: git reset HEAD docs/learning/index.html
```

---

### 2. WRITE OVERNIGHT REPORT (local only — never committed)

Write OVERNIGHT_REPORT.md to the repo root.
This file is gitignored. Do not commit it.

```markdown
# Overnight Build Report
Date: [date]
Repo: [repo name]
Duration: [start] → [finish]
Issues attempted: [N]

## Summary
[2-3 sentences: what was accomplished]

## Completed ✅
| Issue | Title | PR | Branch | Concept Taught |
|-------|-------|----|--------|----------------|
| #N | [title] | #PR | feature/issue-N-slug | [concept] |

## Blocked ❌
| Issue | Title | Failure |
|-------|-------|---------|
| #N | [title] | [specific failure — actionable] |

## Needs Human ⏭
| Issue | Title | What's Needed |
|-------|-------|---------------|
| #N | [title] | [exact question or clarification needed] |

## Learning Materials Created
| Concept | Lesson | Drill | Vocab |
|---------|--------|-------|-------|
| [concept] | ✅ lessons/NNNN.html | ✅ drills/NNNN.html | ✅/— |

## Code Economy Summary
[Any issues where the decision ladder flagged unnecessary complexity,
or where Claude chose a minimal path and left an economy comment]

## Draft PRs Awaiting Your Review
| PR | Issue | Title | URL |
|----|-------|-------|-----|
| #PR | #N | [title] | [url] |

## Your Morning Checklist
1. cat OVERNIGHT_REPORT.md                   ← you're reading it
2. open docs/learning/index.html             ← study last night's changes
3. gh pr list --draft                        ← see PRs to review
4. gh pr diff [N]                            ← review a specific PR
5. gh pr merge [N] --squash --delete-branch ← merge if satisfied
6. gh issue list --label ai-blocked         ← fix and re-queue
```

Verify not staged:
```bash
git status --short | grep OVERNIGHT_REPORT
# Must be ?? — if staged: git reset HEAD OVERNIGHT_REPORT.md
```

---

## COMMIT POLICY SUMMARY

| Path | Committed? | Note |
|------|-----------|------|
| src/, tests/, lib/, app/ | ✅ YES | The work |
| docs/learning/ | ❌ NEVER | Local study only |
| OVERNIGHT_REPORT.md | ❌ NEVER | Morning reading |
| .claude/memory/ | ❌ NEVER | Session state |
| .claude/plans/ | ❌ NEVER | Planning artifacts |
| .gitignore update | ✅ ONCE | On main, first run only |

When in doubt: `git diff --cached --name-only` before every commit.
Only source code and tests should appear. Everything else gets reset.

---

## FAILURE POLICY

Never stop the run because one issue failed.
Never commit docs, reports, or memory files.
Never silently skip — always label and comment the issue.
If in doubt whether a change is safe: stop, label needs-human, move on.
