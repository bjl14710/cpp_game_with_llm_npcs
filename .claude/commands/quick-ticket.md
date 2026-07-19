---
description: Generates a well-formed Jira ticket description for a small task — no multi-question interrogation, no Jira API upload. Prints the ticket text so you can paste it into Jira yourself, or copy it in as-is. For quick items not worth the full /idea → /generate-tickets pipeline.
argument-hint: A short description of the task, as rough as you like
---

You are writing ONE Jira ticket description for a small, well-understood
task. This is the lightweight path — no interrogation loop like /idea,
no Jira API calls like /generate-tickets. Just produce clean ticket text.

Use this when:
- The task is small and clear enough that you don't need clarifying questions
- You want to paste the result into Jira yourself, on your own schedule
- It's not worth spinning up a full plan document and epic

Do NOT use this when:
- The idea is genuinely ambiguous — use /idea instead, it will ask questions
- You want several related tickets under one epic — use /generate-tickets
- You want it created in Jira right now — this command does not do that

---

## STEP 1 — QUICK SANITY CHECK ONLY

Unlike /idea, do NOT run a full interrogation. Only ask a clarifying
question if the request is so vague that ANY reasonable engineer would
be stuck (e.g. "fix the bug" with no context at all). For anything with
even a rough shape, proceed directly to writing the ticket — infer
sensible details from the codebase rather than asking.

```bash
# Quick context check — just enough to write specifics, not a deep dive
find src/ lib/ app/ -type f 2>/dev/null | grep -i "$(echo '$1' | grep -oE '[a-zA-Z]+' | head -1)" | head -5
git log --oneline -5
```

If the task references a specific file, class, or component — find it
and use its real name and path in the ticket. Don't guess at names when
you can verify them in 5 seconds.

---

## STEP 2 — WRITE THE TICKET

Use this exact template — the same structure /generate-tickets and the
overnight session expect, so this ticket works with the existing
pipeline the moment it lands in Jira, however it gets there.

```markdown
## What to change
[Specific behavior — one or two sentences, no ambiguity]

## Where it is
[File or component path, verified against the actual codebase if possible]

## How to verify it's done
[A concrete test to run, or an observable behavior to check]

## Constraints
[What must not break — usually one or two lines for a small ticket]

## Branch name
`feature/[PROJECT-KEY]-N-[short-slug]`
(the ticket key will replace N once created — leave as a placeholder)

## Concept for learning materials
[The specific technical concept this illustrates — for the overnight
session's /teach and /drill-me generation. Can be brief for a small ticket.]
```

Keep it proportional to the task. A one-file bug fix doesn't need five
paragraphs under "What to change" — a sentence is fine. Don't pad a
small ticket to look more substantial than it is.

---

## STEP 3 — PRINT AND SAVE

Print the finished ticket in a code block so it's easy to copy directly
into Jira's description field.

Also save it locally for reference (not uploaded, not committed):
```bash
mkdir -p .claude/plans/quick-tickets
# filename derived from the task, dash-case
cat > .claude/plans/quick-tickets/[slug].md << 'EOF'
[the ticket content from Step 2]
EOF
echo "Saved locally to .claude/plans/quick-tickets/[slug].md (not uploaded)"
```

---

## STEP 4 — OFFER THE UPLOAD PATH, DON'T TAKE IT AUTOMATICALLY

After printing the ticket, tell the user their options — but do not
call the Jira API yourself unless explicitly asked to in this same turn:

```
Ticket text ready above — paste it into Jira, or:

To create it directly in Jira right now, say so and I will:
  1. Read .claude/skills/jira/SKILL.md
  2. Create the ticket with create_story()
  3. Label it ready-for-ai
  4. Assign it to you

To have Claude implement it overnight without touching Jira UI:
  Just ask me to create it directly (above), then run nightly-jira.sh
  once it's labelled ready-for-ai.
```

If the user confirms they want it uploaded in the same turn, then and
only then read the Jira skill and create it with the exact ticket text
from Step 2 — do not re-derive or rephrase it.

---

## THE DIFFERENCE FROM THE OTHER PLANNING COMMANDS

| Command | Interrogation | Uploads to Jira | Use for |
|---|---|---|---|
| /idea | Full, multi-question | No — saves a plan doc | Anything non-trivial or ambiguous |
| /quick-ticket (this one) | Minimal, only if truly stuck | No — prints text, optional upload on request | Small, clear, one-off tasks |
| /generate-tickets | None — derives from plans + codebase | Yes — creates Epic + Stories directly | Batch-processing plan docs or full project analysis |

This command exists specifically for the gap between "too small to
interrogate" and "still want a properly formatted ticket that works
with the existing overnight pipeline."
