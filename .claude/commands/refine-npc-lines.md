---
description: Daily NPC line-bank refinement — grow the banks from real play, prove quality and speed with the five gates, open a draft PR against dev.
---

You are running the daily NPC line refinement routine. The goal is that
conversations with residents get BETTER (more in character, fewer
paraphrase misses) and FASTER (more cache hits, fewer live model calls)
than they were yesterday — and that you can prove both with numbers.

You may read, write, commit, push, and open DRAFT PRs against `dev`.
You may NOT merge, and you may NOT touch `main`.

Format contract: `banks/README.md`. Plan: `.claude/plans/npc-line-bank.md`.

---

## STEP 0 — PRECONDITIONS (abort if any fail)

```bash
# Model must be up — gates 3 and 4 call it and fail closed without it
command -v ollama >/dev/null 2>&1 || { echo "ABORT: no ollama on PATH"; exit 1; }
ollama list | grep -q 'qwen3:8b'   || { echo "ABORT: missing qwen3:8b"; exit 1; }

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

The cmake build is also what produces `build/persona_prompt`, which
**gate 1 shells out to**. Skip this build and gate 1 cannot run, which
counts as a failure, not a skip.

On a headless box the raylib configure step needs system headers:
`libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`.
A `RandR headers not found` or `OPENGL_INCLUDE_DIR-NOTFOUND` error is a
missing package, not a repo regression.

Record the case count and assertion count. If either build is red,
stop and report; do not refine on top of a broken tree.

---

## STEP 2 — SCORECARD BEFORE

Score every existing bank. One call per file:

```bash
shopt -s nullglob
for b in banks/*.bank; do
  echo "── $b"
  python3 tools/eval_lines.py --bank "$b" --seed 1 \
      --markdown-out "/tmp/before-$(basename "$b" .bank).md"
done
```

If that loop runs zero times, there are no banks yet — that is issue
#153, not a passing scorecard. Say so plainly and skip to Step 3.

Pass `--seed` so gate 4's order shuffling reproduces between the before
and after runs. Without it you are comparing two different A/B draws and
the delta is noise.

`--gates 1,2,5` scores offline with no network. That is a debugging
aid, **not** a substitute for a full run: 3 and 4 are the two gates that
speak to quality, and a scorecard missing them is not a scorecard.

Save the five gate results verbatim. These are the before numbers:

- Gate 1 — tag compliance (shells to `build/persona_prompt --parse`, the
  one source of truth for `parseDirectives`)
- Gate 2 — stream budget, not raw latency: a banked reply must not take
  longer to stream at `line_bank_cps` than qwen3:8b takes to generate one
- Gate 3 — persona/voice fidelity, judge ≥ 4/5, none < 3
- Gate 4 — blind, order-shuffled A/B against live qwen3:8b, win-or-tie ≥ 70%
- Gate 5 — leakage: no fourth-wall breaks, spoken brackets, knowledge-boundary
  violations, or prior-meeting references in `familiarity = first` lines

A gate that cannot run counts as a FAILURE, not a skip. The job fails
closed on purpose.

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
   is gitignored, so a fresh clone or CI container has none and the
   honest output is `no new transcripts — nothing to refine`. Check the
   file exists before concluding anything about coverage.
2. **Traffic exists but every topic is already covered.** A real and
   good result — report it as such.
3. **The persona mapping missed.** `ConversationStore` keys rows by
   `Persona::name` ("Marge Holloway"), while banks and persona files are
   named by stem ("baker"). `persona_files_by_name()` in
   `refine_lines.py` maps between them; this silently skipped the whole
   cast once. If transcripts exist but every NPC prints
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

**Familiarity tiers.** A `first` line and a `returning` line should not
be interchangeable. If they are, one of them is wasted. Every other
topic must be authored memory-agnostic — gate 5 hard-fails lines that
reference prior meetings outside a `returning` topic.

**Rotation.** Same speaker, same topic, asked three times, should give
three different lines. That is the LRU doing its job.

---

## STEP 4 — SCORECARD AFTER, AND THE DIFF

Same loop as Step 2, same `--seed`, writing to `/tmp/after-*.md`:

```bash
shopt -s nullglob
for b in banks/*.bank; do
  echo "── $b"
  python3 tools/eval_lines.py --bank "$b" --seed 1 \
      --markdown-out "/tmp/after-$(basename "$b" .bank).md"
done
```

Compare against Step 2. Report as a table: gate, before, after, delta.

**Any gate that regressed blocks the PR.** Restore the banks from the
snapshot, say which gate went backwards and by how much, and stop.
A faster cache that says worse things is not an improvement.

---

## STEP 5 — SPEED, MEASURED

Report:

- Cache hit rate before vs after (how many player lines the bank served
  without touching the model)
- Stream budget headroom at `line_bank_cps = 220`

Baseline to beat, from `bench/REPORT.md`: 2.7 s to first token, 4.5 s
median total. A banked reply should be visibly faster than a live one.

Hit rate and real served latency come from the C++ side's `BankStats`,
not from `eval_lines.py` — gate 2 measures the stream budget, which is a
different question. Do not quote a gate 2 pass as a hit rate.

If hit rate did not move, say so plainly — that is the whole point of
the routine, and a run that improved nothing is a real result worth
reporting.

---

## STEP 6 — REGRESSION GUARD

```bash
make -C tests test
cmake --build build -j8
```

Both green, or restore and report.

---

## STEP 7 — DRAFT PR

Stage explicit paths only. NEVER `git add .` or `git add -A`.

```bash
git checkout -b refine/lines-$(date +%Y-%m-%d)
git add banks/
git commit -m "content(linebank): daily refinement $(date +%Y-%m-%d)"
git push -u origin refine/lines-$(date +%Y-%m-%d)
```

Open a DRAFT PR with base `dev` via PyGithub (no `gh` CLI in this repo).

The PR body must contain:
1. The before/after gate table
2. Cache hit rate delta
3. Every line added or changed, grouped by persona, with one sentence
   on why each was worth banking
4. Anything you rejected and why — this is the most useful section for
   review, because it shows the judgement, not just the output

Commit trailer: `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`

NEVER commit: `docs/learning/`, `OVERNIGHT_REPORT.md`, `.claude/memory/`,
`docs/qa/**`. Those are local-only.

---

## FAILURE POLICY

- A gate regression is a STOP, not a warning.
- Two failed attempts at the same sticking point → restore, report, exit.
- Never open a PR on an unverified tree.
- Never lower a gate to make content pass. If gate 4 shows banked lines
  losing to the live model, the content is wrong — fix the content.
- If there is nothing worth banking, say "nothing recurred often enough
  to bank today" and open no PR. Distinguish that from "every topic is
  already covered" and from "there were no transcripts at all" — they
  are three different results (see Step 3).
