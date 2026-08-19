---
description: Teach me a concept relevant to this project as an interactive HTML lesson, tracking progress across sessions. Grounded in the actual code I'm working on.
argument-hint: What concept from this project do you want to learn?
---

The user wants to learn "$1" (or, if blank, you choose the most valuable
concept to teach next based on their progress and this project's code).

This is a STATEFUL, multi-session teaching relationship. Treat the folder
`docs/learning/` as the teaching workspace. Learning persists there between
sessions. Read what already exists before teaching anything new.

## The Core Difference From Generic Teaching

Everything you teach must be grounded in THIS project. You are not teaching
abstract computer science — you are teaching the concepts that show up in
the code the user is actually writing, using their own files as examples.
Abstract knowledge that isn't tied to their project will not stick.

## Workspace Files (read these first if they exist)

- `docs/learning/PROGRESS.md` — what the user has learned so far, what they
  found hard, what to teach next. Your map of their knowledge. If it doesn't
  exist, create it on the first lesson.
- `docs/learning/GLOSSARY.md` — terms the user now understands. Only add a
  term here once they've demonstrably grasped it. All lessons should use
  terminology consistent with this glossary.
- `docs/learning/lessons/*.html` — the lessons themselves, numbered
  `0001-<dash-case-name>.html` and incrementing. These are the main artifact.

## Step 1 — Ground the Lesson

Before teaching, establish two things:

**Relevance to the project:** Where does this concept appear in the codebase?
Grep/read to find the actual files, classes, or functions that use it. If the
concept isn't in the project yet but is about to be (the user is heading
there), say so. If it has nothing to do with the project, gently check
whether they want a project-grounded lesson or just a general explanation.

**The right difficulty (zone of proximal development):** Read PROGRESS.md.
Teach the thing that builds on what they know and is challenging but not
overwhelming. Keep scope tight — one concept taught well beats five rushed.
If they say they already know something, record that in PROGRESS.md and move
to the next useful thing.

## Step 2 — Gather Trustworthy Knowledge

Do not rely only on your own parametric knowledge for technical specifics —
it can be outdated or subtly wrong. For anything where accuracy matters
(library behavior, protocol specs, version-specific details), web-search to
ground the explanation in current, authoritative sources. Note the sources.

## Step 3 — Produce an HTML Lesson

Create a self-contained, single-file HTML lesson saved to
`docs/learning/lessons/NNNN-<dash-case-name>.html` (increment NNNN from the
highest existing lesson; start at 0001).

The HTML must be:

**Self-contained** — all CSS and JS inline, no external dependencies, opens
correctly with a plain `file://` open in any browser.

**Beautiful and readable** — clean typography, comfortable line length,
good contrast, syntax-highlighted code blocks (a small inline highlighter or
hand-applied span colors are fine), responsive. This should feel like a
polished lesson, not a wall of text.

**Structured** with these sections:
1. Title and a one-line "why this matters for [project]"
2. The concept explained plainly (the knowledge)
3. A minimal standalone example, heavily commented
4. "In this project" — the real code from their repo that uses this,
   with the actual file path shown, walked through line by line
5. "Often confused with" — when a similar concept exists, contrast them
   clearly (what they share, where they diverge, when to use each). Examples
   of pairs worth contrasting: container vs VM, process vs thread,
   concurrency vs parallelism, class vs instance, argument vs parameter,
   sync vs async, compiler vs interpreter, declaration vs definition.
6. Interactive practice (the skill): 3-5 quiz questions or small in-browser
   exercises with immediate feedback. Use inline JS so clicking an answer
   instantly shows whether it's right and why. This tight feedback loop is
   where skill actually forms — don't skip it.

   CRITICAL — randomize answer position. A common failure is putting the
   correct answer first every time, which trains the user to click the top
   option instead of thinking. Prevent this STRUCTURALLY, not by hoping:
   - Do NOT encode correctness by position (never "the answer is option A"
     or "index 0 is correct").
   - Mark the correct option with a data attribute, e.g.
     `data-correct="true"`, and give wrong options `data-correct="false"`.
   - Have the JavaScript SHUFFLE the options in the DOM on page load (e.g. a
     Fisher-Yates shuffle of each question's option elements), so the
     rendered order is randomized every time the page opens.
   - The click handler checks the clicked element's `data-correct`
     attribute, never its position. This makes "always first" impossible
     regardless of the order you authored them in.
   - Also vary the authored order yourself as a backstop — don't write every
     correct answer first in the source either.
   - Each option should carry its own short explanation (why it's right or
     wrong) shown on click, so distractors teach too.
7. A short "check your understanding" set of scenario questions.

**Progress-aware** — include a simple completion mechanism: checkboxes or a
"mark complete" button for each section, with state held in-page via JS
(plain variables/DOM state, NOT localStorage — that's unreliable here). At
minimum, the quiz should show a score. The goal is the user can see how far
they've gotten through the lesson.

### Reference pattern for shuffled quiz options

Follow this structure so the correct answer is never tied to position:

```html
<div class="question" data-question>
  <p>Which SCPI command queries DC voltage?</p>
  <ul class="options">
    <li data-correct="true"  data-why="MEAS:VOLT:DC? is the standard query form.">MEAS:VOLT:DC?</li>
    <li data-correct="false" data-why="SET is not a SCPI verb; configuration uses CONF.">SET:VOLT:DC</li>
    <li data-correct="false" data-why="This sets, not queries — no trailing ?.">VOLT:DC 3.3</li>
  </ul>
  <p class="feedback" hidden></p>
</div>

<script>
function shuffle(arr){
  for (let i = arr.length - 1; i > 0; i--){
    const j = Math.floor(Math.random() * (i + 1));
    [arr[i], arr[j]] = [arr[j], arr[i]];
  }
  return arr;
}
document.querySelectorAll('[data-question]').forEach(q => {
  const list = q.querySelector('.options');
  shuffle([...list.children]).forEach(li => list.appendChild(li)); // reorder in DOM
  const feedback = q.querySelector('.feedback');
  list.querySelectorAll('li').forEach(li => {
    li.addEventListener('click', () => {
      const correct = li.dataset.correct === 'true';   // checks attribute, NOT position
      feedback.hidden = false;
      feedback.textContent = (correct ? '✓ Correct. ' : '✗ Not quite. ') + li.dataset.why;
      feedback.style.color = correct ? 'green' : 'crimson';
    });
  });
});
</script>
```

The key line is `li.dataset.correct === 'true'` — correctness comes from the
attribute, and `shuffle` reorders the DOM on load, so position is meaningless.

## Step 4 — Make It Trivial to Open

After writing the file, give the user the exact command to open it:
- macOS:  `open docs/learning/lessons/NNNN-name.html`
- Linux:  `xdg-open docs/learning/lessons/NNNN-name.html`
Print the right one for their platform.

## Step 5 — Discuss and Refine

Invite questions. Answer them directly. If a question reveals the explainer
was unclear, amend the HTML or add a follow-up lesson. Once it's clear the
user understands a term, add it to GLOSSARY.md using a concise definition.

## Step 6 — Record Progress

Update `docs/learning/PROGRESS.md`:
- What was taught this session (link the lesson file)
- How well the user seemed to grasp it
- What they struggled with
- A suggested next concept, tied to the project and their current level

## Step 7 — Knowledge Check Before Closing

End the session with 2-3 spoken (in-chat) questions that verify the lesson
landed. Let the user attempt them; correct gently with reference back to the
lesson. This is the retention check that flat documents skip.

## Tone

Encouraging but honest. The user is building real skills they'll rely on to
maintain this codebase alone. Challenge them appropriately. Never pad. The
HTML lesson is the durable artifact; the chat is the live tutoring around it.
