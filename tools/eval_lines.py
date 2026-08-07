#!/usr/bin/env python3
"""Score candidate NPC bank lines against the five-dimension rubric.

Plan: .claude/plans/npc-line-bank.md, step 5. Format: banks/README.md.

A line enters a bank only if it passes ALL gates:

  1 tag_compliance  parses via the REAL parseDirectives (build/persona_prompt's
                    sibling CLI, not a Python regex reimplementation), exactly
                    one known mood tag, no malformed directives
  2 latency         p95 of the served reply under 50 ms
  3 fidelity        LLM judge scores 1-5 against the persona's rendered prompt
                    AND its resolved traits/*.trait rules; >= 4 mean, none < 3
  4 non_regression  blind A/B against a live qwen3:8b answer to the same player
                    line; banked must win or tie >= 70% of pairs
  5 leakage         zero fourth-wall breaks, spoken brackets, knowledge-boundary
                    violations, or prior-meeting references in familiarity=first

Gate 4 is the load-bearing one: it answers "does the cache make the game worse
to make it faster?" with a measurement rather than a promise. The judge is blind
to which side is which and pair order is shuffled.

Importable by tools/refine_lines.py so scoring lives in exactly one place.

Stdlib only — no new dependencies (CLAUDE.md). Usage:
  python3 tools/eval_lines.py --bank banks/baker.bank
  python3 tools/eval_lines.py --bank banks/baker.bank --gates 1,5   # offline only
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OLLAMA = "http://localhost:11434"

# Gates 1, 2 and 5 are offline. Gates 3 and 4 need the network: 3 calls the
# cloud judge, 4 additionally calls local Ollama for the live baseline.
OFFLINE_GATES = (1, 2, 5)
ALL_GATES = (1, 2, 3, 4, 5)

# Thresholds from the plan. Changing one changes what ships — do it in a commit
# that says why, not as a side effect.
FIDELITY_MEAN_MIN = 4.0
FIDELITY_FLOOR = 3
NON_REGRESSION_WIN_RATE = 0.70
LATENCY_P95_MS = 50.0


class GateFailure(Exception):
    """A gate could not be EVALUATED (not the same as a line failing it).

    Raised when Ollama is down, the judge key is missing, or persona_prompt is
    unbuilt. The caller must fail closed: a half-scored bank must never be
    written, because an unscored line is indistinguishable from a passing one
    once it is sitting in the file.
    """


def parse_bank(path):
    """Parse a .bank file into {"persona": str, "topics": [...]}.

    Mirrors src/core/LineBank.cpp's parser. The two must agree; a divergence
    means the nightly job scores something different from what the game serves.

    TODO(linebank step 5): implement. Keep it a straight port of the C++ —
    resist "improving" the format on this side.
    """
    raise NotImplementedError("TODO(linebank step 5)")


def gate_tag_compliance(reply):
    """Gate 1. Shell out to the real parseDirectives rather than re-regexing it.

    tools/bench_npc_models.py duplicates the mood keyword list in Python and its
    comment admits it must be kept in sync by hand. Do not repeat that here:
    call the C++ so there is one source of truth.

    TODO(linebank step 5): add a tiny `parse_directives` CLI beside
    tools/persona_prompt.cpp, or extend that one with a --parse flag.
    """
    raise NotImplementedError("TODO(linebank step 5)")


def gate_latency(bank_path):
    """Gate 2. Measure served latency over a replayed transcript set.

    Reported, not gated, for hit rate; gated at p95 < 50 ms for served time.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def gate_fidelity(persona_prompt, trait_rules, reply):
    """Gate 3. LLM judge, 1-5, against the persona's OWN rendered prompt.

    Use build/persona_prompt for the prompt text — the same code path the game
    uses — exactly as tools/bench_npc_models.py does. Judging against a
    paraphrase of the persona measures the paraphrase.

    bench/README.md's "Known limits" names this as the thing the existing
    consistency metric wanted and could not have without cloud access. The
    OpenRouter provider path (src/core/OpenAiBackend.cpp, config/llm.cfg) is
    that access.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def gate_non_regression(persona_prompt, player_line, banked_reply):
    """Gate 4. Blind A/B against what qwen3:8b actually says to the same line.

    Shuffle pair order and strip any hint of provenance before the judge sees
    them. If the judge can tell which is which, the number means nothing.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def gate_leakage(persona, topic, reply):
    """Gate 5. Hard fail, zero tolerance.

    Four checks: fourth-wall breaks, spoken brackets (the model saying "MOOD
    happy" out loud), knowledge-boundary violations against the persona's
    `knowledge` field (Marge must not discuss world news), and prior-meeting
    references in a familiarity=first line.

    The last one is the subtle one and the reason familiarity buckets exist —
    see banks/README.md.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def score_bank(bank_path, gates=ALL_GATES):
    """Run `gates` over every reply in a bank. Returns a scorecard dict.

    Raises GateFailure if any requested gate cannot be evaluated. Callers must
    not catch-and-continue — see GateFailure's docstring.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def write_scorecard(scorecard, json_path, markdown_path):
    """Emit machine-readable JSON and the human table committed with the PR.

    JSON goes to bench/out/ (already gitignored); markdown to
    bench/SCORECARD.md, which IS committed — it is the evidence attached to
    each nightly PR.

    TODO(linebank step 5)
    """
    raise NotImplementedError("TODO(linebank step 5)")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bank", type=Path, required=True,
                        help="path to a .bank file to score")
    parser.add_argument("--gates", default="",
                        help="comma-separated gate numbers; default all. "
                             "Use 1,2,5 to score offline with no network.")
    parser.add_argument("--json-out", type=Path,
                        default=ROOT / "bench" / "out" / "line_scores.json")
    parser.add_argument("--markdown-out", type=Path,
                        default=ROOT / "bench" / "SCORECARD.md")
    args = parser.parse_args(argv)

    gates = tuple(int(g) for g in args.gates.split(",")) if args.gates else ALL_GATES

    try:
        scorecard = score_bank(args.bank, gates)
    except NotImplementedError as exc:
        print(f"not implemented yet: {exc}", file=sys.stderr)
        return 2
    except GateFailure as exc:
        # Fail closed. Nothing is written.
        print(f"cannot evaluate: {exc}", file=sys.stderr)
        return 1

    write_scorecard(scorecard, args.json_out, args.markdown_out)
    print(json.dumps(scorecard, indent=2))
    return 0 if scorecard.get("all_passed") else 1


if __name__ == "__main__":
    sys.exit(main())
