# Role Leak Baseline — 2026-08-21

**Verdict: PASS on the exploit the milestone was built to close, with one
reproducible caveat that is not a leak but is not what the design intended
either.** Methodology in `tools/role_leak_probe.py`; raw per-turn data in
`bench/out/` (gitignored — regenerate with the command below).

```
python3 tools/role_leak_probe.py --turns 8 --model qwen3:8b \
    --json bench/out/role-leak-<date>.json
```

## What was measured

Every cast member is asked one neutral question — *"Where were you on Tuesday
evening?"* — then seven escalating follow-ups, ending in a direct accusation.
Prompts come from `build/persona_prompt --state`, so each resident is asked with
the **real** role block in the **real** composition order (#304); replies are
classified by `build/persona_prompt --parse`, the real `parseDirectives`. Neither
side is re-implemented in Python, because placement is the thing under test.

Cast: 10 shipped personas — 1 killer, 4 secret-keepers, 5 bystanders. Four
secret-keepers is still the plan's guess, and this run is the first evidence
about whether the number is the right lever (see the caveat: it is not).

## Results

Two independent runs, 2026-08-21, `qwen3:8b` at `think=false`, temperature 0.8.
Run A: 5 turns × 10 residents (50 calls). Run B: 8 turns × 10 residents (80
calls). Run B is the baseline; run A is reported because agreement between them
is most of what makes these numbers worth anything at this sample size.

### 1. Action leak — FIXED

**Zero forbidden directives across 130 model calls, both runs.** The measured
failure that motivated the milestone — the killer emitting
`[[ACTION: call_police]]` on three of five turns with the role block placed
before "Stay in character" — does not reproduce at the shipped placement.

### 2. Mood leak, as a player would exploit it — PASS

A player's actual attack is: ask everyone one question, sort the faces by how
hostile they looked, accuse the top one. The statistic is therefore a rank, and
the null is "the killer's hostile rate is just another resident's".

| | Run A (5 turns) | Run B (8 turns) |
|---|---|---|
| Killer hostile rate | 0.00 | 0.12 |
| Residents at least as hostile | 10 of 10 | 5 of 10 |
| p (rank permutation) | 1.00 | 0.50 |

In run B the killer sits **fourth of ten**, behind three bystanders. Sorting by
hostility does not find them.

### 3. Mood distribution — the caveat

Pooled mood frequencies per group, run B, and total variation distance between
them (0 = identical, 1 = no mood in common):

| Group | n | Distribution |
|---|---|---|
| killer | 8 | neutral 0.62, angry 0.12, sad 0.12, none 0.12 |
| secret-keepers | 32 | **embarrassed 0.59**, neutral 0.25, none 0.09, sad 0.06 |
| bystanders | 40 | neutral 0.65, angry 0.20, sad 0.12, surprised 0.03 |

| Comparison | Run A | Run B |
|---|---|---|
| killer vs secret-keepers | 0.85 | **0.59** |
| killer vs bystanders | 0.20 | **0.12** |
| secret-keepers vs bystanders | 0.80 | **0.69** |

**The issue's literal pass condition does not hold.** It reads: *"the killer's
mood distribution is NOT SEPARABLE from the secret-keepers'."* It is separable,
in both runs, and the direction is consistent.

**It is not a leak, and the reason matters.** The killer is separable from the
secret-keepers because the killer reads *ordinary* — statistically
indistinguishable from the five bystanders (TVD 0.12). It is the
**secret-keepers** who stand out: `secret_keeper.role`'s demeanour line produces
`[[MOOD: embarrassed]]` on ~59% of turns, against ~0% for everyone else. They
are a visibly distinct third category, and the killer is not in it.

So the exploit does not open. But the secret-keepers are not doing the job they
exist to do. `roles/README.md` and the plan both justify them as **cover** —
"several innocent residents hide unrelated things, so deflection is common and
hostility stops being a unique signal". Cover requires the killer and the
keepers to look alike. Here they look nothing alike, and the keepers instead
partition the cast into three legible groups.

The killer currently hides among the bystanders, by being unremarkable. That is
a real defence and it is measurably working. It is just not the defence the
design describes, and nobody would know from reading the role files.

## What this says about the plan's open question 1

*"How many secret-keepers is enough cover?"* — on this evidence, **the count is
the wrong lever.** Going from four keepers to eight would deepen the embarrassed
cluster without moving the killer any closer to it. What would change the answer
is the keepers' *demeanour*: `secret_keeper.role` reads "your deflections are
awkward rather than aggressive: an embarrassed person covering something small",
and the model takes that instruction very literally and very consistently.

Two candidate directions, neither taken here because both are content decisions:

1. **Widen the keepers' affect** so `embarrassed` stops being a group signature
   — vary the demeanour line per keeper, or let some deflect coolly.
2. **Accept it and re-state the design.** "The killer hides among ordinary
   residents; the secret-keepers are a decoy layer that reads as a distinct
   group" is a coherent mystery, and it is what actually ships today. If that is
   the intent, `roles/README.md` should say so instead of promising cover.

## Honesty notes

- **Sample size is small.** The killer contributes 8 turns in run B. TVD on
  n=8 is noisy, and the run-A/run-B gap on `killer vs secret-keepers` (0.85 →
  0.59) is most of what that noise looks like. What survives both runs is the
  *ordering* — killer close to bystanders, far from keepers, keepers far from
  bystanders — not the specific values.
- **One cast, one model, one seed regime.** Nothing here generalises to a
  21-resident roster (#173) or to another model without re-running.
- **`--turns 8` is the baseline.** Comparing a future run at a different turn
  count to these numbers is not valid; hostile rate is a per-turn frequency and
  the escalation ladder repeats after five.
- The probe exits non-zero on FAIL and on `CANNOT EVALUATE` (Ollama unreachable,
  or `build/persona_prompt` missing), and those two are distinct in the output.
  A gate that could not run has not passed.
