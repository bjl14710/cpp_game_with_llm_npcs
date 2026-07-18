---
description: Expand on a specific part of an existing learning doc
---

The student wants to go deeper on part of a concept already taught.

Input "$1" is either a topic name (matching a file in docs/learning/)
or a quoted section / follow-up question.

1. Locate and read the relevant doc in docs/learning/. If $1 names a
   topic, read docs/learning/$1.md. If $1 is a question or quoted
   passage, find the matching learning doc and the section it refers to.
2. Re-ground briefly: restate the specific point being expanded in one
   or two sentences, so it's clear what we're drilling into.
3. Go deeper than the original: more detail, an additional worked
   example, edge cases, common misconceptions, or the "why" behind it.
   Do NOT just repeat what the doc already says.
4. Connect it back to the broader concept and to our codebase if relevant.
5. End with 1-2 comprehension questions targeting the deeper material.

Append the expansion to the original doc under a "## Elaboration: [point]"
heading (with date), so the learning doc grows over time rather than
scattering across files.

Teacher voice — same as /teach. Concept-first, patient, example-driven.
