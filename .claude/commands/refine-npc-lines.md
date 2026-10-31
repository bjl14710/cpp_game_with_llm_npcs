---
description: Daily NPC line-bank refinement — grow the banks from real play, measure quality and matching, open a draft PR against dev with the day's scorecard.
---

You are running the daily NPC line refinement routine. The goal is that
conversations with residents get BETTER (more in character, fewer
paraphrase misses) and FASTER (more cache hits, fewer live model calls)
than they were yesterday — and that you can prove both with numbers.

You may read, write, commit, push, and open DRAFT PRs against `dev`.
You may NOT merge, and you may NOT touch `main`.

Format contract: `banks/README.md`. Plan: `.claude/plans/npc-line-bank.md`.

**Open a draft PR on every run, even a no-op one.** The daily PR is the
review artifact; a run that changed nothing still reports its scorecard
in the PR body and says plainly that nothing changed. Do not skip the PR
because there was nothing to bank — say so in it instead.

---

## STEP 0 — PRECONDITIONS

```bash
# Clean tree, current dev
git status --short
git checkout dev && git pull

# Read the actual flag surface before assuming it
python3 tools/refine_lines.py --help
python3 tools/eval_lines.py --help
```

Do not guess at script flags. Read `--help` first, every run.

Two that matter and have been guessed wrong before:

- **`eval_lines.py` requires `--bank <file>` and scores ONE bank per
  invocation.** There is no "score everything" mode. A bare
  `python3 tools/eval_lines.py` exits 2 on argparse, which is easy to
  misread as "the gates passed."
- `refine_lines.py --dry-run` runs steps 1–5 and writes nothing. Use it
  whenever you want to see the candidate set without touching `banks/`.

**This routine runs the OFFLINE gates by default.** Gates 1, 2 and 5 need
no model and no key. Gates 3 and 4 need a judge, and a scheduled run
starts in a fresh container with no ollama and no key, so they will not
run. That is expected — report them as UNRUN, never as passed. See
STEP 4 for what that means for the verdict.

---

## STEP 1 — BASELINE, BOTH BUILDS

```bash
make -C tests test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
```

BOTH are required. `tests/Makefile` globs `../src/core/*.cpp` but the
root `CMakeLists.txt` lists sources explicitly — a green test suite does
NOT prove the cmake build works. This has broken before (see
`fix(build): drop the CMakeLists entry for a scaffold file that is not here`).
After adding any `src/core/*.cpp`, check it is in `CMakeLists.txt`.

The cmake build also produces the two binaries the measurements shell
out to: `build/persona_prompt` (gate 1) and `build/bank_probe` (STEP 5).
Skip this build and neither can run, which counts as a failure, not a
skip.

On a headless box the raylib configure step needs system headers:
`libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
A `RandR headers not found` or `OPENGL_INCLUDE_DIR-NOTFOUND` error is a
missing package, not a repo regression.

Also run the Python tool tests — stdlib only, no dependencies:

```bash
python3 -m unittest discover -s tools -p 'test_*.py'
```

Record the case count and assertion count. If any of the three is red,
stop and report; do not refine on top of a broken tree.

---

## STEP 2 — SCORECARD BEFORE

Score every existing bank. One call per file:

```bash
shopt -s nullglob
for b in banks/*.bank; do
  echo "── $b"
  python3 tools/eval_lines.py --bank "$b" --gates 1,2,5 --seed 1 \
      --markdown-out "/tmp/before-$(basename "$b" .bank).md"
done
```

If that loop runs zero times, there are no banks at all — that is a
broken checkout, not a passing scorecard. Say so and stop.

Pass `--seed` so any judged run reproduces between the before and after
runs. Without it you are comparing two different A/B draws and the delta
is noise.

The five gates:

- Gate 1 — tag compliance (shells to `build/persona_prompt --parse`, the
  one source of truth for `parseDirectives`)
- Gate 2 — stream budget, not raw latency: a banked reply must not take
  longer to stream at `line_bank_cps` than qwen3:8b takes to generate one
- Gate 3 — persona/voice fidelity, judge ≥ 4/5, none < 3 — **needs a judge**
- Gate 4 — blind, order-shuffled A/B against live qwen3:8b, win-or-tie
  ≥ 70% — **needs a judge AND a local model**
- Gate 5 — leakage: no fourth-wall breaks, spoken brackets, or
  prior-meeting references in `familiarity = first` lines

Gates 3-5 route through `config/llm.cfg`: `base_url` picks the endpoint
(default OpenRouter, keyed by `api_key_env`), `judge_model` picks the
model there. Pointing `base_url` at `http://localhost:11434/v1` runs them
against Ollama with no key. Do that only as a smoke test — a local judge
is weaker and grades lines written by a sibling of itself.
`eval_lines.py` stamps `judge:` on every scorecard and prints a warning
banner when the judge was local, so a local run can never be mistaken for
real gate 3/4 evidence. It is also slow: on CPU-only hardware budget
minutes per line, and gate 4 may exceed the request timeout and fail
closed.

---

## STEP 3 — REFINE

```bash
python3 tools/refine_lines.py --dry-run   # look first
python3 tools/refine_lines.py             # then commit to it
```

The job snapshots `banks/` before writing and restores on any failure.
Trust that, but verify: after it runs, `git diff --stat banks/` should
show only `.bank` files.

**If it reports zero candidates, diagnose before believing it.** The
three causes are different results and the script has confused them
before:

1. **No transcripts at all.** `refine_lines.py` reads
   `saves/conversations.sqlite3` and `saves/ratings/*.jsonl`. `saves/`
   is gitignored, so a fresh clone or a scheduled container has none and
   the honest output is `no new transcripts — nothing to refine`. Check
   the file exists before concluding anything about coverage. **This is
   the expected result until the game has been played against this
   checkout** — report it plainly, it is not a failure.
2. **Traffic exists but every topic is already covered.** A real and
   good result — report it as such.
3. **The persona mapping missed.** `ConversationStore` keys rows by
   `Persona::name`, while banks and persona files are named by stem.
   `persona_files_by_name()` maps between them; this silently skipped
   the whole cast once. If transcripts exist but every NPC prints
   `skip <name>: not in personas/`, that mapping is what to suspect —
   not an absence of traffic.

While reading its output, judge these specifically:

**Quality.** Does each new line sound like THAT resident, not like a
helpful assistant? Reject anything that breaks the fourth wall, hedges,
or over-explains. A banked line ships verbatim forever — it gets more
scrutiny than a live generation, not less.

**Paraphrase coverage.** When a topic was missed, the fix is almost
always MORE `trigger =` phrasings on an existing topic, NOT a new topic.
`TopicMatcher` is deliberately lexical; paraphrase is solved at
authoring time. Target 8–12 phrasings per topic, 3–5 replies.

Author the CONTRACTED forms too. `normalizeLine` turns an apostrophe
into a separator, so `"I'm lost"` normalizes to `"m lost"` while a
trigger written `"im lost"` normalizes to `"im lost"` — different
strings, under threshold, topic never fires. Players type contractions.

**Familiarity tiers.** A `first` line and a `returning` line should not
be interchangeable. If they are, one of them is wasted. Every other
topic must be authored memory-agnostic.

**Rotation.** Same speaker, same topic, asked three times, should give
three different lines. That is the LRU doing its job.

---

## STEP 4 — SCORECARD AFTER, AND THE DIFF

Same loop as STEP 2, same gates, same `--seed`, writing to `/tmp/after-*.md`.

Compare against STEP 2. Report as a table: gate, before, after, delta.

**Any gate that regressed blocks the merge.** Restore the banks from the
snapshot, say which gate went backwards and by how much, and open the PR
reporting the regression rather than the content. A faster cache that
says worse things is not an improvement.

Gates 3 and 4 are UNRUN on a scheduled run. Say `UNRUN` in the table —
not `pass`, not `—`, not a blank cell. A blank reads as clean. The PR
body must state that this scorecard is not sufficient to flip
`line_bank = on`, which is what issue #153 gates on.

---

## STEP 5 — MATCHING AND SPEED, MEASURED

`eval_lines.py` scores the REPLIES. Nothing there scores the TRIGGERS,
and a bank can pass all five gates and still never fire. `bank_probe`
is the other half:

```bash
./build/bank_probe banks/ --probes bench/probes.txt
./build/bank_probe banks/ --probes bench/probes.txt --json   # for the PR body
```

Exit codes: 0 every probe hit, 1 one or more missed, 2 a bank failed to
load or none loaded at all.

Report:

- Probe hit rate, before vs after, and every miss by name
- Rotation: three identical asks must yield three distinct replies
- Familiarity: first and returning must differ
- Stream budget headroom at `line_bank_cps = 220`

**Every miss goes into `bench/probes.txt` before the trigger that fixes
it**, so the fix stays measured. Never add a probe by copying a trigger
phrasing — that scores 1.0 and measures nothing.

Baseline to beat, from `bench/REPORT.md`: 2.7 s to first token, 4.5 s
median total.

The probe hit rate is NOT the cache hit rate issue #153 asks for. That
one is measured over replayed real transcripts by the C++ side's
`BankStats`. Do not quote one as the other. If there are no transcripts,
say the cache hit rate is unmeasured.

---

## STEP 6 — REGRESSION GUARD

```bash
make -C tests test
cmake --build build -j8
python3 -m unittest discover -s tools -p 'test_*.py'
```

All three green, or restore and report.

---

## STEP 7 — DRAFT PR, EVERY RUN

Stage explicit paths only. NEVER `git add .` or `git add -A`.

```bash
git checkout -b refine/lines-$(date +%Y-%m-%d)
git add banks/ bench/probes.txt
git commit -m "content(linebank): daily refinement $(date +%Y-%m-%d)"
git push -u origin refine/lines-$(date +%Y-%m-%d)
```

When nothing changed there is nothing to commit — push the branch with
an empty commit (`git commit --allow-empty`) so the PR still exists and
carries the day's numbers.

Open a DRAFT PR with base `dev` via PyGithub (no `gh` CLI in this repo).

The PR body must contain:
1. The before/after gate table, with gates 3 and 4 marked UNRUN
2. Probe hit rate before/after, and every miss by name
3. Whether the cache hit rate was measurable, and if not, why
4. Every line added or changed, grouped by persona, with one sentence
   on why each was worth banking
5. Anything you rejected and why — this is the most useful section for
   review, because it shows the judgement, not just the output
6. If nothing changed: say so in one line at the top, and keep the
   numbers below it

Title the PR `content(linebank): daily refinement YYYY-MM-DD`.

Commit trailer: `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`

NEVER commit: `docs/learning/`, `OVERNIGHT_REPORT.md`, `.claude/memory/`,
`docs/qa/**`, `config/secrets.cfg`, or a `base_url` override in
`config/llm.cfg`. Those are local-only; a committed local judge endpoint
silently weakens gates 3-5 for everyone.

---

## FAILURE POLICY

- A gate regression is a STOP on merging, not on reporting. Open the PR
  and say what regressed.
- Two failed attempts at the same sticking point → restore, report, exit.
- Never claim a gate ran when it did not. UNRUN is a result.
- Never lower a gate to make content pass. If gate 4 shows banked lines
  losing to the live model, the content is wrong — fix the content.
- Distinguish "nothing recurred often enough to bank today" from "every
  topic is already covered" from "there were no transcripts at all" —
  they are three different results (see STEP 3).
