---
description: Compact current session into a handoff document
---

Create a handoff document so a fresh agent (or future me) can continue
this work without context.

Save to .claude/memory/handoff-$(date +%Y-%m-%d-%H%M).md

Required sections:

## Original Goal
What we set out to do. The big picture.

## Progress Summary
What's been accomplished, with specific file references.

## Current State
Exactly where we are right now:
- What's working
- What's half-built
- What's untouched

## Next Steps
The concrete next actions, in order. Specific enough that someone could
pick up immediately.

## Key Decisions
Important choices made this session and why. Include alternatives
considered and rejected.

## Files Touched
List with one-line description of each change.

## Gotchas
Anything tricky, surprising, or non-obvious a fresh agent needs to know.
Common pitfalls. Counter-intuitive bits.

## Open Questions
Things still needing answers, with who needs to answer them.

## Test Status
What tests exist, what passes, what fails, what's missing.

## Suggested First Move
For whoever continues this: the single best next action.

Quality bar: a fresh agent reading this should be able to continue
without asking me anything for at least an hour.
