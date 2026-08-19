---
description: Drill me on a concept from this project with an interactive HTML self-test — multiple-choice plus written recall, grounded in the actual code, tracking what's solid vs shaky across sessions.
argument-hint: What concept do you want to drill? (leave blank to drill your weakest area)
model: fable
---

The user wants to drill "$1" (or, if blank, you choose the concept most worth
drilling right now — prioritize what they've recently learned or flagged as shaky
in PROGRESS.md, since drilling is for retention, not first exposure).

This is the TESTING half of a stateful, multi-session learning loop. Its sibling
`/teach` builds understanding; `/drill-me` verifies and hardens it through active
recall. They share the workspace `docs/learning/`. Read what already exists there
before building anything — a drill is only useful when aimed at what the user has
actually learned and where they're actually weak.

## How this differs from /teach

`/teach` produces lessons: first exposure, explanation, light practice.
`/drill-me` produces **drills**: recall-heavy self-tests that surface what's
retained vs. forgotten, then feed that signal back into PROGRESS.md so the next
`/teach` session can target the gaps. Where teach explains, drill interrogates.

## Workspace files (read these first if they exist)

- `docs/learning/PROGRESS.md` — what's been taught, what was hard, what's next.
  Use it to choose the target concept and its difficulty, and to know which
  sub-areas to hammer. You will update it after the drill (Step 5).
- `docs/learning/GLOSSARY.md` — terms the user understands. Keep the drill's
  wording consistent with it.
- `docs/learning/lessons/*.html` — existing lessons from /teach. If a lesson
  exists for this concept, your drill should test exactly what that lesson taught.
- `docs/learning/drills/*.html` — past drills, numbered `NNNN-<dash-case-name>.html`,
  incrementing. Increment from the highest existing number; start at 0001.

## Step 1 — Scope and ground the drill

**Target.** If "$1" is given, drill it at the stated granularity — broad
("concurrency") or narrow ("the `volatile` keyword"). If blank, pick from
PROGRESS.md, favoring recently taught concepts and anything marked shaky.

**Channels.** Break the target into 3–6 sub-aspects that become selectable
categories. For `volatile`, for example: "what it does", "when to use it",
"gotchas & misconceptions", "reading real code".

**Ground in THIS project.** Grep/read for real usages of the concept in the
codebase and build several questions from the ACTUAL code — show a real snippet,
ask what it does, why it's written that way, or what breaks if you change it, and
cite `path/to/file:line`. Real-code questions are the most valuable; lead with
them. If the concept isn't in the codebase, use strong canonical examples and say so.

## Step 2 — Gather trustworthy knowledge

Don't rely solely on parametric knowledge for technical specifics — it can be
outdated or subtly wrong. For anything accuracy-sensitive (library behavior,
protocol specs, version-specific details, standards), web-search to ground it in
current authoritative sources, and note the sources. A wrong answer in a drill
actively teaches the wrong thing, so correctness matters even more here than in a lesson.

## Step 3 — Produce the drill HTML (one self-contained file)

Save to `docs/learning/drills/NNNN-<dash-case-name>.html`. Inline CSS + JS only,
no external dependencies, opens correctly with a plain `file://`. **No localStorage
or sessionStorage** — hold all state in JS memory. Weight the bank toward **written
recall** (~60–70%); the rest multiple-choice. 15–25 questions total.

Required features:
- **Start screen:** channel chips (plus an "all" toggle), a shuffle toggle, a begin button.
- **One question at a time:** progress (Q x / N), a progress bar, the category tag, a type label.
- **Multiple-choice:** instant feedback, answers hidden until committed. Obey the two
  integrity rules below — they are not optional.
- **Written recall:** a `<textarea>`; a "check my answer" button that scans the typed
  text against the question's key concepts and shows which were HIT vs missed with a
  tally (e.g. "4 / 5 key concepts detected"); and a "show me the answer" button that
  reveals the model answer, then offers self-rating ("knew it" / "needs review").
- **Code snippets** in a monospace `<pre>` block.
- **Navigation:** prev / next; a results screen (MC score + self-rated written tally);
  and a "review only what I missed" mode (re-runs MC misses + "needs review" items).
- Persist typed answers in memory across prev/next so navigation doesn't lose them.
- A visible note that the keyword check is rough — self-judge against the model answer.

### CRITICAL — multiple-choice integrity (carried over from /teach)

A drill that can be gamed teaches nothing. Enforce both rules structurally.

**Position must carry zero signal.** Never encode correctness by position.
- Mark the correct option with `data-correct="true"`, wrong ones `data-correct="false"`.
- SHUFFLE each question's options in the DOM on load (Fisher–Yates), so the rendered
  order is randomized every open. The click handler checks `dataset.correct`, never index.
- Vary the authored order yourself too, as a backstop.
- Each option carries its own short `data-why` explanation, shown on click — so distractors teach.

**Length and polish must carry zero signal.** Don't let the correct answer be the
longest, most-hedged, or most-detailed option.
- All options for a question: roughly equal length, same level of specificity and hedging.
  If the right answer has a caveat, give the distractors comparable caveats.
- Distractors are wrong because the CONTENT is wrong — plausible choices someone who
  half-understands would pick — never because they're thin, vague, or obviously short.

### Reference pattern — shuffled, position-independent MC

```html
<div class="question" data-question>
  <p>Which keyword forces the compiler to re-read a variable from memory on each access?</p>
  <ul class="options">
    <li data-correct="true"  data-why="volatile stops the compiler caching the value in a register.">volatile</li>
    <li data-correct="false" data-why="const means read-only; it doesn't affect caching of reads.">const</li>
    <li data-correct="false" data-why="static controls linkage and lifetime, not memory re-reads.">static</li>
  </ul>
  <p class="feedback" hidden></p>
</div>
<script>
function shuffle(a){for(let i=a.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[a[i],a[j]]=[a[j],a[i]];}return a;}
document.querySelectorAll('[data-question]').forEach(q=>{
  const list=q.querySelector('.options');
  shuffle([...list.children]).forEach(li=>list.appendChild(li));   // reorder DOM on load
  const fb=q.querySelector('.feedback');
  list.querySelectorAll('li').forEach(li=>li.addEventListener('click',()=>{
    const correct=li.dataset.correct==='true';                     // checks attribute, not position
    fb.hidden=false; fb.textContent=(correct?'✓ Correct. ':'✗ Not quite. ')+li.dataset.why;
    fb.style.color=correct?'green':'crimson';
  }));
});
</script>
```
The load-bearing line is `li.dataset.correct === 'true'` — correctness comes from the
attribute, and `shuffle` reorders the DOM on load, so position is meaningless.

### Reference pattern — written-recall keyword check

Each written question carries key concepts; a concept is "hit" if any of its terms
appears (case-insensitive substring) in the typed answer. Show hits/misses and a
tally, then reveal the model answer for honest self-judgment. Choose robust stems
(e.g. `"reallocat"` to match reallocate/reallocation).

```js
const text = (typed || "").toLowerCase();
const hits = concepts.map(c => ({ c, hit: c.terms.some(t => text.includes(t.toLowerCase())) }));
const got  = hits.filter(h => h.hit).length;   // show got / concepts.length, list HIT vs missed
```

### Style

Theme the drill to fit the subject's own world rather than a generic default. Pair a
characterful monospace (labels, tags, headings) with a clean sans (body); use one
signature visual element tied to the subject; clear, distinct pass / fail / hit
colors; visible keyboard focus; respect `prefers-reduced-motion`. Beautiful and
readable, not a wall of text.

## Step 4 — Make it trivial to open

After writing the file, print the exact command for the user's platform:
- macOS:  `open docs/learning/drills/NNNN-name.html`
- Linux:  `xdg-open docs/learning/drills/NNNN-name.html`

## Step 5 — Record results (close the loop with /teach)

Update `docs/learning/PROGRESS.md`:
- What was drilled this session (link the drill file) and the date.
- The MC score and which concepts the user self-rated solid vs. needs-review.
- Promote a concept toward "solid" only after it holds up across more than one drill.
- A suggested next move: re-`/teach` the shaky concepts, or drill the next concept up.

This is what lets the next `/teach` session target real gaps instead of guessing —
the two commands share one source of truth about what the user actually knows.

## Step 6 — Knowledge check before closing

End the session with 2–3 spoken (in-chat) recall questions on the shakiest areas
from this drill. Let the user attempt them; correct gently with reference back to the
model answers. This is the retention check a static file alone misses.

## Tone

Encouraging but honest. Drilling exists to find gaps — frame a miss as a gap found,
not a failure, and never pad. Challenge the user appropriately; they're hardening
skills they'll rely on for real. The HTML drill is the durable artifact; the chat is
the live coaching around it.
