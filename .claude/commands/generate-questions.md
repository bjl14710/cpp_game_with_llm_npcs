---
description: Generate interview-style questions on a topic as an interactive HTML quiz. Reveals the relevant example from your actual code after each answer (or generates one if absent).
argument-hint: The topic to be interviewed on (e.g. "BSP", "thread safety", "SCPI parsing")
---

The user wants to be interviewed on "$1" — drilled with interview-style
questions to prepare for a real technical interview or to harden their
understanding. Produce an interactive HTML quiz, grounded in this project's
code wherever possible.

If "$1" is blank, ask what topic they want to be interviewed on before
proceeding.

## Step 1 — Ground in the Codebase
Before writing questions, grep/read the repo to find where "$1" actually
appears — the files, classes, functions, or patterns that use this concept.
You'll attach these real snippets to the relevant questions later. Note
which aspects of the topic ARE present in the code and which are not (you'll
generate examples for the absent ones).

## Step 2 — Verify Technical Accuracy
Interview questions must be correct. For anything version- or spec-sensitive
(library behavior, protocol details, language semantics), web-search to
confirm rather than relying on memory. A plausible-but-wrong "expected
answer" would actively mislead the user before an interview. Note sources.

## Step 3 — Generate 10 Questions
Create a mix that mirrors a real interview, escalating in difficulty:
- A few warm-up recall questions (definitions, basic usage)
- Several core conceptual questions (how/why it works, tradeoffs)
- A couple of "compare and contrast" questions (X vs Y distinctions)
- One or two applied/scenario questions ("what happens if...", "how would
  you...", "debug this")

Use a mix of formats:
- Multiple choice (for recall and crisp conceptual points)
- Short-answer / open-ended (for "explain..." and "compare..." — these get
  a model answer revealed for self-assessment, since JS can't grade prose)
- Scenario (present a situation, ask what they'd do; reveal a strong answer)

## Step 4 — Attach Code Examples
For EACH question, prepare a "from the code" reveal that appears after the
user answers:
- If the concept appears in THIS repo: show the real snippet with its file
  path (e.g. `src/scpi/parser.py:88`), and a sentence connecting it to the
  question.
- If it does NOT appear in the repo: generate a clear, minimal example that
  illustrates the concept, and label it clearly as a generated example
  ("Not in your codebase yet — here's how it typically looks:").

This is the core value: the user sees how the abstract question maps to real,
inspectable code they own.

## Step 5 — Produce the HTML
Self-contained single file at
`docs/learning/interviews/NNNN-<dash-case-topic>.html`
(increment NNNN; start 0001). Inline CSS/JS, opens via file://, clean and
readable with syntax-highlighted code.

Structure:
1. Title: "Interview: [topic]" + one line on why it matters for this project
2. The 10 questions, presented one at a time or as a scrollable set
3. For multiple-choice: immediate right/wrong feedback on click
4. For open-ended/scenario: a "Show model answer" button revealing a strong
   answer for self-comparison, plus a self-rating (got it / partial / missed)
5. After answering each question: a "Show example from code" reveal with the
   snippet prepared in Step 4
6. A running score for the gradable questions + a self-assessment tally for
   the open-ended ones
7. A short "areas to review" summary at the end based on the topic's subparts

CRITICAL — randomize answer position (multiple choice). Never encode
correctness by position. Mark the correct option with data-correct="true",
wrong ones "false". Shuffle options in the DOM on load (Fisher-Yates). The
click handler checks the data-correct attribute, NOT position. Each option
carries a data-why explanation shown on click.

CRITICAL — no length or detail tell. Do not make the correct answer the
longest or most-detailed option. All options for a question must be roughly
the same length and the same level of specificity and hedging — if the
correct answer carries a caveat or precise qualifier, give the distractors
comparable ones. Distractors must be plausible (wrong because the CONTENT is
wrong, not because they're thin or vague — what someone who half-understands
would pick). Length and polish must carry zero signal about correctness.

Reference pattern:

  <div class="question" data-question>
    <p>Question text</p>
    <ul class="options">
      <li data-correct="true"  data-why="Why correct.">Option</li>
      <li data-correct="false" data-why="Why wrong.">Option</li>
    </ul>
    <button class="reveal-code" hidden>Show example from code</button>
    <pre class="code-example" hidden></pre>
  </div>
  <script>
  function shuffle(a){for(let i=a.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[a[i],a[j]]=[a[j],a[i]];}return a;}
  document.querySelectorAll('[data-question]').forEach(q=>{
    const list=q.querySelector('.options');
    if(list){shuffle([...list.children]).forEach(li=>list.appendChild(li));
      list.querySelectorAll('li').forEach(li=>li.addEventListener('click',()=>{
        const ok=li.dataset.correct==='true';
        li.style.color=ok?'green':'crimson';
        const btn=q.querySelector('.reveal-code'); if(btn) btn.hidden=false;
      }));}
  });
  </script>

Hold score/state in plain JS variables and DOM, NOT localStorage (unreliable
on file://).

## Step 6 — Make It Easy to Open
Print the open command for their platform:
- macOS:  open docs/learning/interviews/NNNN-name.html
- Linux:  xdg-open docs/learning/interviews/NNNN-name.html

## Step 7 — Offer a Debrief
After they've worked through it, offer to go deeper on any question they
missed — and note that `/teach $1` can build a full lesson on the topic if
they want to shore up a weak area. If a clear weakness emerges, suggest
recording it in docs/learning/PROGRESS.md so future study targets it.

## Tone
This is interview prep — be rigorous. Questions should be the kind a sharp
interviewer actually asks, not softballs. But the goal is building real
confidence through their own code, so the code reveals should make each
concept concrete and memorable.
