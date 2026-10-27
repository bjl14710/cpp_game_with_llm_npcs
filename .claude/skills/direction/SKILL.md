---
name: direction
disable-model-invocation: true
description: Create or refresh .claude/project/DIRECTION.md — the north star every routine reads (goal, current phase, pivots, non-goals, constraints, open research questions).
argument-hint: Optional — "refresh", or a pivot you're considering
---

You are maintaining `.claude/project/DIRECTION.md` for this repo. Every routine in this kit (scout, bug-hunt, research-radar, design-scout, propose) reads it to decide what is worth proposing. A vague direction doc produces vague proposals, so precision matters more than length.

Input: "$ARGUMENTS"

## If DIRECTION.md does not exist
1. Read CLAUDE.md, README, `.claude/plans/*.md`, open issues, and `git log --oneline -40`.
2. Draft every section of the template below from what you read. Mark each line you inferred with `(inferred — confirm)`.
3. Ask Brandon to confirm or correct, ONE question at a time, starting with the sections where you were least sure. Typically: the north star, the current phase, and the non-goals. Stop asking once the doc would let a stranger decide whether a proposal fits.
4. Write the file. Keep it under ~120 lines.

## If it exists ("refresh" or a pivot was given)
1. Read it, plus everything merged since its `Last reviewed` date (`git log --since=<date>`) and issues closed since then.
2. Show a short diff of what reality says changed: phase progress, finished milestones, new constraints, ideas rejected since.
3. If a pivot was given, write a "Pivot assessment" in chat: what it would gain, what it abandons, which open issues it invalidates, the cheapest experiment to test it. Don't apply the pivot to the file unless Brandon says so; record it under "Pivots under consideration".
4. Update the file and bump `Last reviewed`.

Commit DIRECTION.md on a branch `docs/direction-YYYY-MM-DD` and open a draft PR (it is team-facing, so it belongs in the repo, unlike docs/learning/). Never push to main.

## Template

```markdown
# Direction: <project name>
Last reviewed: YYYY-MM-DD

## North star (one paragraph)
What this becomes when it's "done enough." Who it's for (me as user / learner / portfolio piece / sellable).

## Current phase
Name of the phase and its exit criteria (3–5 checkable bullets).

## Priorities this phase (ranked)
1. ...
2. ...

## Non-goals (routines must not propose these)
- ...

## Hard constraints
- Architectural rules that are never broken (e.g. schema-as-contract, dry-run only)
- Licensing, privacy, safety, platform limits

## Pivots under consideration
- <pivot> — status: exploring / parked / rejected, and the experiment that would decide it

## Open research questions (research-radar works on these)
- ...

## Quality bars / metrics we track
- metric — where it's measured — current value — target

## What I want to learn from this project
- Concepts that should flow to /teach and /drill-me
```
