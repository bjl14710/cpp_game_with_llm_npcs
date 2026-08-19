---
description: Work through tonight's ready-for-ai GitHub issues systematically. Builds draft PRs, generates learning materials per feature, and produces a morning report.
argument-hint: Optional focus area (blank = all ready-for-ai issues)
---

You are running an overnight autonomous development session on this
repository. Use the overnight-coordinator agent to manage the full session.
You have full permission to read, write, commit, push, and open draft PRs.
You do NOT have permission to merge to main or close PRs after opening them.

## SETUP (run once at session start)

```bash
# 1. Ensure we are on main and up to date
git checkout main
git pull

# 2. Rebuild the knowledge graph so you navigate current code, not stale code
# (only if graphify is installed — skip silently if not)
graphify update . --force 2>/dev/null || echo "graphify not installed — skipping"

# 3. Check for a prior checkpoint (resume if interrupted)
cat .claude/memory/checkpoint.md 2>/dev/null || echo "No checkpoint — starting fresh"

# 4. Read tonight's issues
gh issue list --label "ready-for-ai" --json number,title,body,labels \
  --order created --limit 20
```

If a checkpoint exists and shows IN PROGRESS or FAILED stages, resume
from there rather than starting over.

Write the session start to .claude/memory/checkpoint.md:
```
# Checkpoint
Session started: [timestamp]
Goal: Work through all ready-for-ai issues
Status: IN PROGRESS
Issues to process: [list from gh output]
Issues completed: []
Issues failed: []
Issues skipped: []
```

---

## FOR EACH ISSUE (in number order, lowest first)

### A. READ AND ASSESS

```bash
gh issue view [N] --comments
```

Ask: Is this issue clear enough to attempt autonomously?

**Blockers that cause a SKIP:**
- Requires a design decision not documented in the issue
- Depends on another issue that isn't complete yet
- Requires credentials, API keys, or external resources not available
- Scope is too vague to produce a verifiable result

If blocked, label and move on — do NOT guess at intent:
```bash
gh issue edit [N] --add-label "needs-human" --remove-label "ready-for-ai"
gh issue comment [N] --body "Skipped overnight: [specific reason]. Please clarify before re-queuing."
```

### B. ORIENT

Invoke the git-historian agent:
"Use the git-historian agent to understand the code area this issue will touch."

This surfaces hot files, recent decisions, and things NOT to redo.
Read .claude/memory/history-brief.md after it completes.

### C. MARK IN PROGRESS

```bash
gh issue edit [N] --add-label "ai-in-progress" --remove-label "ready-for-ai"
gh issue comment [N] --body "Starting overnight implementation. Planning now."
```

### D. BRANCH

```bash
git checkout main
git checkout -b feature/issue-[N]
```

### E. PLAN

Use /plan-feature [concept from issue title] and wait for a plan.
Save the plan to .claude/plans/issue-[N].md

Do not implement until you have a written plan.

### F. IMPLEMENT

Use the appropriate specialist agents for this repo (see repo-specific
section below). Follow the plan. Write complete code — no placeholders.

If you hit a problem after 2 attempts on the same sticking point, SKIP
the issue and mark it ai-blocked. Do not spiral.

### G. TEST

Run the project's test suite. The exact commands are in the repo-specific
section below. Tests MUST pass before committing.

If tests fail after 2 fix attempts: skip, mark blocked, move on.
```bash
gh issue edit [N] --remove-label "ai-in-progress" --add-label "ai-blocked"
gh issue comment [N] --body "Blocked: tests failing after 2 attempts. [describe failure]. Needs human attention."
git checkout main
git branch -D feature/issue-[N]
```

### H. COMMIT AND PUSH

```bash
git add .
git commit -m "fix(#[N]): [issue title, shortened]"
git push origin feature/issue-[N]
```

### I. OPEN DRAFT PR

Generate the PR description using the /mr-description command, then:

```bash
gh pr create \
  --title "fix(#[N]): [issue title]" \
  --body "[output from /mr-description]" \
  --base main \
  --head feature/issue-[N] \
  --draft
```

Label the issue complete and close it with a reference to the PR:
```bash
gh issue edit [N] \
  --remove-label "ai-in-progress" \
  --add-label "ai-completed"
gh issue close [N] \
  --comment "Implemented. Draft PR open for review — see above."
```

### J. GENERATE LEARNING MATERIALS

**Identify the primary concept:**
Ask yourself: "What is the single most important technical concept a
developer would need to understand to maintain or extend this change?"

Guidelines:
- Choose the concept, not the feature name
  ("triple-channel SCPI addressing" not "E3631A support")
- Should be teachable in one lesson
- Should have testable recall in a drill
- Should introduce vocabulary worth learning

Then generate all three artifacts. In overnight/automated context:
- Generate the HTML files fully
- Skip in-chat interactive knowledge checks (no user present)
- Make the HTML content thorough — it is the only output
- Update PROGRESS.md with what was covered

```
/teach [concept]
```
This creates docs/learning/lessons/NNNN-[concept].html

```
/drill-me [concept]
```
This creates docs/learning/drills/NNNN-[concept].html

```
/terms [concept]
```
This creates docs/learning/vocab/NNNN-[concept].html
and updates docs/learning/GLOSSARY.md

Commit the learning materials on the same feature branch:
```bash
git add docs/learning/
git commit -m "docs(learning): add lesson, drill and vocab for [concept] (from issue #[N])"
git push origin feature/issue-[N]
```

### K. UPDATE CHECKPOINT

Append to .claude/memory/checkpoint.md:
```
Issue #[N]: [COMPLETED | FAILED | SKIPPED]
  Branch: feature/issue-[N]
  PR: #[PR number if opened]
  Concept taught: [concept]
  Learning files: lessons/NNNN-x.html, drills/NNNN-x.html, vocab/NNNN-x.html
  Notes: [anything notable]
```

Return to main: `git checkout main`

---

## END OF ALL ISSUES

### 1. Rebuild the learning index

```
/index
```

Commit:
```bash
git add docs/learning/index.html
git commit -m "docs(learning): rebuild index after overnight run $(date +%Y-%m-%d)"
git push origin main
```

### 2. Write the morning report

Write OVERNIGHT_REPORT.md to the repo root:

```markdown
# Overnight Build Report
Date: [date]
Session duration: [start] to [finish]
Issues attempted: [N]

## Summary
[2-3 sentences on what was accomplished overall]

## Completed ✅
| Issue | Title | Branch | PR | Concept Taught |
|-------|-------|--------|----|----------------|
| #N | [title] | feature/issue-N | #PR | [concept] |

## Failed ❌
| Issue | Title | Failure Reason |
|-------|-------|----------------|
| #N | [title] | [why — specific, actionable] |

## Skipped (needs human) ⏭
| Issue | Title | Reason |
|-------|-------|--------|
| #N | [title] | [what needs clarification] |

## Learning Materials Created
| Concept | Lesson | Drill | Vocab |
|---------|--------|-------|-------|
| [concept] | ✅ | ✅ | ✅ |

## Test Status
[overall pass/fail and any notable failures]

## Suggested First Move
[The single most useful thing to do when you open your laptop]

## Draft PRs Awaiting Review
[list with PR numbers and one-line description each]
```

Commit and push the report:
```bash
git add OVERNIGHT_REPORT.md .claude/memory/checkpoint.md
git commit -m "chore: overnight report $(date +%Y-%m-%d)"
git push origin main
```

---

## LEARNING CONCEPT SELECTION — GENERAL GUIDANCE

These guidelines apply to all repos unless overridden in a repo-specific
overnight-session command.

Choose a concept that:
- Is non-obvious (skip things any developer already knows)
- Will appear again in this codebase (worth learning for maintainability)
- Can be explained in one lesson without splitting into prerequisite lessons
- Has clear vocabulary (terms worth adding to the glossary)
- Produces useful recall questions in a drill

Avoid:
- Concepts named after specific model numbers or vendor names
  ("E3631A" is not a concept; "triple-channel PSU SCPI addressing" is)
- Concepts so generic they produce a useless lesson
  ("adding a class" — skip this)
- Concepts already well-covered in the existing lesson library
  (check docs/learning/PROGRESS.md first)

---

## FAILURE HANDLING POLICY

This policy applies to the entire session.

**Never stop the run because one issue failed.**
**Never silently skip — always label and comment.**
**Never attempt a risky or irreversible action without a prior commit.**

If at any point the session hits a situation that is genuinely ambiguous,
dangerous, or requires a human decision not covered by the issue:
1. Stop work on that issue
2. Label it needs-human
3. Leave a detailed comment explaining exactly what decision is needed
4. Move to the next issue

The goal is to maximize completed work, not to force through stuck issues.

---

## NOTE ON AUTOMATED LEARNING GENERATION

In a live session, /teach, /drill-me, and /terms end with in-chat
knowledge checks. In overnight autonomous mode, skip the interactive
questions and put that energy into making the HTML content more thorough
instead. The HTML is the deliverable — the in-chat conversation is not.
