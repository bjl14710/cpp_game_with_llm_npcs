---
description: Structured debugging loop instead of guessing at fixes
---

For the bug "$1":

Follow this loop strictly. Do not skip steps. Do not guess.

## Step 1 — REPRODUCE
Get the bug reliably reproducing first.
- What exact steps trigger it?
- Does it happen every time, or intermittently?
- What environment, inputs, and state are required?

Do NOT proceed until reproduction is reliable.
If you cannot reproduce, STOP and tell me what information is missing.

## Step 2 — MINIMIZE
Strip the reproduction to the smallest possible case.
- Remove unrelated code
- Reduce input size
- Eliminate external dependencies if possible

Goal: a tiny, isolated reproduction.

## Step 3 — HYPOTHESIZE
State 2-3 possible causes, ranked by likelihood.
For each: what evidence supports it, what would prove it?

Do NOT pick "I'll just try a fix" — pick a hypothesis to test.

## Step 4 — INSTRUMENT
Add logging, breakpoints, or assertions to test the top hypothesis.
The instrumentation should produce evidence that proves or disproves it.

## Step 5 — CONFIRM
Run the instrumented code.
- Did the evidence appear?
- Does it confirm or refute the hypothesis?

If refuted: return to Step 3 with the next hypothesis.
If confirmed: proceed to Step 6.

## Step 6 — FIX
Implement the minimal fix that addresses the PROVEN cause.
Not the symptom. The cause.

## Step 7 — REGRESS
Write a test that would have caught this bug.
Run the full test suite to confirm no new breakage.

## Step 8 — REPORT
Summarize:
- Root cause (one sentence)
- Fix applied
- Test added
- Anything related that should be checked separately
