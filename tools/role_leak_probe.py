#!/usr/bin/env python3
"""Does the role layer leak the killer through a side channel?

Plan: .claude/plans/role-layer.md, step 5. Format: roles/README.md.

SCAFFOLD — the measurement is designed here, not implemented. See the TODOs.

WHY THIS EXISTS, AND WHY IT IS THE REAL DELIVERABLE
---------------------------------------------------
The role layer's two worst failures were invisible to every unit test and were
only found by running the model. Measured against qwen3:8b at think=false:

  MOOD LEAK    Same persona, same question, with and without a role block:
               innocent Marge laughs when accused ([[MOOD: amused]]); guilty
               Marge goes [[MOOD: angry]] on turn one. Mood drives the rendered
               face, so a player could ask all twenty residents one question
               and watch for the angry one — the mystery solved in twenty
               questions with zero reasoning. Worse in multiplayer, where
               NpcMoodUpdate is broadcast to every client.

  ACTION LEAK  With the role block placed before "Stay in character", the
               killer emitted [[ACTION: call_police]] on three of five turns.
               A murderer summoning the police is a bug and a second tell.

A green `make -C tests test` says nothing about either. That is the point of
this file.

THE PASS CONDITION
------------------
    The killer's mood distribution is NOT SEPARABLE from the secret-keepers'.

Not "the killer is never angry" — suppressing affect entirely produces a cast of
bland androids, and the fix for the leak is to hide the signal in noise rather
than remove it. Several innocent residents hide unrelated things, so deflection
is common and hostility stops being a unique signal.

Separability is the metric because it is what a PLAYER can actually exploit. If
a player sorting residents by "how angry did that answer look" gets the killer
at the top more often than chance, the mystery is broken regardless of how good
the prose is.

Slow, and needs Ollama, so this is a tools/ probe run before shipping content —
never part of the unit suite. Same shape as tools/bench_npc_models.py.

Stdlib only, no new dependencies (CLAUDE.md).

Usage:
  python3 tools/role_leak_probe.py --cast saves/probe-cast.json --turns 5
  python3 tools/role_leak_probe.py --cast ... --model qwen3:8b --json out.json
"""
import argparse
import sys

# The neutral question every resident is asked. Neutral on purpose: an
# accusatory prompt makes everyone defensive and hides the very difference this
# is trying to measure.
DEFAULT_PROBE = "Where were you on Tuesday evening?"

# Emitted by parseDirectives; the probe counts these rather than reading prose.
MOOD_TAGS = ("neutral", "happy", "angry", "sad", "embarrassed", "surprised")


def build_arg_parser() -> argparse.ArgumentParser:
    """CLI surface. Kept small — the probe has one job."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cast", required=True,
                        help="JSON: residents, their personas, roles and secrets")
    parser.add_argument("--turns", type=int, default=5,
                        help="escalating turns per resident (default 5)")
    parser.add_argument("--model", default="qwen3:8b")
    parser.add_argument("--json", help="write the raw per-turn results here")
    return parser


def main(argv: list[str]) -> int:
    """Run the probe and report separability.

    TODO(role step 5): implement in this order — each step is independently
    checkable, and the last one is the only one that matters.

    1. Load the cast. One killer, four or five secret_keepers, the rest
       bystanders and witnesses. Open question 1 in the plan asks how many
       secret-keepers is enough cover; four is a GUESS and this probe is what
       should decide it. Report the number used in the output so a later run
       can be compared.

    2. For each resident, ask the probe question and `--turns - 1` escalating
       follow-ups. Escalation matters: the measured leak appeared on turn one,
       but a role that holds for one turn and cracks on four is still broken.

    3. Parse each reply with the REAL parseDirectives, by shelling out to
       `build/persona_prompt --parse` on stdin. Do not reimplement tag parsing
       in Python — tools/eval_lines.py gate 1 already established that the
       parser has exactly one source of truth, and a second one drifts.

    4. Tally per resident: mood tag counts, and any [[ACTION:]] directive that
       was not asked for. A killer emitting call_police is an automatic FAIL
       regardless of the mood numbers.

    5. Compute separability between the killer and the secret_keeper group.
       Report the metric AND the raw distributions — a single number nobody can
       interrogate is how a bad pass condition survives.

       Keep the statistic simple and explainable; a permutation test over "how
       often is the killer the most-angry resident" is defensible and needs no
       new dependency. FAIL if the killer is identifiable more often than
       chance.

    6. Exit non-zero on FAIL so this can gate a content PR by hand, and write
       the result beside bench/REPORT.md so the next model change is checked
       against a known baseline rather than a memory of one.

    HONESTY REQUIREMENT: report the sample size and the number of Ollama calls
    alongside the verdict. A pass over three turns and six residents is not
    evidence, and a probe that hides its own n is worse than no probe.
    """
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    print("role_leak_probe: scaffold only — see the TODO in main().",
          file=sys.stderr)
    print(f"  would probe cast={args.cast} turns={args.turns} model={args.model}",
          file=sys.stderr)
    print(f"  probe question: {DEFAULT_PROBE!r}", file=sys.stderr)
    print(f"  mood tags counted: {', '.join(MOOD_TAGS)}", file=sys.stderr)
    return 2  # not implemented; never let a scaffold report a pass


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
