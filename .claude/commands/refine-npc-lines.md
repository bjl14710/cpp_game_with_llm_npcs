---
description: Measure and refine the NPC line banks in-chat — gates, matching, rotation — and report the numbers. Never branches, commits, pushes, or opens a PR.
---

Run the NPC line-bank refinement and report the numbers here, in chat.

**This command touches git NOT AT ALL.** No branch, no commit, no push, no
PR. If refinement produces new lines they are left in the working tree for
the human to read with `git diff banks/` and decide about. Ending a run by
saying "and I've pushed it" is a bug, not a convenience.

The goal is unchanged: conversations with residents get BETTER (more in
character, fewer paraphrase misses) and FASTER (more cache hits, fewer live
model calls) — and you can prove both with numbers rather than adjectives.

Format contract: `banks/README.md`. Plan: `.claude/plans/npc-line-bank.md`.

---

## STEP 0 — READ THE TOOL SURFACE

```bash
python3 tools/refine_lines.py --help
python3 tools/eval_lines.py --help
```

Do not guess at flags. Read `--help` first, every run.

Two that have been guessed wrong before:

- **`eval_lines.py` requires `--bank <file>` and scores ONE bank per
  invocation.** There is no "score everything" mode. A bare
  `python3 tools/eval_lines.py` exits 2 on argparse, which reads
  deceptively like a pass.
- `refine_lines.py --dry-run` runs steps 1–5 and writes nothing.

---

## STEP 1 — BUILD AND TEST

```bash
make -C tests test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
python3 -m unittest discover -s tools -p 'test_*.py'
```

All three, every run. `tests/Makefile` globs `../src/core/*.cpp` while the
root `CMakeLists.txt` lists sources explicitly — a green test suite does NOT
prove the cmake build works, and that has broken before. After adding any
`src/core/*.cpp`, check it is listed in `CMakeLists.txt`.

The cmake build produces the two binaries the measurements shell out to:
`build/persona_prompt` (gate 1) and `build/bank_probe` (STEP 4). Skip it and
neither can run, which is a failure, not a skip.

On a bare box the raylib configure step needs system headers:

```bash
apt-get update && apt-get install -y libxrandr-dev libxinerama-dev \
    libxcursor-dev libxi-dev libgl1-mesa-dev
```

`RandR headers not found` or `OPENGL_INCLUDE_DIR-NOTFOUND` is a missing
package, not a repo regression.

---

## STEP 2 — DECIDE WHICH GATES CAN RUN, AND SAY SO

Gates 1, 2 and 5 need nothing but the repo. Gates 3 and 4 need a judge, and
gate 4 additionally needs a live local model to argue against.

Check what is actually available before promising anything:

```bash
command -v ollama >/dev/null && ollama list | grep -q 'qwen3:8b' && echo "local model: yes"
python3 -c "import os;print('cloud key:', bool(os.environ.get('OPENROUTER_API_KEY')))"
```

**If the model is missing and the human wants the judged gates, fetch it —
that is a normal part of this command, not an imposition:**

```bash
apt-get install -y zstd            # the installer needs it
curl -fsSL https://ollama.com/install.sh | sh
nohup ollama serve > /tmp/ollama.log 2>&1 &
ollama pull qwen3:8b               # ~5 GB
```

Then point the judge at it by appending to `config/llm.cfg`:

```
base_url = http://localhost:11434/v1
judge_model = qwen3:8b
```

**Revert that before you finish** (`git checkout -- config/llm.cfg`) and
NEVER commit it. A committed local endpoint silently downgrades gates 3-5
for everyone.

Know what a local judge is worth. It is weaker than the cloud model and it
grades lines written by a sibling of itself. `eval_lines.py` stamps `judge:`
on every scorecard and prints a warning banner when the judge was local,
precisely so a smoke test cannot be mistaken for the gate 3/4 evidence
issue #153 wants before flipping `line_bank = on`. Repeat that caveat in
your report; do not let the banner be the only place it appears.

Cost, measured on a 4-core CPU box at ~4.4 tokens/sec: roughly 5 minutes per
reply for gates 1,2,3,5, and gate 4 adds a full live generation on top. Tell
the human the estimate before starting a long run, and prefer scoring one
bank over all ten.

**Whatever you skip, write `UNRUN` — never `pass`, never a blank cell.** A
blank reads as clean.

---

## STEP 3 — SCORE, AND REFINE IF THERE IS ANYTHING TO REFINE

```bash
shopt -s nullglob
mkdir -p /tmp/linebank
for b in banks/*.bank; do
  s=$(basename "$b" .bank)
  echo "── $b"
  python3 tools/eval_lines.py --bank "$b" --gates 1,2,5 --seed 1 \
      --json-out "/tmp/linebank/$s.json" --markdown-out "/tmp/linebank/$s.md"
done
```

**Always pass `--markdown-out`.** Its default is `bench/SCORECARD.md`, a
single tracked path, so a loop over ten banks overwrites it nine times and
leaves a file that looks like a full scorecard but holds only the last
persona scored. `--json-out` defaults into `bench/out/`, which is gitignored,
so only the markdown side bites.

Add `3` (and `4`) to `--gates` only when STEP 2 said they can run. Keep
`--seed` fixed so any A/B ordering reproduces between runs.

If that loop runs zero times there are no banks at all — a broken checkout,
not a passing scorecard. Say so and stop.

Then look for new material:

```bash
python3 tools/refine_lines.py --dry-run
```

**If it reports zero candidates, diagnose before believing it.** Three
different causes, and the script has confused them before:

1. **No transcripts at all.** It reads `saves/conversations.sqlite3` and
   `saves/ratings/*.jsonl`. `saves/` is gitignored, so a fresh clone has
   none and `no new transcripts — nothing to refine` is the honest output.
   Expected until the game has been played against this checkout.
2. **Traffic exists, every topic already covered.** A real and good result.
3. **The persona mapping missed.** `ConversationStore` keys rows by
   `Persona::name` while banks are named by stem; `persona_files_by_name()`
   maps between them, and this silently skipped the whole cast once. If
   transcripts exist but every NPC prints `skip <name>: not in personas/`,
   suspect that — not an absence of traffic.

Only if the dry run found something worth banking, run it for real
(`python3 tools/refine_lines.py`), then **stop there** and show
`git diff --stat banks/`. The human reviews the diff. You do not commit it.

When judging new lines: does each sound like THAT resident, not a helpful
assistant? Reject fourth-wall breaks, hedging, over-explaining. A banked
line ships verbatim forever, so it earns more scrutiny than a live
generation, not less. A missed topic is almost always fixed with MORE
`trigger =` phrasings on an existing topic, not a new topic — the matcher is
deliberately lexical and paraphrase is solved at authoring time. Target
8–12 phrasings and 3–5 replies per topic.

Author the CONTRACTED forms. `normalizeLine` turns an apostrophe into a
separator, so `"I'm lost"` normalizes to `"m lost"` while a trigger written
`"im lost"` normalizes to `"im lost"` — different strings, under the 0.62
threshold, topic never fires. Seven of the first twenty-four probe lines
missed for exactly this reason while every bank passed every runnable gate.

---

## STEP 4 — MATCHING, ROTATION, FAMILIARITY

`eval_lines.py` scores REPLIES. Nothing there scores TRIGGERS, and a bank
can pass all five gates and still never fire. `bank_probe` is the other half:

```bash
./build/bank_probe banks/ --probes bench/probes.txt
./build/bank_probe banks/ --probes bench/probes.txt --json
```

Exit codes: 0 every probe hit, 1 one or more missed, 2 a bank failed to
parse or none loaded.

Report the hit rate, every miss by name, whether three identical asks give
three distinct replies, and whether first/returning greetings differ.

**Every miss goes into `bench/probes.txt` before the trigger that fixes it**,
so the fix stays measured. Never write a probe by copying a trigger phrasing
— it scores 1.0 and measures nothing.

The probe hit rate is NOT the cache hit rate issue #153 gates on. That one
comes from `BankStats` over replayed real transcripts. Do not quote one as
the other; if there are no transcripts, say the cache hit rate is unmeasured.

---

## STEP 5 — REPORT

Print, in chat:

| | |
|---|---|
| Gates 1/2/5 | lines scored, accepted, rejected |
| Gates 3/4 | the result, or `UNRUN` and why |
| Judge | which model and endpoint, and `local` if it was |
| Probe hit rate | before → after, with every miss named |
| Rotation / familiarity | pass or fail |
| Builds and tests | case and assertion counts |
| Cache hit rate | the number, or `unmeasured` and why |

Then, in prose: what changed and why it was worth banking, what you rejected
and why — the rejections are the more useful half, because they show the
judgement rather than the output — and anything left in the working tree for
the human to look at.

If nothing changed, say so in one line and give the numbers anyway. A run
that improved nothing is a real result.

Finish by leaving the tree in the state you found it apart from any bank
changes: revert `config/llm.cfg` if you touched it, and confirm with
`git status --short`.

---

## FAILURE POLICY

- Never claim a gate ran when it did not. UNRUN is a result.
- Never lower a gate to make content pass. If gate 4 shows banked lines
  losing to the live model, the content is wrong — fix the content.
- Never commit, branch, push, or open a PR from this command.
- Two failed attempts at the same sticking point → stop and report.
- Distinguish "nothing recurred often enough to bank" from "every topic is
  already covered" from "there were no transcripts at all".
