#!/usr/bin/env python3
"""Run the frozen golden set and produce a dialogue scorecard. LOOP B.

Plan: .claude/plans/npc-dialogue-measurement.md, steps 8-10.
Format: goldens/README.md. Output: evals/README.md.

SCAFFOLD — the measurement is designed here, not implemented. See the TODOs.
main() returns 2. A scaffold must never report a pass.

WHAT THIS IS, AND WHAT IT DELIBERATELY IS NOT
---------------------------------------------
It is a CALIBRATED INSTRUMENT. It replays frozen probes against scripted game
states and reports numbers. It is not an autotuner: nothing here promotes a
response into a prompt, adjusts a threshold, or edits a trait. If a metric
regresses, this reports it and exits non-zero. Fixing it is a person's job,
and that separation is the reason the numbers can be trusted at all.

TWO CLASSES OF RESULT, AND THE LINE BETWEEN THEM
------------------------------------------------
BLOCKING (deterministic). No judge model anywhere near these. The gated-secret
architecture means the harness knows the ground truth exactly: it put the
secret in the prompt, or it did not, and it knows the token.

  secrecy      gate closed -> secret_token MUST NOT appear in the spoken text
               gate open   -> secret_token MUST appear
               The second is not optional. A model that reveals nothing under
               any pressure is not passing the secrecy test; it is broken in a
               way the first assertion alone cannot see.
  consistency  a probe marked stay_consistent_with must not contradict the
               named probe's answer across save-branches
  schema       DirectiveCheck validity rate must not fall below baseline

ADVISORY (statistical, reported against baseline, never blocking on its own):
  distinct-1 / distinct-2, self-BLEU on a sample, response-length mean and
  variance, em-dashes per 1k characters, sycophancy-phrase flag rate,
  paraphrase semantic stability (the ONLY judged number in the system).

Every advisory number carries "advisory": true in the JSON and its own n in
the report. role_leak_probe.py's honesty requirement, inherited verbatim: a
probe that hides its own sample size is worse than no probe.

RELATIONSHIP TO tools/role_leak_probe.py (issue #200)
-----------------------------------------------------
That probe is NOT superseded by this one and is not modified by this milestone.
It asks a distributional question about a whole cast — across twenty residents,
is the killer separable from the secret-keepers by mood alone? This harness
asks an exact question about one turn — did this reply contain this token?
Neither subsumes the other, and a cast-level leak is invisible to every probe
here precisely because each golden probe looks at one NPC.

What they share is plumbing, and that lives in tools/probe_lib.py.

Slow, and needs a model, so this is never part of `make -C tests test` — the
same rule tools/eval_lines.py follows. What IS unit-tested offline: the
format, the loader, the scorecard arithmetic and the contamination check.

Stdlib only — no new dependencies (CLAUDE.md).

Usage:
  python3 tools/eval_dialogue.py --set goldens/golden-v1
  python3 tools/eval_dialogue.py --set goldens/golden-v1 --blocking-only
  python3 tools/eval_dialogue.py --set goldens/golden-v2 --baseline evals/baselines
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import probe_lib  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SET = ROOT / "goldens" / "golden-v1"
BASELINE_DIR = ROOT / "evals" / "baselines"
REPORT_DIR = ROOT / "evals" / "reports"

# Phrases that read as sycophancy rather than character. Seeded from
# eval_lines.py's FOURTH_WALL / PRIOR_MEETING lists. This is a FLAG RATE, not
# a gate, precisely because the list is a guess — gating on a guessed list
# would make the guess load-bearing.
SYCOPHANCY = [
    "great question", "what a great", "i'd be happy to", "i am happy to",
    "absolutely!", "of course!", "certainly!", "you're right to ask",
    "that's a really good", "i appreciate you",
]

# Paraphrases needed before the invariance number is reported without a
# small-sample warning.
MIN_PARAPHRASES = 5


def build_arg_parser() -> argparse.ArgumentParser:
    """CLI surface. Small on purpose — the harness has one job."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--set", default=str(DEFAULT_SET),
                        help="golden set directory (default goldens/golden-v1)")
    parser.add_argument("--blocking-only", action="store_true",
                        help="deterministic assertions only; no model judge")
    parser.add_argument("--baseline", default=str(BASELINE_DIR),
                        help="where baselines are read and first written")
    parser.add_argument("--model", default=None,
                        help="override the model from config/llm.cfg")
    parser.add_argument("--json", help="write the raw scorecard here")
    return parser


def main(argv: list[str]) -> int:
    """Run the set and print a scorecard.

    TODO(dialogue-measurement steps 8-10): implement in this order. Each step
    is independently checkable and the ORDER IS THE DESIGN — the freeze check
    has to come before anything is measured, and the baseline write has to
    come last.

    1. LOAD AND FREEZE-CHECK the set. Refuse on any manifest mismatch, naming
       the file. Never partially load: scoring a subset and comparing it to a
       baseline taken over the full set silently changes the denominator, and
       nothing downstream would show it.

    2. RENDER each probe's prompt through `build/persona_prompt --state`
       (probe_lib.render_prompt). Do not assemble prompts in Python.

    3. ASK the model each probe's utterance, then each paraphrase. Record raw
       replies verbatim before anything strips them.

    4. PARSE every reply through `build/persona_prompt --parse`
       (probe_lib.parse_reply) — the real DirectiveCheck, not a Python
       reimplementation.

    5. BLOCKING ASSERTIONS, in this order:
       a. secrecy, both directions (see the module docstring)
       b. consistency for stay_consistent_with probes
       c. schema-validity rate vs baseline
       Report every failure, do not stop at the first: an operator wants the
       full picture from one run, not a bisect.

    6. ADVISORY METRICS. Compute against the baseline for this set VERSION.
       Report deltas, never verdicts.

    7. PARAPHRASE INVARIANCE. Ask each probe 5+ ways, judge semantic stability
       of the answers. Mark advisory. Below MIN_PARAPHRASES, report the number
       AND the small-sample warning rather than suppressing either.

    8. VERSION REPORTING. Print the current-version total AND the score over
       the subset of probes that existed in the ORIGINAL version, so a v2 run
       stays comparable to v1's history. Both numbers, always — reporting only
       the total is how a version bump quietly resets a trend line.

    9. BASELINE. If none exists for this version, write it, print
       "BASELINE ESTABLISHED" and exit 0. Do NOT fail a first run: a harness
       that fails when it has nothing to compare against teaches people to
       pass --force, and then it never gates anything again.

    10. EXIT non-zero on any blocking failure, and non-zero with
        CANNOT EVALUATE if the model was unreachable. Those two must be
        distinguishable in the output — a gate that could not run has not
        passed, and must never be mistaken for one that did.

    HONESTY REQUIREMENT (inherited from role_leak_probe.py): report the sample
    size, the model, the golden-set version and the number of model calls
    alongside every verdict.
    """
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    print("eval_dialogue: scaffold only — see the TODO in main().",
          file=sys.stderr)
    print(f"  would evaluate set={args.set} blocking_only={args.blocking_only}",
          file=sys.stderr)
    print(f"  baselines={args.baseline}", file=sys.stderr)
    print(f"  judge used for paraphrase stability only: {probe_lib.JUDGE_MODEL}",
          file=sys.stderr)
    return 2  # not implemented; never let a scaffold report a pass


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
