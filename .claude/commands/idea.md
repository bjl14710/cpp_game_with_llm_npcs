---
description: Turn a raw idea into a structured plan document. Asks clarifying questions first, then produces a full implementation plan saved to .claude/plans/. The plan can then be handed to /develop or the overnight session.
argument-hint: Your idea, as rough or detailed as you like
---

You are turning "$1" into a structured, actionable plan document.

This command has two phases:
- PHASE 1: Interrogate the idea until it is unambiguous
- PHASE 2: Produce the plan document

Do not skip to Phase 2 until Phase 1 is complete.

---

## PHASE 1 — INTERROGATE THE IDEA

Read the codebase first so your questions are grounded in reality:
```bash
find src/ lib/ app/ -type f 2>/dev/null | head -30
git log --oneline -10
cat CLAUDE.md 2>/dev/null || true
```

Then ask clarifying questions — ONE at a time, wait for the answer,
then ask the next. Do not dump a list of questions all at once.
A question only gets asked if you genuinely cannot write a good plan
without knowing the answer.

Things worth asking about (pick the most important ones):

**Scope:**
- What does done look like? What can a user DO when this is complete?
- What is explicitly OUT of scope for this version?
- Is this a new feature, a change to existing behaviour, or a fix?

**Users and context:**
- Who uses this and in what situation?
- What do they have / know before they reach this feature?

**Technical constraints:**
- Does this touch existing code that other things depend on?
- Any performance, security, or compatibility requirements?
- Any third-party APIs or libraries involved?

**Edge cases:**
- What happens when the input is invalid or missing?
- What's the failure mode and how should it be handled?

**Priority:**
- Does this need to be done in a specific order relative to other work?
- Is there a simpler version that delivers most of the value?

Stop asking when you have enough to write a plan that could be handed
to a developer (or the overnight session) without them needing to ask
you anything.

---

## PHASE 2 — PRODUCE THE PLAN DOCUMENT

Save to `.claude/plans/[slug].md` where slug is a short dash-case
name derived from the idea. Example: "add weapon inventory" →
`.claude/plans/weapon-inventory.md`

The document structure:

```markdown
# Plan: [Feature Name]
Date: [date]
Status: READY FOR IMPLEMENTATION
Estimated complexity: S / M / L / XL

## The Idea (one paragraph)
[What this is, in plain language. Someone unfamiliar with the codebase
should understand this in 30 seconds.]

## Goal
[One sentence: what a user can DO when this is shipped that they
cannot do now.]

## Out of Scope (this version)
[Explicit list of things NOT included. This prevents scope creep
during implementation.]

## Affected Areas
[Files, modules, or systems this touches. Be specific.]
- `src/path/to/file.py` — [why it changes]
- `src/path/to/other.cpp` — [why it changes]
- New files needed: [list]

## Implementation Order
[Ordered steps. Each step should be independently committable.]

1. [Step 1 — what it produces]
2. [Step 2 — builds on step 1]
3. [Step 3 — etc.]

## Acceptance Criteria
[Testable conditions that define "done". Each one should be
verifiable by running a test or observing a specific behaviour.]

- [ ] [Given X, when Y, then Z]
- [ ] [Given X, when Y, then Z]
- [ ] Tests pass: [specific test file or command]

## Edge Cases and Error Handling
[What can go wrong and how each case is handled.]

| Situation | Expected behaviour |
|-----------|-------------------|
| [case] | [what happens] |

## Open Questions
[Anything still unresolved that will need a decision during implementation.
If there are none, write "None — plan is complete."]

## Suggested GitHub Issues
[If this plan should become overnight issues, list them here as a
starting point for /plan-github:]

1. [Issue title] — [one line scope]
2. [Issue title] — [one line scope]
3. [Issue title] — [one line scope]
```

---

## AFTER SAVING THE PLAN

Tell the user:
1. Where the plan was saved
2. How to turn it into GitHub issues:
   `/plan-github "[feature name]"` — or offer to run it now
3. How to start implementation immediately:
   `/develop [feature name]` — uses the plan as the spec
4. How to queue it for overnight:
   Add the suggested issues to GitHub, label ready-for-ai,
   then `bash ~/scripts/nightly.sh`

---

## TONE

Ask questions like a sharp senior engineer reviewing a spec —
direct, specific, not adversarial. The goal is to surface
assumptions before they become bugs, not to gatekeep.

If the idea is already clear enough to plan without questions,
say so briefly and go straight to the plan.
