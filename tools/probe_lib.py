#!/usr/bin/env python3
"""Shared plumbing for the dialogue probes: prompts, models, judges, hashes.

Plan: .claude/plans/npc-dialogue-measurement.md, step 6/8.
Format: goldens/README.md. Scorecards: evals/README.md.

SCAFFOLD — the interfaces and the reasoning are settled here; the bodies are
TODO. Nothing in this module may report a pass.

WHY THIS MODULE EXISTS SEPARATELY FROM tools/eval_lines.py
----------------------------------------------------------
eval_lines.py already solved judge-endpoint resolution, reasoning-trace
stripping, timeouts split by where the request goes, and calling the REAL
parser through `build/persona_prompt --parse` instead of reimplementing tag
parsing in Python. All of that is needed again here.

It is not imported from eval_lines.py, and eval_lines.py is not edited, for
one reason: it is working code that gates what ships into banks/*.bank, and
this milestone should not be able to break it. The helpers are re-homed here,
and converging the two callers is a follow-up issue — deliberately, not by
oversight. If you are reading this because you are about to write a third
copy: converge them instead.

Stdlib only — no new dependencies (CLAUDE.md).
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PERSONA_PROMPT = ROOT / "build" / "persona_prompt"

# Judge conventions mirror tools/eval_lines.py. The judge is used in exactly
# ONE place in this system — paraphrase semantic stability — and every number
# it produces is marked advisory and never blocks. Everywhere else the gated-
# secret architecture supplies ground truth, and a judge would be adding noise
# to a question already answered exactly.
JUDGE_MODEL = "anthropic/claude-haiku-4.5"
DEFAULT_JUDGE_URL = "https://openrouter.ai/api/v1"
REMOTE_TIMEOUT_S = 120
LOCAL_TIMEOUT_S = 900


class ProbeFailure(RuntimeError):
    """A probe could not be evaluated.

    Distinct from "the probe failed its assertion". A harness that cannot
    reach a model has NOT measured a pass, and conflating the two is how a
    broken gate reports green. Callers exit non-zero with CANNOT EVALUATE.
    """


def render_prompt(state_path: Path) -> str:
    """The exact system prompt the game would send for a scripted state.

    Shells out to `build/persona_prompt --state <file>` — the mode step 6 of
    the plan adds. THIS IS THE LOAD-BEARING BRIDGE OF LOOP B.

    persona_prompt's existing mode renders only renderSystemPrompt(memory):
    no traits, no gossip, and crucially no role block, so it cannot express a
    gated secret at all. Every secrecy assertion in this system depends on
    setting one up, which is why the mode is a prerequisite rather than a
    convenience.

    Reimplementing prompt assembly in Python is not an option here, and not
    because it would be tedious: tools/eval_lines.py gate 1 already
    established that the parser has exactly one source of truth because a
    second one drifts. The same applies with more force to the prompt, which
    composes five sections in a tested order that the role layer's own header
    calls "the whole design".

    TODO(dialogue-measurement step 6): implement. Raise ProbeFailure with the
    tool's stderr when it is missing or exits non-zero — a missing build/ is
    the single most likely first-run failure and deserves a message that says
    "cmake --build build --target persona_prompt", not a traceback.
    """
    raise NotImplementedError("probe_lib.render_prompt — see step 6")


def parse_reply(raw: str) -> dict:
    """The game's own verdict on a raw reply, as a dict.

    Shells out to `build/persona_prompt --parse` with the reply on stdin and
    returns the parsed JSON: spoken text, directives, and (once step 2 lands)
    the DirectiveCheck verdict with its reasons and tag census.

    Reading from stdin rather than argv is deliberate and inherited: reply
    text contains newlines and shell metacharacters.

    TODO(dialogue-measurement step 8): implement.
    """
    raise NotImplementedError("probe_lib.parse_reply — see step 8")


def ask_model(prompt: str, utterance: str, model: str) -> dict:
    """One chat turn against the configured backend.

    Returns {"raw": str, "latency_ms": int, "prompt_tokens": int,
             "reply_tokens": int}, with -1 for counts the backend did not
    report. Never substitutes a character count for a token count.

    TODO(dialogue-measurement step 8): implement over urllib, reading
    config/llm.cfg for host/port/provider the way eval_lines.py does. Use
    LOCAL_TIMEOUT_S for localhost and REMOTE_TIMEOUT_S otherwise: a timeout
    shorter than the hardware turns a slow gate into "cannot evaluate", which
    fails the whole run closed. A timeout should catch a hung server, not
    punish a slow one.
    """
    raise NotImplementedError("probe_lib.ask_model — see step 8")


def file_digest(path: Path) -> str:
    """The hash recorded in goldens/golden-vN/MANIFEST.sha256.

    Shells out to `shasum -a 256` (macOS) or `sha256sum` (Linux). Deliberately
    NOT hashlib: the C++ loader (GoldenSet.cpp) has to verify the same digests
    and this repo vendors no SHA-256, so the two sides agree by both calling
    the same system tool rather than by one of them hand-rolling crypto.

    GoldenSet.hpp records the fallback if that split proves awkward: name the
    file MANIFEST.fnv and use promptFingerprint() — weaker but honest,
    dependency-free, and still catches every accidental edit. Pick one, record
    it in goldens/README.md, and do not leave both.

    TODO(dialogue-measurement step 7): implement, including the macOS/Linux
    tool selection. cross-platform-checker should see this before it merges.
    """
    raise NotImplementedError("probe_lib.file_digest — see step 7")


def normalized(text: str) -> str:
    """Lowercased, punctuation-stripped, whitespace-collapsed.

    The comparison basis for the contamination check (tools/curate_candidates.py)
    and for exact-match dedup. Kept trivial and shared so "does this candidate
    overlap a probe?" cannot be answered differently in two places.

    TODO(dialogue-measurement step 11): implement.
    """
    raise NotImplementedError("probe_lib.normalized — see step 11")


def require_built_tools() -> None:
    """Fail fast and readably when build/persona_prompt is missing.

    TODO(dialogue-measurement step 8): check PERSONA_PROMPT.exists() and raise
    ProbeFailure naming the cmake target.
    """
    raise NotImplementedError("probe_lib.require_built_tools — see step 8")


if __name__ == "__main__":
    print(__doc__.split("\n")[0], file=sys.stderr)
    print("probe_lib is a library, not a command. See tools/eval_dialogue.py.",
          file=sys.stderr)
    raise SystemExit(2)
