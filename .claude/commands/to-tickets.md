---
description: Slice a feature or chunk of work into well-formed, ready-to-paste tickets (Jira / GitHub Issues / Linear). Draft-only — you stay in control of what lands in the tracker.
argument-hint: The feature or work to break into tickets
---

Break the work described in "$1" into well-formed tickets I can paste into my
issue tracker. This is DRAFT-ONLY — you produce ticket text, you do not call
any tracker API. If "$1" is vague, ask one or two clarifying questions first
(or suggest I run /grill-me to nail down scope before slicing).

## Step 1 — Understand the Work
Read any relevant spec (.claude/plans/spec-*.md if one exists), the codebase,
and the request. Understand the actual scope before slicing it.

## Step 2 — Slice Vertically, Not Horizontally
Break the work into tickets that each deliver a thin, complete, shippable
slice — not horizontal layers that can't ship alone.
- GOOD (vertical): "User can create a draft" = schema + API + UI + tests for
  that one capability, all together.
- BAD (horizontal): "Build all the database tables", then "build all the
  APIs", then "build all the UI" — none ships independently.
Each ticket should be independently valuable and, ideally, independently
deployable. Order them so each builds on the last.

## Step 3 — Write Each Ticket
For each ticket, produce:

**Title** — imperative and specific ("Add CSV export to oscilloscope block",
not "Export stuff").

**Description** — 1-3 sentences of context: what and why.

**Acceptance Criteria** — testable, in given/when/then form where it fits.
These define "done" for the ticket.

**Technical Notes** — relevant implementation pointers: files likely touched,
dependencies, gotchas. Keep brief.

**Estimate** — a rough size. Use story points (1,2,3,5,8 — Fibonacci) for
Jira-style, or S/M/L if I prefer. Flag anything an 8+ as "should probably be
split further."

**Dependencies** — which other tickets (by title) must land first, if any.

**Labels / Type** — bug / feature / chore / spike, plus any obvious component
labels.

## Step 4 — Output
Present the tickets as clean, copy-paste-ready blocks — each ticket clearly
separated so I can paste them one at a time. Use this shape:

---
**[FEATURE] Add CSV export to oscilloscope block**

**Description:** Let users export the captured waveform buffer to CSV from
the scope block's context menu.

**Acceptance Criteria:**
- Given a populated buffer, when the user clicks Export CSV, then a file
  dialog opens defaulting to ~/Desktop
- Given an empty buffer, when the user clicks Export, then a clear warning
  shows and no file is written
- Exported CSV has timestamp and voltage columns, ISO-8601 timestamps

**Technical Notes:** Touches src/oscilloscope/scope.py (buffer access),
src/gui/scope_block.py (menu item), new src/io/csv_exporter.py. pandas
already available.

**Estimate:** 3 points
**Dependencies:** none
**Type:** feature | **Component:** oscilloscope, io
---

Lead with a one-line summary of how many tickets you created and the suggested
order. After the tickets, note any work you deliberately deferred or any
ambiguity I should resolve before starting.

## Step 5 — Offer to Save
Offer to save the ticket set to .claude/plans/tickets-<feature>.md as a record.

## Notes
- Default to Jira vocabulary (story points, epics) since that's the common
  case, but if I mention GitHub Issues or Linear, adapt (GitHub uses labels +
  milestones, no native points; Linear uses its own estimate scale).
- Don't invent scope. If something's ambiguous, list it as an open question
  rather than guessing a ticket into existence.
- Keep tickets small. If you're writing an 8-point ticket, ask whether it
  should be an epic with sub-tickets instead.
