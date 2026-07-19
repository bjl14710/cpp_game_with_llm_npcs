---
description: Build a terminology reference AND an interactive HTML vocabulary drill from this repo or session — flashcards, two-way recall, and "which term applies" discrimination questions to force you to remember industry terms.
argument-hint: A topic to pull terms from (blank = derive from the repo/session)
---

For the topic "$1" (or, if blank, derive terms from the current session and
the repo's recent work):

Produce two artifacts that work together, like /teach (lesson) and /drill-me
(drill) do for concepts:
1. A glossary reference (markdown) — the lookup document.
2. An interactive HTML vocabulary drill — the active-recall artifact that
   forces you to retrieve the terms from memory, not just reread them.

The goal is fluency in the vocabulary of what you're building — being able to
produce the term and its meaning on demand, including the nuance between
easily-confused terms.

## Step 1 — Select the Terms
If "$1" is given, focus there. Otherwise scan the session and repo (code,
comments, docs, dependencies, file names) and pick the 8-15 most important
terms. Prioritize terms that repeat, are domain-specific or industry jargon,
are easy to confuse, that the user used loosely, or that are foundational.
Skip the trivially obvious.

## Step 2 — For Each Term, Capture
- Plain definition — one or two clear sentences.
- In this project — a concrete example referencing actual files/classes/
  features in THIS repo (or note it's not present yet but relevant).
- Example sentence — how an engineer uses the term naturally, to build active
  vocabulary.
- Confused with — any similar term, and the key distinction (what they share,
  where they diverge, when to use each).

Web-search anything accuracy-sensitive rather than trusting memory; a wrong
definition drilled into memory is worse than none. Note sources.

## Step 3 — Write the Glossary Reference
Save/append to docs/learning/GLOSSARY.md (the shared glossary the /teach and
/drill-me loop also use — keep wording consistent). If it exists, append new
terms rather than duplicating, noting which are new. Each entry: term,
definition, project example, example sentence, confused-with note.

## Step 4 — Produce the HTML Vocabulary Drill
Self-contained file at docs/learning/vocab/NNNN-<dash-case-topic>.html
(increment NNNN; start 0001). Inline CSS/JS, opens via file://, no external
deps. NO localStorage — state in JS memory. Clean, readable.

Active-recall modes, selectable from a start screen (plus "all modes" and a
shuffle toggle):

Mode 1 — Flashcards (two-way). Show one side, the user actively recalls the
other, then flips to check. Randomly mix direction per card:
- Term -> recall the definition (and its project example)
- Definition -> recall the term
After flipping, self-rate "knew it" / "didn't know it." Two-way recall is far
stronger than one direction — it builds recognition AND production.

Mode 2 — Type the term. Show a definition or a real usage from the repo; the
user types the term; check (case-insensitive, trim) and show correct/incorrect
plus the full entry. Typing forces production, not recognition.

Mode 3 — Discrimination (the confusable pairs). For terms with a "confused
with" partner, present a scenario, definition, or code snippet and ask "which
term applies here?" as multiple choice between the confusable options. This is
where the nuance gets hammered in — container vs VM, process vs thread, etc.

Multiple-choice integrity (Mode 3 and any MC):
- Mark correct option data-correct="true", wrong "false".
- Shuffle options in the DOM on load (Fisher-Yates); click handler checks the
  attribute, never position.
- No length/detail tell: keep option text roughly equal length, and make each
  option's data-why DETAILED for EVERY option — a wrong option's explanation
  should fully explain why that term does NOT apply here, at the same depth as
  the correct one. Detail teaches on every option and never signals the answer.

Reference pattern:

  <div class="question" data-question>
    <p>A lightweight isolated environment sharing the host OS kernel — which term?</p>
    <ul class="options">
      <li data-correct="true"  data-why="Correct. A container virtualizes at the OS level — it shares the host kernel and isolates only the process/filesystem/network view, which is why it's lightweight and starts in milliseconds.">Container</li>
      <li data-correct="false" data-why="Wrong. A virtual machine virtualizes hardware and runs its own full guest OS kernel on a hypervisor — heavier and slower to boot, precisely because it does NOT share the host kernel the way this describes.">Virtual machine</li>
    </ul>
    <p class="feedback" hidden></p>
  </div>
  <script>
  function shuffle(a){for(let i=a.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[a[i],a[j]]=[a[j],a[i]];}return a;}
  document.querySelectorAll('[data-question]').forEach(q=>{
    const list=q.querySelector('.options');
    shuffle([...list.children]).forEach(li=>list.appendChild(li));
    const fb=q.querySelector('.feedback');
    list.querySelectorAll('li').forEach(li=>li.addEventListener('click',()=>{
      const ok=li.dataset.correct==='true';
      fb.hidden=false; fb.textContent=(ok?'✓ Correct. ':'✗ Not quite. ')+li.dataset.why;
      fb.style.color=ok?'green':'crimson';
    }));
  });
  </script>

Also include: progress (Q x / N) + progress bar; a results screen showing
which terms were solid vs shaky (from flashcard self-ratings, typed accuracy,
discrimination misses); and a "review only what I missed" mode. Persist state
in JS memory across navigation.

Style: theme it to the subject; characterful monospace for term labels + clean
sans for definitions; clear knew-it / didn't-know colors; visible keyboard
focus; respect prefers-reduced-motion.

## Step 5 — Make It Trivial to Open
- macOS:  open docs/learning/vocab/NNNN-name.html
- Linux:  xdg-open docs/learning/vocab/NNNN-name.html

## Step 6 — Record Progress
Update docs/learning/PROGRESS.md: which terms were drilled (link the file),
which are solid vs shaky. A term graduates to confident only after it holds up
across more than one vocab drill. Feeds the same source of truth /teach and
/drill-me use.

## Step 7 — Quick Check
End with 2-3 in-chat questions on the shakiest terms — including one
discrimination question on a confusable pair. Let the user attempt; correct
gently with reference to the glossary entries.

## Tone
Clear and direct. The point is durable fluency in the real vocabulary of the
field and this codebase — producing the term and the nuance on demand, the way
you'd need to in an interview or a design discussion.
