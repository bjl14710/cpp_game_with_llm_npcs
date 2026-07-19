---
description: Work through tonight's ready-for-ai GitHub issues. Each issue gets its own branch and draft PR containing only code changes. Learning materials (lessons, drills, vocab) and the overnight report are written locally but never committed to any branch.
argument-hint: Optional focus area (blank = all ready-for-ai issues)
---

You are running an overnight autonomous development session on this
repository. Use the overnight-coordinator agent to manage the session.

Before making any GitHub API calls, read .claude/skills/github/SKILL.md.
It contains auth, label management, issue reading, PR creation (branch
must exist on remote first), and the overnight workflow helpers.

## IMPORTANT — WHAT GETS COMMITTED AND WHAT DOES NOT

**On feature branches (one per issue):** source code and tests only.
No docs, no learning files, no reports. The PR reviewer sees only the
change that solves the issue — nothing else.

**Never committed anywhere:**
- docs/learning/ (lessons, drills, vocab, index) — written locally only
- OVERNIGHT_REPORT.md — written locally only
- .claude/memory/ files — written locally only
- Any generated HTML or markdown documentation

Learning files and the report are for you to open in the morning.
They are not part of the codebase and should not appear in any PR diff.

The repo's .gitignore must exclude these paths. Verify and add if missing:

```bash
grep -q "docs/learning/" .gitignore || echo "docs/learning/" >> .gitignore
grep -q "OVERNIGHT_REPORT.md" .gitignore || echo "OVERNIGHT_REPORT.md" >> .gitignore
grep -q ".claude/memory/" .gitignore || echo ".claude/memory/" >> .gitignore
```

If .gitignore itself was modified, commit that change to main now:
```bash
git diff --name-only | grep -q ".gitignore" && \
  git add .gitignore && \
  git commit -m "chore: exclude learning docs and overnight report from git" && \
  git push origin main || true
```

---

## SETUP (run once at session start)

```bash
# 1. Start on main, up to date
git checkout main
git pull

# 2. Rebuild knowledge graph if graphify is installed
graphify update . --force 2>/dev/null || true

# 3. Resume prior run if checkpoint exists
cat .claude/memory/checkpoint.md 2>/dev/null || echo "No checkpoint — fresh session"

# 4. Read tonight's issues
gh issue list \
  --label "ready-for-ai" \
  --json number,title,body,labels \
  --order created \
  --limit 20
```

Record session start in .claude/memory/checkpoint.md:
```
# Checkpoint — [date] [time]
Status: IN PROGRESS
Issues queued: [N1, N2, N3 ...]
Issues completed: []
Issues blocked: []
Issues skipped (needs-human): []
Learning concepts covered: []
```

---

## FOR EACH ISSUE — repeat A through L in order

---

### A. READ AND ASSESS

```bash
gh issue view [N] --comments
```

Is this issue clear enough to attempt autonomously tonight?

Skip immediately (label and move on) if:
- Requires a design decision not in the issue text
- Depends on another issue not yet complete
- Scope is too vague to produce a verifiable result
- Requires credentials or external access not available here

```bash
# If skipping:
gh issue edit [N] --add-label "needs-human" --remove-label "ready-for-ai"
gh issue comment [N] \
  --body "Skipped overnight — [specific reason]. Clarify before re-queuing."
# then move to next issue
```

---

### B. ORIENT — git-historian

Before touching any code, invoke the git-historian agent:
"Use the git-historian agent to understand the code area issue #[N] touches."

Read .claude/memory/history-brief.md when it finishes.
This tells you what changed recently, what is unstable, and what NOT to redo.

---

### C. MARK IN PROGRESS

```bash
gh issue edit [N] --add-label "ai-in-progress" --remove-label "ready-for-ai"
gh issue comment [N] --body "Overnight session starting on this issue now."
```

---

### D. CREATE FEATURE BRANCH

```bash
git checkout main
git checkout -b feature/issue-[N]
```

---

### E. PLAN

Run /plan-feature to produce a written plan before any implementation.
Save to .claude/plans/issue-[N].md

Do not write any code until you have an approved plan saved to that file.

---

### F. IMPLEMENT

Use the specialist agents defined for this repo (see the repo-specific
overnight command if one exists alongside this file, or use the
universal agents: architect, tester, verifier, reviewer).

Write complete code. No placeholders. No TODOs.
If blocked after 2 attempts on the same problem, skip the issue.

---

### G. TEST

Run the project test suite. Must pass before committing.
(Exact commands defined per repo — run pytest, cmake+ctest, npm test,
or whatever this project uses.)

If tests fail after 2 fix attempts:
```bash
gh issue edit [N] --remove-label "ai-in-progress" --add-label "ai-blocked"
gh issue comment [N] \
  --body "Blocked: tests failed after 2 attempts. [exact failure]. Needs human review."
git checkout main
git branch -D feature/issue-[N]
# move to next issue
```

---

### H. COMMIT CODE ONLY — no docs

Commit only source code and test files. Explicitly exclude everything else.

```bash
# Stage only code — never docs/learning, never OVERNIGHT_REPORT, never .claude/
git add src/ tests/ lib/ app/ 2>/dev/null || true

# Also stage any other source files changed (but not docs or generated files)
git status --short | grep -E "^\s*M|^\s*A" | \
  grep -vE "docs/|OVERNIGHT_REPORT|\.claude/|\.log$|__pycache__" | \
  awk '{print $2}' | xargs git add 2>/dev/null || true

# Verify staging looks right — should contain only code
git diff --cached --name-only

# Commit
git commit -m "fix(#[N]): [issue title shortened to 60 chars]"
git push origin feature/issue-[N]
```

If `git diff --cached --name-only` shows any docs/learning/ or OVERNIGHT_REPORT.md
files, run `git reset HEAD <that file>` before committing.

---

### I. OPEN DRAFT PR

Run /mr-description to generate the PR body, then:

```bash
gh pr create \
  --title "fix(#[N]): [issue title]" \
  --body "$(cat .claude/plans/mr-description-[N].md 2>/dev/null || echo 'See issue #[N] for context.')" \
  --base main \
  --head feature/issue-[N] \
  --draft

gh issue edit [N] \
  --remove-label "ai-in-progress" \
  --add-label "ai-completed"

gh issue close [N] \
  --comment "Implemented in the draft PR above. Awaiting review."
```

---

### J. GENERATE LEARNING MATERIALS (local only — never committed)

These files are written to docs/learning/ on disk. They are gitignored.
They will NOT appear in the PR. They are for you to open in the morning.

The lesson must be grounded in the ACTUAL CHANGES made — not the issue
title, not a generic concept. You want to understand what changed, why
it changed that way, and what you need to know to maintain or extend it.
The diff is the source of truth.

**Step 1 — Read the actual diff.**
```bash
git diff main..feature/issue-[N]-[slug] --stat
git diff main..feature/issue-[N]-[slug]
```

From the diff, extract:
- Which files changed and why each one changed
- What new patterns, structures, or APIs were introduced
- What decisions were made and what alternatives were rejected
- What would break if someone changed this without understanding it
- Any non-obvious choices (why THIS approach, not a simpler one)

**Step 2 — Write a change summary.**
Write to `.claude/memory/change-summary-[N].md`:

```markdown
# Change Summary — Issue #[N]: [title]
Date: [date]

## What Changed
[Files changed and one-line description of each]

## Why Each Change Was Made
[For each file: the reasoning behind the approach]

## Key Decisions
[Non-obvious choices and why — what alternatives were rejected]

## What Could Break
[What a future developer could accidentally break without
understanding this change]

## The Concept to Teach
[The most important technical concept illustrated by these
changes — derived from the diff, not the issue title]
```

Check docs/learning/PROGRESS.md — skip if already well covered.

**Step 3 — Generate the lesson (REQUIRED).**

```
/teach [concept from diff]

Context — base this lesson on these actual changes:
[paste the change summary]

The lesson must show the actual changed files, explain the
decisions made (not just what but why), and ground all
examples in the real code from this commit.
```

→ Creates docs/learning/lessons/NNNN-[concept].html

In automated overnight mode:
- Generate the HTML fully — it is the only output
- Skip the in-chat knowledge check (no user present)
- Make the content thorough and grounded in the real diff

**Step 4 — Generate the concept drill (REQUIRED).**

```
/drill-me [concept]

Ground the drill in the actual changes from issue #[N].
Test understanding of WHY the approach was taken, not just what.
Include questions about what would break if key parts were changed.
```

→ Creates docs/learning/drills/NNNN-[concept].html

**Step 5 — Generate the vocabulary drill.**
Run: /terms [concept]
→ Creates docs/learning/vocab/NNNN-[concept].html
→ Updates docs/learning/GLOSSARY.md

**Step 6 — Verify none of these ended up staged.**
```bash
git status --short | grep "docs/learning"
# Should show nothing (or show ?? meaning untracked — that is correct)
# If you see M or A next to any docs/learning file, unstage it:
git reset HEAD docs/learning/ 2>/dev/null || true
```

---

### K. UPDATE CHECKPOINT

```
Issue #[N]: COMPLETED
  Branch: feature/issue-[N]
  PR: #[number]
  Concept taught: [concept name]
  Lesson: docs/learning/lessons/NNNN-[concept].html
  Drill: docs/learning/drills/NNNN-[concept].html
  Vocab: docs/learning/vocab/NNNN-[concept].html
```

---

### L. RETURN TO MAIN

```bash
git checkout main
```

Repeat A through L for the next issue.

---

## END OF ALL ISSUES

---

### 1. REBUILD THE LEARNING INDEX (local only — never committed)

Run: /index
→ Regenerates docs/learning/index.html linking all trios

Verify it did not get staged:
```bash
git status --short | grep "docs/learning/index.html"
# Should be untracked (??) or empty — not staged (A or M)
git reset HEAD docs/learning/index.html 2>/dev/null || true
```

---

### 2. WRITE THE MORNING REPORT (local only — never committed)

Write OVERNIGHT_REPORT.md to the repo root. This file is gitignored.
Do not commit it. Do not push it. It is for you to read locally.

```markdown
# Overnight Build Report
Date: [date]
Duration: [start time] to [end time]
Issues attempted: [N]

## Summary
[2-3 sentences: what was accomplished overall tonight]

## Completed ✅
| Issue | Title | Branch | PR | Concept Taught |
|-------|-------|--------|----|----------------|
| #N | [title] | feature/issue-N | #PR | [concept] |

## Blocked ❌ (tests failed or implementation stuck)
| Issue | Title | Failure Reason |
|-------|-------|----------------|
| #N | [title] | [specific actionable description] |

## Needs Human ⏭ (skipped — requires your decision)
| Issue | Title | What Is Needed |
|-------|-------|----------------|
| #N | [title] | [exactly what question or clarification is needed] |

## Learning Materials Created Tonight
| Concept | Lesson | Drill | Vocab |
|---------|--------|-------|-------|
| [concept] | ✅ lessons/NNNN-x.html | ✅ drills/NNNN-x.html | ✅ vocab/NNNN-x.html |

## Test Status
[overall: X passed, Y failed. Any notable failures described.]

## Draft PRs Awaiting Your Review
| PR | Issue | Title |
|----|-------|-------|
| #PR | #N | [title] |

## Your Morning Checklist
1. Read this report
2. open docs/learning/index.html  ← your learning pack for tonight's work
3. gh pr list --draft             ← see all PRs to review
4. gh pr diff [N]                 ← review a specific PR
5. gh pr merge [N] --squash       ← merge if satisfied
6. gh issue list --label ai-blocked  ← decide what to fix and re-queue
```

Verify the report is not staged:
```bash
git status --short | grep OVERNIGHT_REPORT
# Should be ?? (untracked) — if it shows A or M, unstage it:
git reset HEAD OVERNIGHT_REPORT.md 2>/dev/null || true
```

---

### 3. FINAL CHECKPOINT UPDATE

Write session complete to .claude/memory/checkpoint.md:
```
Session completed: [timestamp]
Status: DONE
Summary: [N] completed, [N] blocked, [N] skipped
```

.claude/memory/ is gitignored — do not commit this.

---

## COMMIT POLICY — SUMMARY

| Path | Committed? | Why |
|------|-----------|-----|
| src/, tests/, lib/ | ✅ YES | This is the work |
| docs/learning/ | ❌ NO | Local study materials |
| OVERNIGHT_REPORT.md | ❌ NO | Morning reading |
| .claude/memory/ | ❌ NO | Session state |
| .claude/plans/ | ❌ NO | Planning artifacts |
| .gitignore (if updated) | ✅ YES | Needed once, on main |

When in doubt: `git diff --cached --name-only` before every commit.
If you see anything other than source code and tests — reset it.

---

## FAILURE HANDLING POLICY

Never stop the run because one issue failed.
Never silently skip — always label and comment on the issue.
Never commit docs or reports onto any branch.

If a situation is genuinely ambiguous or dangerous:
1. Stop work on that issue
2. Label it needs-human
3. Comment with exactly what decision is needed
4. Move to the next issue
