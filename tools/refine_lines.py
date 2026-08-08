#!/usr/bin/env python3
"""Nightly job: grow the NPC line banks from the day's real play, then PR them.

Plan: .claude/plans/npc-line-bank.md, step 6. Format: banks/README.md.

Pipeline, in order:

  1 read      saves/conversations.sqlite3 transcripts + saves/ratings/*.jsonl
  2 cluster   group player lines into candidate topics
  3 diff      drop anything the existing banks already cover
  4 author    Claude writes 3-5 reply variants + 8-12 trigger phrasings per gap,
              against that persona's REAL rendered system prompt
  5 score     every candidate through tools/eval_lines.py — all five gates
  6 write     regenerate banks/*.bank with the survivors
  7 verify    make -C tests test
  8 PR        draft PR against dev, scorecard in the body. NEVER merges.

FAILS CLOSED. If Ollama is down, the judge key is missing, the token budget is
exhausted, or the tests fail, the working tree is restored and no PR is opened.
A half-scored bank must never land: once a line is sitting in a .bank file,
nothing distinguishes "scored and passed" from "never scored".

Stdlib + PyGithub only — no new dependencies (CLAUDE.md). GitHub auth follows
.claude/skills/github/SKILL.md: GH_TOKEN from the environment, never printed.

Usage:
  python3 tools/refine_lines.py --dry-run          # steps 1-5, writes nothing
  python3 tools/refine_lines.py                    # full run, opens a draft PR
  python3 tools/refine_lines.py --max-topics 5     # smaller review
"""
import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BANKS = ROOT / "banks"
SAVES = ROOT / "saves"

# Caps. A nightly diff nobody reviews is worse than no nightly diff, and an
# unbounded token spend on an unattended job is how you get a surprise bill.
DEFAULT_MAX_NEW_TOPICS = 10
DEFAULT_TOKEN_BUDGET = 100_000

# Base branch for the nightly PR. Never main — repo rule.
PR_BASE = "dev"


class AbortRun(Exception):
    """Stop before anything is written. The only way this job fails."""


def read_days_transcripts(since):
    """Step 1. Player lines from ConversationStore plus the ratings log.

    ConversationStore stores one row per NPC: (npc_id, summary,
    transcript_json, updated_at). The transcript is a JSON array of
    {role, content}; player lines are role == "user".

    RatingLog writes saves/ratings/{candidates,rejected}.jsonl as
    {ts, persona, traits, player, npc}. Thumbs-up exchanges are a strong signal
    for what to bank; thumbs-down ones are a signal for what to avoid, and both
    are worth reading.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def cluster_topics(player_lines):
    """Step 2. Group near-duplicate player lines into candidate topics.

    Reuse the same lexical similarity the game matches with, so clusters and
    runtime matching cannot disagree — call the C++ through a small CLI rather
    than writing a second scorer in Python. A topic the clusterer invents but
    the matcher can never reach is dead weight in the bank.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def existing_coverage(bank_dir):
    """Step 3. What the banks already answer, so tonight only fills gaps.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def author_topic(persona_name, topic_lines, budget):
    """Step 4. Claude writes variants and triggers for one uncovered topic.

    Two things make or break this:

      - Prompt with the persona's REAL rendered system prompt (build/
        persona_prompt, the exact code path the game uses) plus its resolved
        traits/*.trait rules. Authoring against a summary produces lines that
        score well and sound wrong.
      - Ask for 8-12 trigger paraphrases, not just replies. Trigger breadth is
        what lets a lexical matcher cover paraphrase; it is the whole reason
        this design needs no embedding model.

    Must decrement `budget` and raise AbortRun when exhausted.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def write_banks(banks, bank_dir):
    """Step 6. Regenerate .bank files. Stable ordering — sort topics by id and
    keep replies in authored order, so an unchanged night produces an EMPTY
    diff. A job that churns the file every night trains you to stop reading it.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def run_tests():
    """Step 7. `make -C tests test`. Non-zero exit aborts the run.

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def open_draft_pr(branch, scorecard_markdown):
    """Step 8. Draft PR against dev via PyGithub. Never merges, never touches main.

    Body carries the scorecard so review is "read the numbers, read the diff",
    not "go run something". Stage explicit paths only — never `git add .`
    (repo rule; saves/, docs/learning/ and .claude/plans/ must stay local).

    TODO(linebank step 6)
    """
    raise NotImplementedError("TODO(linebank step 6)")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry-run", action="store_true",
                        help="steps 1-5 only: report what would change, write nothing")
    parser.add_argument("--max-topics", type=int, default=DEFAULT_MAX_NEW_TOPICS,
                        help=f"cap new topics per run (default {DEFAULT_MAX_NEW_TOPICS}). "
                             "What gets deferred is logged, never silently dropped.")
    parser.add_argument("--token-budget", type=int, default=DEFAULT_TOKEN_BUDGET,
                        help=f"hard cloud token ceiling (default {DEFAULT_TOKEN_BUDGET})")
    parser.add_argument("--since", default="24h",
                        help="how far back to read transcripts")
    args = parser.parse_args(argv)

    try:
        lines = read_days_transcripts(args.since)
        if not lines:
            # A quiet day is a success, not a failure. Open no PR, exit 0.
            print("no new transcripts — nothing to refine")
            return 0
        raise NotImplementedError("TODO(linebank step 6): steps 2-8")
    except NotImplementedError as exc:
        print(f"not implemented yet: {exc}", file=sys.stderr)
        return 2
    except AbortRun as exc:
        # Fail closed: working tree restored, no PR.
        print(f"aborted before writing: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
