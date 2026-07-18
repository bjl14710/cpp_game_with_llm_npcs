---
description: Build a glossary of relevant terminology from this repo or session, with definitions, project examples, usage sentences, and contrasts with similar terms
---

For the topic "$1" (or, if no argument given, derive terms from the current
session and the repo's recent work):

Your goal is to make me fluent in the vocabulary of what I'm building — not
just dictionary definitions, but real understanding I can use in conversation.

## Step 1 — Select the Terms

If I gave a topic in "$1", focus on terminology related to it.
If I gave nothing, scan the recent session and the repo (code, comments,
docs, dependencies, file names) and identify the 8-15 most important terms
I should understand to work confidently in this project.

Prioritize terms that:
- Appear repeatedly in the code or our conversation
- Are domain-specific or technical jargon
- Are easy to confuse with similar terms
- I've used loosely or seemed unsure about
- Are foundational (other concepts depend on them)

Skip terms that are trivially obvious or universally known.

## Step 2 — Define Each Term

For EACH term, provide all of the following:

### [Term Name]

**Plain definition:** One or two sentences in clear language. No jargon
inside the definition unless that jargon is itself defined elsewhere in
this glossary.

**In this project:** A concrete example of where and how this term applies
in THIS repo. Reference actual files, classes, or features when possible
(e.g., "In src/devices/scope.py, the Oscilloscope class is an *instance* of
the Device *base class*"). If the term doesn't appear in the repo yet but
is relevant to where we're headed, say so.

**Example sentences:** 2-3 sentences using the term correctly the way an
engineer would in real conversation or documentation. These should model
natural usage so the word becomes part of my active vocabulary, not just
something I recognize.

**Often confused with:** If there are similar or related terms, name them
and explain the distinction clearly. Spell out the nuance — what they share,
where they diverge, and when to use one versus the other. Use a brief
contrast like:
  - Term A: [what it is, when to use]
  - Term B: [what it is, when to use]
  - Key difference: [the one thing that separates them]
(Example pairs worth contrasting when relevant: container vs virtual machine,
process vs thread, concurrency vs parallelism, library vs framework,
authentication vs authorization, compiler vs interpreter, mutable vs
immutable, synchronous vs asynchronous, declaration vs definition,
class vs instance, argument vs parameter.)

**Why it matters here:** One sentence on why understanding this term helps
me work on this specific project.

## Step 3 — Relationships

After the individual terms, add a short "How these connect" section: a few
sentences (or a Mermaid diagram if it genuinely helps) showing how the terms
relate to each other. Vocabulary sticks better when I see the map, not just
the pins.

## Step 4 — Quick Self-Check

End with 3-5 short questions that test whether I actually understand the
distinctions — especially the "often confused with" pairs. Don't give the
answers immediately; let me attempt them. Offer to check my answers after.

## Output

Save the glossary to docs/learning/glossary-$1.md (if no topic, use
docs/learning/glossary-<date>.md). If a glossary file already exists,
append new terms rather than duplicating existing ones, and note which
terms are new.

Keep tone clear and direct. The goal is confident fluency, not a textbook.
