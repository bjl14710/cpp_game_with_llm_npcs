#!/usr/bin/env python3
"""Turn well-rated telemetry turns into REVIEW candidates. The Part C bridge.

Plan: .claude/plans/npc-dialogue-measurement.md, step 11.

SCAFFOLD — designed here, not implemented. main() returns 2.

THIS IS DELIBERATELY MANUAL, AND THE MANUAL STEP IS THE FEATURE.
Well-rated turns are written to a review file as CANDIDATE few-shot examples.
Nothing is promoted into personas/*.persona, traits/*.trait or banks/*.bank by
this tool, ever. Promotion is a human edit, reviewed as a normal diff.

src/core/RatingLog.hpp already states that rule for the F1/F2 loop (issue
#118) and pins it with a test. This extends the same rule to the richer
telemetry capture; it does not weaken it.

THE CONTAMINATION CHECK IS THE PART THAT IS ACTUALLY NEW.
A candidate that overlaps a golden probe cannot be promoted, because the
moment probe content appears in a prompt, every measurement taken with that
probe stops meaning anything — including the measurements already taken and
already used to justify decisions. The failure is silent and retroactive,
which is exactly why it needs a tool rather than a rule in a document.

The golden set is never the side that moves. A collision is resolved by
dropping the candidate, not by rewriting the probe.

Stdlib only — no new dependencies (CLAUDE.md).

Usage:
  python3 tools/curate_candidates.py --since 2026-08-01
  python3 tools/curate_candidates.py --db saves/telemetry.sqlite3 --min-rating 1
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import probe_lib  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DB = ROOT / "saves" / "telemetry.sqlite3"
DEFAULT_GOLDENS = ROOT / "goldens"
REVIEW_DIR = ROOT / "review"

# Overlap threshold for the contamination check: an exact normalized match, or
# a shared n-gram of this length or more, against any probe utterance,
# paraphrase or secret token. Short and blunt on purpose — a false positive
# costs one discarded candidate, a false negative costs every number the
# golden set has ever produced.
OVERLAP_NGRAM = 5


def build_arg_parser() -> argparse.ArgumentParser:
    """CLI surface."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--db", default=str(DEFAULT_DB),
                        help="telemetry database (default saves/telemetry.sqlite3)")
    parser.add_argument("--goldens", default=str(DEFAULT_GOLDENS),
                        help="directory holding every golden-vN to check against")
    parser.add_argument("--min-rating", type=int, default=1,
                        help="lowest dev rating to consider (default 1)")
    parser.add_argument("--since", help="ISO date; only turns at or after it")
    parser.add_argument("--out", default=str(REVIEW_DIR),
                        help="where the review file is written")
    return parser


def main(argv: list[str]) -> int:
    """Collect candidates, refuse contaminated ones, write the review file.

    TODO(dialogue-measurement step 11): implement in this order.

    1. Read dialogue_turns where rating >= --min-rating, schema_valid = 1 and
       banked = 0. Banked replies are authored lines that are already in
       banks/*.bank — promoting one back in is a no-op that looks like work.

    2. Load EVERY golden-vN under --goldens, not just the newest. A probe
       retired from v2 was still used to take v1's measurements, and those
       numbers are still in the trend line.

    3. CONTAMINATION CHECK, per candidate, against every probe utterance,
       paraphrase and secret token:
         - exact match on probe_lib.normalized(), or
         - a shared contiguous n-gram of OVERLAP_NGRAM words or more
       On any hit: refuse the candidate BY NAME, print which probe it collided
       with, and count it. Do not write it to the review file even flagged —
       a flagged line in a review file gets promoted by someone in a hurry.

    4. Write review/candidates-<date>.md: the player line, the reply, the NPC,
       the active traits, the prompt version and hash, and the rating. Markdown
       because a human reads it and then hand-edits a content file; the whole
       point is that this output is not machine-consumable.

    5. Exit NON-ZERO if any candidate was refused, so a contamination event is
       impossible to miss in a script. Exit 0 with a count when clean.

    DO NOT ADD: a --promote flag, an --auto flag, or anything that writes to
    personas/, traits/ or banks/. If a future issue asks for one, that issue is
    asking to delete the measurement system's guarantee, and it should say so
    out loud first.
    """
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    print("curate_candidates: scaffold only — see the TODO in main().",
          file=sys.stderr)
    print(f"  would read {args.db} at rating >= {args.min_rating}",
          file=sys.stderr)
    print(f"  would check against every golden set under {args.goldens}",
          file=sys.stderr)
    print(f"  overlap n-gram: {OVERLAP_NGRAM} words "
          f"(normalizer: probe_lib.normalized)", file=sys.stderr)
    print(f"  probe_lib loaded from {probe_lib.ROOT}", file=sys.stderr)
    return 2  # not implemented; never let a scaffold report a clean run


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
