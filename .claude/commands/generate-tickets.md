---
description: Reads CLAUDE.md, the current codebase state, and the active branch to automatically create Jira epics and child tasks. No human drafting required — Claude derives the work from the project context.
argument-hint: Optional focus area, e.g. "authentication" or "phase 2 features" (blank = full project analysis)
---

Before doing anything else, read the relevant skill files:
- .claude/skills/github/SKILL.md  — if creating GitHub issues
- .claude/skills/jira/SKILL.md    — if creating Jira tickets
Both cover auth, issue creation, label management, and error handling.
Do not write any API code from memory — the skills are the reference.

You are a backlog generation agent. Your job is to read this project's
current state and produce real, committed Jira tickets — not draft text,
not suggestions, actual tickets created via jira-cli.

You create: one Epic per theme, with child Story/Task tickets underneath.
Each child task is scoped for a single overnight autonomous session and
named after the branch pattern it will create (feature/issue-KEY).

## BEFORE STARTING — PREREQUISITES CHECK

```bash
# Verify jira-cli is installed and authenticated
jira issue list --plain --paginate 1 2>/dev/null \
  && echo "jira-cli OK" \
  || echo "ERROR: jira-cli not configured — run: jira init"

# Confirm environment variables are set
echo "JIRA_API_TOKEN: ${JIRA_API_TOKEN:+set}"
echo "JIRA_PROJECT:   ${JIRA_PROJECT:+set}"

# Get the current branch name — child tasks are named after this pattern
CURRENT_BRANCH=$(git branch --show-current)
echo "Current branch: $CURRENT_BRANCH"

# Get existing open tickets to avoid duplicates
jira issue list \
  --jql "project = $JIRA_PROJECT AND status != Done AND status != Closed" \
  --plain --columns KEY,SUMMARY \
  --paginate 50
```

If JIRA_PROJECT is not set, stop and ask the user to set it:
```bash
export JIRA_PROJECT="YOUR-PROJECT-KEY"  # e.g. SILM, NPC, TRD
echo 'export JIRA_PROJECT="YOUR-PROJECT-KEY"' >> ~/.bashrc
```

---

## STEP 0 — PROCESS ALL PENDING PLAN DOCS FIRST (batch mode)

This step runs BEFORE any codebase-derived ticket generation. It exists
because /idea can be run multiple times in a row before /generate-tickets
is ever called — each call to /idea produces one plan doc. This step
turns EVERY unprocessed plan into tickets in one pass, so a person can
do idea → scaffold → idea → scaffold → idea → scaffold as many times as
they want, then run /generate-tickets once to turn all of them into
Jira tickets in a single batch.

```bash
# Find all plan docs that have NOT yet been turned into tickets.
# A processed plan is marked by moving it to .claude/plans/processed/
# after tickets are created from it.
mkdir -p .claude/plans/processed

echo "=== Pending plan docs (not yet ticketed) ==="
ls .claude/plans/*.md 2>/dev/null || echo "None found"
```

If pending plan docs exist, process each one in turn:

For each `.claude/plans/[name].md` found (skip the `processed/` subfolder):

1. Read the full plan document
2. Extract from it:
   - The feature name (from the title)
   - The "Suggested GitHub Issues" or "Implementation Order" section
   - The acceptance criteria
   - Any explicit branch names already specified
3. Create ONE Epic for this plan (if the plan is large enough to need one)
   or add its tasks under an existing relevant Epic if one already
   exists for the same feature area
4. Create child Story/Task tickets for each implementation step in the
   plan — following the same ticket template as Step 4 below, but using
   the plan's own acceptance criteria and constraints verbatim rather
   than re-deriving them from the codebase
5. Label every created ticket `ready-for-ai`
6. After tickets are successfully created for this plan, mark it processed:
   ```bash
   mv .claude/plans/[name].md .claude/plans/processed/[name].md
   ```
   This prevents the same plan from generating duplicate tickets if
   /generate-tickets is run again later.

Print a summary after processing all pending plans:
```
Processed N pending plan(s):
  weapon-shop.md       → Epic PROJ-10, 4 tickets created
  npc-memory.md        → Epic PROJ-15, 3 tickets created
  visual-testing.md    → added to existing Epic PROJ-8, 5 tickets created

All moved to .claude/plans/processed/
```

If NO pending plan docs exist, print "No pending plans — proceeding to
codebase analysis" and continue to Step 1 below.

**Important:** Step 0 handles plans from /idea sessions — your actual
intent. Step 1 onward handles deriving NEW work from reading the
codebase directly, for when you want Claude to find gaps itself rather
than implementing something you specifically asked for. Both can run
in the same /generate-tickets call — Step 0 always runs first.

---

## STEP 1 — READ THE PROJECT CONTEXT

Read all of these before generating any tickets:

**CLAUDE.md** — project purpose, tech stack, hard rules, workflow.
This is the source of truth for what the project is trying to be.

**README.md** — user-facing description, current state, roadmap if any.

**Current codebase structure:**
```bash
# What exists right now
find src/ lib/ app/ -type f | head -40 2>/dev/null || \
find . -name "*.py" -o -name "*.cpp" -o -name "*.ts" | \
  grep -v node_modules | grep -v build | head -40

# Recent commits — what direction is work moving
git log --oneline -20

# Open files / incomplete work signals
grep -rn "TODO\|FIXME\|HACK\|XXX\|PLACEHOLDER\|NotImplemented" \
  src/ lib/ app/ 2>/dev/null | head -30
```

**Existing tickets (already read above)** — don't create duplicates.

**Active branch:**
```bash
git branch --show-current
# The child task branch pattern will be:
# feature/[JIRA-KEY] — e.g. feature/SILM-42
```

---

## STEP 2 — ANALYSE AND PLAN TICKETS

From the context above, identify themes of work. Each theme becomes one Epic.
Under each Epic, identify 2-6 specific, self-contained tasks.

A good task for overnight autonomous work is:
- Completable in one session (2-6 hours of AI work)
- Has clear acceptance criteria derivable from the codebase
- Produces a testable result
- Does not require a human decision mid-implementation

A bad task is:
- "Improve the system" (too vague)
- "Complete the entire trading engine" (too large)
- "Fix the bug" (no context)
- A duplicate of an existing open ticket

**For "$1" focus:** if an argument was given, limit Epic generation to
themes related to that topic. Otherwise generate Epics covering all
visible gaps and next logical steps in the codebase.

Think through:
1. What is clearly missing compared to what CLAUDE.md describes as the goal?
2. What does the TODO/FIXME grep reveal?
3. What would the next logical features be based on the recent commit history?
4. What test coverage gaps exist?
5. What documentation or tooling is missing?

---

## STEP 3 — CREATE EPICS

For each identified theme, create one Epic:

```bash
# Create an Epic and capture its key
EPIC_KEY=$(jira epic create \
  --project "$JIRA_PROJECT" \
  --summary "[Epic] [Theme name — clear and specific]" \
  --body "## Goal
[What this epic delivers when complete]

## Motivation
[Why this is the next logical work given the current project state]

## Definition of Done
[What does complete look like for this theme]

## Source
Generated from codebase analysis on $(date +%Y-%m-%d).
Branch at generation time: $(git branch --show-current)
Commit: $(git rev-parse --short HEAD)" \
  --no-input \
  --plain 2>/dev/null | grep -oE '[A-Z]+-[0-9]+' | head -1)

echo "Created Epic: $EPIC_KEY"
```

Save the Epic keys — you need them to parent the child tasks.

---

## STEP 4 — CREATE CHILD TASKS (Stories or Sub-tasks)

For each task under an Epic, create a Story or Sub-task with a branch
name in the title so the overnight session knows what to create:

```bash
# The branch that the overnight session will create for this task
BRANCH_NAME="feature/${EPIC_KEY}-$(echo '[task-slug]' | tr ' ' '-' | tr '[:upper:]' '[:lower:]')"

TASK_KEY=$(jira issue create \
  --project "$JIRA_PROJECT" \
  --type "Story" \
  --summary "[Clear, specific task title]" \
  --body "## What to change
[Specific behavior or addition — not 'improve' but exactly what]

## Where it is
[File or component path — helps Claude navigate fast]

## How to verify it's done
[Concrete acceptance criteria — a test to run or output to check]

## Constraints
[What must not break, what must stay compatible]

## Branch name
\`$BRANCH_NAME\`

## Epic
$EPIC_KEY

## Generated from
Codebase analysis on $(date +%Y-%m-%d).
Commit: $(git rev-parse --short HEAD)" \
  --label "ready-for-ai" \
  --no-input \
  --plain 2>/dev/null | grep -oE '[A-Z]+-[0-9]+' | head -1)

echo "Created Task: $TASK_KEY (branch: $BRANCH_NAME)"

# Link the task to its Epic
jira epic add "$EPIC_KEY" "$TASK_KEY" 2>/dev/null \
  && echo "  Linked $TASK_KEY to Epic $EPIC_KEY" \
  || echo "  Warning: could not link to Epic — link manually in Jira UI"
```

Repeat for each task under each Epic.

**For Sub-tasks** (if your Jira project uses Sub-task type instead of Story):
```bash
# Sub-task syntax — parent is the story/task, not the epic
jira issue create \
  --project "$JIRA_PROJECT" \
  --type "Sub-task" \
  --parent "$PARENT_TASK_KEY" \
  --summary "[Sub-task title]" \
  --body "[description]" \
  --label "ready-for-ai" \
  --no-input
```

Note: The correct type string for subtasks is `Sub-task` with a capital S and hyphen — not `subtask` or `sub-task`. Case and string matter exactly.

---

## STEP 5 — VERIFY THE CREATED TICKETS

After creating all tickets, verify they exist and are labelled correctly:

```bash
# Show all newly created ready-for-ai tickets
jira issue list \
  --jql "project = $JIRA_PROJECT AND labels = 'ready-for-ai' AND created >= -1h" \
  --plain \
  --columns KEY,SUMMARY,STATUS

# Show the epics created
jira epic list \
  --plain \
  --paginate 10
```

---

## STEP 6 — PRODUCE A SUMMARY

After creating all tickets, write a local summary file
(NOT committed — local only):

```
.claude/memory/ticket-generation-[date].md
```

Content:
```markdown
# Ticket Generation Summary
Date: [date]
Branch at time of generation: [branch]
Commit: [hash]
Project: [JIRA_PROJECT]

## Epics Created
| Epic Key | Summary | Child Tasks |
|----------|---------|-------------|
| PROJ-N | [summary] | PROJ-N1, PROJ-N2, PROJ-N3 |

## Tasks Created
| Key | Summary | Branch | Label |
|-----|---------|--------|-------|
| PROJ-N1 | [summary] | feature/PROJ-N1-slug | ready-for-ai |

## What Was NOT Ticketed
[Things seen in the codebase that were not turned into tickets and why —
existing tickets, out of scope, too vague to specify, etc.]

## Suggested Overnight Run Order
1. [task key and why it should go first]
2. [next...]
```

---

## JIRA FIELD NOTES

Different Jira instances use different field names and issue type names.
If a creation command fails with a 400 error:

```bash
# Check what issue types your project supports
jira issue list --plain --paginate 1 2>/dev/null
jira --help | grep -A5 "issue create"

# Common type name variants to try:
# "Story" / "Task" / "Sub-task" / "Subtask" / "Sub Task"
# Your Jira admin controls these — check with them if unsure
```

For Enterprise Jira with required custom fields (team, component, sprint):
```bash
jira issue create \
  --project "$JIRA_PROJECT" \
  --type "Story" \
  --summary "[title]" \
  --body "[description]" \
  --label "ready-for-ai" \
  --component "[component if required]" \
  --assignee "$(jira me 2>/dev/null || echo '')" \
  --no-input
```

If your instance requires fields that jira-cli cannot set via flags,
use the interactive mode (drop `--no-input`) and fill the form once,
then script subsequent calls based on the first successful creation.

---

## CONNECTING TO THE OVERNIGHT SESSION

Once tickets are created with the `ready-for-ai` label, the overnight
session picks them up automatically. The branch name in the ticket body
tells Claude exactly what branch to create.

The overnight session uses:
```bash
jira issue list \
  --jql "project = $JIRA_PROJECT AND labels = 'ready-for-ai' AND status = 'To Do'" \
  --plain --json
```

After completing each task, it transitions:
```bash
jira issue transition [KEY] "In Progress"   # when starting
jira issue transition [KEY] "In Review"     # when draft MR is opened
# "Done" is set by YOU after reviewing and merging the MR
```
```

---

## RUNNING THIS COMMAND

```
/generate-tickets              # full project analysis
/generate-tickets "phase 2"    # focus on phase 2 features
/generate-tickets "testing"    # focus on test coverage gaps
/generate-tickets "device X"   # focus on a specific component
```

The command is non-destructive if run multiple times — it reads
existing tickets first and avoids duplicates. Rerunning it after
completing a phase of work will pick up the next logical set of gaps.
