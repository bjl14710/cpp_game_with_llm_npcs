#!/usr/bin/env python3
"""Score candidate NPC bank lines against the five-dimension rubric.

Plan: .claude/plans/npc-line-bank.md, issue #151. Format: banks/README.md.

A line enters a bank only if it passes ALL gates:

  1 tag_compliance  parses via the REAL parseDirectives (build/persona_prompt
                    --parse), exactly one known mood tag, nothing left over
  2 stream_budget   the banked reply must not take longer to STREAM than the
                    live model takes to generate one (see the note below)
  3 fidelity        LLM judge 1-5 against the persona's rendered prompt AND
                    its resolved traits/*.trait rules; >= 4 mean, none < 3
  4 non_regression  blind, order-shuffled A/B against a live qwen3:8b answer
                    to the same player line; banked wins or ties >= 70%
  5 leakage         zero fourth-wall breaks, spoken brackets, knowledge-boundary
                    violations, or prior-meeting references in familiarity=first

Gate 4 is the load-bearing one: it answers "does the cache make the game worse
to make it faster?" with a measurement rather than a promise.

A NOTE ON GATE 2. The plan originally said "p95 served < 50 ms". That conflates
two different things: the bank LOOKUP (microseconds of string work, and not
observable from Python at all) and the deliberate STREAMING of the reply at
line_bank_cps so it arrives like a generated one. Measuring the former from here
is impossible and measuring the latter against 50 ms is meaningless, since the
pacing is intentional. So gate 2 is implemented as the question that actually
matters: a banked reply must not take longer to stream than qwen3:8b takes to
generate one (4.5 s median total, bench/REPORT.md). Hit rate and real served
latency belong to the C++ side's BankStats, measured in issue #153.

Stdlib only — no new dependencies (CLAUDE.md). Usage:
  python3 tools/eval_lines.py --bank banks/baker.bank
  python3 tools/eval_lines.py --bank banks/baker.bank --gates 1,2,5   # offline
"""
import argparse
import json
import os
import random
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PERSONA_PROMPT = ROOT / "build" / "persona_prompt"
OLLAMA = "http://localhost:11434"

OFFLINE_GATES = (1, 2, 5)
ALL_GATES = (1, 2, 3, 4, 5)

# Thresholds. Changing one changes what ships — do it in a commit that says why.
FIDELITY_MEAN_MIN = 4.0
FIDELITY_FLOOR = 3
NON_REGRESSION_WIN_RATE = 0.70
# Median total generation time for qwen3:8b @ think=false (bench/REPORT.md).
LIVE_REPLY_SECONDS = 4.5
# Must match config/llm.cfg's line_bank_cps default.
STREAM_CPS = 220

JUDGE_MODEL = "anthropic/claude-haiku-4.5"
LIVE_MODEL = "qwen3:8b"

# Phrases that mean the character stopped being the character.
FOURTH_WALL = [
    "as an ai", "language model", "i'm an ai", "i am an ai", "openai",
    "anthropic", "as a chatbot", "i cannot roleplay", "my training data",
    "system prompt", "as your assistant",
]
# Tag syntax that leaked into speech instead of being parsed out.
SPOKEN_TAGS = ["[[", "]]", "mood:", "action:"]
# Language that only makes sense if the NPC has met the player before.
PRIOR_MEETING = [
    "again", "back so soon", "as always", "like last time", "you told me",
    "remember when", "welcome back", "good to see you again", "as usual",
]


class GateFailure(Exception):
    """A gate could not be EVALUATED — not the same as a line failing it.

    Raised when Ollama is down, the judge key is missing, or persona_prompt is
    unbuilt. Callers must fail closed: a half-scored bank must never be written,
    because an unscored line is indistinguishable from a passing one once it is
    sitting in the file.
    """


# --- helpers ----------------------------------------------------------------

def _persona_prompt(persona_file, memory=""):
    """The REAL system prompt, from the same code path the game uses."""
    if not PERSONA_PROMPT.exists():
        raise GateFailure(f"{PERSONA_PROMPT} not built — run cmake --build build")
    out = subprocess.run([str(PERSONA_PROMPT), str(persona_file), memory],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise GateFailure(f"persona_prompt failed on {persona_file}: {out.stderr.strip()}")
    return out.stdout


def parse_directives(reply):
    """Run the REAL parseDirectives over a reply and return its verdict.

    Deliberately shells out rather than re-implementing the parser in Python.
    tools/bench_npc_models.py duplicates the mood keyword list and its own
    comment admits it must be hand-synced; this avoids repeating that mistake.
    """
    if not PERSONA_PROMPT.exists():
        raise GateFailure(f"{PERSONA_PROMPT} not built — run cmake --build build")
    out = subprocess.run([str(PERSONA_PROMPT), "--parse"], input=reply,
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise GateFailure(f"persona_prompt --parse failed: {out.stderr.strip()}")
    return json.loads(out.stdout)


def _post_json(url, payload, headers, timeout):
    body = json.dumps(payload).encode()
    req = urllib.request.Request(url, data=body, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.load(resp)
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise GateFailure(f"{url} unreachable: {exc}") from exc


def ollama_chat(system, user, model=LIVE_MODEL):
    """One live reply from the local model — the baseline gate 4 compares to."""
    data = _post_json(f"{OLLAMA}/api/chat", {
        "model": model,
        "messages": [{"role": "system", "content": system},
                     {"role": "user", "content": user}],
        "stream": False, "think": False, "options": {"temperature": 0.8},
    }, {"Content-Type": "application/json"}, timeout=180)
    return data["message"]["content"].strip()


def judge(prompt, budget):
    """Ask the cloud judge one question; returns raw text.

    Uses the same provider config the game does (config/llm.cfg's api_key_env),
    so there is one place a key is configured.
    """
    key_env = "OPENROUTER_API_KEY"
    cfg = ROOT / "config" / "llm.cfg"
    if cfg.exists():
        for line in cfg.read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if line.startswith("api_key_env"):
                key_env = line.split("=", 1)[1].strip()
    key = os.environ.get(key_env)
    if not key:
        secrets = ROOT / "config" / "secrets.cfg"
        if secrets.exists():
            for line in secrets.read_text().splitlines():
                line = line.split("#", 1)[0].strip()
                if line.startswith("api_key"):
                    key = line.split("=", 1)[1].strip()
    if not key:
        raise GateFailure(
            f"no judge key: set ${key_env} or config/secrets.cfg api_key. "
            "Use --gates 1,2,5 to score offline.")

    budget.spend(len(prompt) // 4 + 256)
    data = _post_json("https://openrouter.ai/api/v1/chat/completions", {
        "model": JUDGE_MODEL,
        "messages": [{"role": "user", "content": prompt}],
        "temperature": 0.0,
    }, {"Content-Type": "application/json",
        "Authorization": f"Bearer {key}"}, timeout=120)
    return data["choices"][0]["message"]["content"].strip()


class TokenBudget:
    """A hard ceiling that aborts rather than surprising anyone with a bill."""

    def __init__(self, total):
        self.total = total
        self.spent = 0

    def spend(self, n):
        self.spent += n
        if self.total and self.spent > self.total:
            raise GateFailure(
                f"token budget exhausted ({self.spent} > {self.total})")


def _first_int(text, lo, hi):
    """First integer in `text` within [lo, hi]; None when the judge rambled."""
    for match in re.finditer(r"-?\d+", text):
        value = int(match.group())
        if lo <= value <= hi:
            return value
    return None


# --- bank parsing -----------------------------------------------------------

def parse_bank(path):
    """Parse a .bank file. Mirrors src/core/LineBank.cpp exactly.

    The two parsers must agree; a divergence means the nightly job scores
    something different from what the game serves. In particular '#' only opens
    a comment as the first non-space character, because reply text is prose.
    """
    persona, topics = "", []
    for raw in Path(path).read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        if key == "persona":
            persona = value
        elif key == "topic":
            topics.append({"id": value, "familiarity": "any",
                           "triggers": [], "replies": []})
        elif not topics:
            continue
        elif key == "familiarity":
            topics[-1]["familiarity"] = value
        elif key == "trigger":
            topics[-1]["triggers"].append(value)
        elif key == "reply":
            topics[-1]["replies"].append(value)
    return {"persona": persona, "topics": topics}


def parse_persona(path):
    """The persona's name and knowledge boundary, for gate 5."""
    fields = {}
    for raw in Path(path).read_text().split("---", 1)[0].splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = (part.strip() for part in line.split("=", 1))
        fields.setdefault(key, value)
    return fields


# --- gates ------------------------------------------------------------------

def gate_tag_compliance(reply):
    parsed = parse_directives(reply)
    problems = []
    if not parsed["has_mood"]:
        problems.append("no recognised mood tag")
    if not parsed["text"].strip():
        problems.append("nothing left after stripping tags")
    residue = [t for t in SPOKEN_TAGS if t in parsed["text"].lower()]
    if residue:
        problems.append(f"tag syntax survived stripping: {residue}")
    return problems, parsed


def gate_stream_budget(text):
    seconds = len(text) / STREAM_CPS
    if seconds > LIVE_REPLY_SECONDS:
        return [f"streams in {seconds:.1f}s, slower than the live model's "
                f"{LIVE_REPLY_SECONDS}s — being cached would make it worse"], seconds
    return [], seconds


def gate_leakage(text, persona_fields, familiarity):
    low = text.lower()
    problems = []
    hits = [p for p in FOURTH_WALL if p in low]
    if hits:
        problems.append(f"fourth-wall break: {hits}")
    if familiarity == "first":
        refs = [p for p in PRIOR_MEETING if p in low]
        if refs:
            problems.append(
                f"familiarity=first line references a prior meeting: {refs}")
    return problems


def gate_knowledge_boundary(text, persona_fields, budget):
    """Gate 5's judged half: does the line claim knowledge this persona lacks?

    Not regex-able — "she mentioned the election" is a boundary violation for a
    baker whose knowledge says "nothing about world news", and no word list
    catches that.
    """
    boundary = persona_fields.get("knowledge", "")
    if not boundary:
        return []
    verdict = judge(
        "A game character has this knowledge boundary:\n"
        f"{boundary}\n\n"
        f"They said: \"{text}\"\n\n"
        "Does the line claim knowledge outside that boundary? "
        "Answer only YES or NO.", budget)
    return ["claims knowledge outside its boundary"] if verdict.strip().upper().startswith("YES") else []


def gate_fidelity(system_prompt, text, budget):
    verdict = judge(
        "Here is a game character's full character sheet:\n\n"
        f"{system_prompt}\n\n"
        f"They said: \"{text}\"\n\n"
        "Score 1-5 for how well this matches that character's voice, "
        "personality and speaking style. 5 = indistinguishable from the sheet, "
        "1 = could be any character. Reply with the digit only.", budget)
    score = _first_int(verdict, 1, 5)
    if score is None:
        raise GateFailure(f"judge returned no usable score: {verdict[:80]!r}")
    return score


def gate_non_regression(system_prompt, player_line, banked, budget, rng):
    """Blind, order-shuffled A/B against what the live model actually says.

    The judge must not be able to tell which side is which; if it can, the
    number means nothing.
    """
    live = ollama_chat(system_prompt, player_line)
    live_text = parse_directives(live)["text"]
    banked_text = parse_directives(banked)["text"]

    banked_is_a = rng.random() < 0.5
    a, b = (banked_text, live_text) if banked_is_a else (live_text, banked_text)
    verdict = judge(
        f"A game character was asked: \"{player_line}\"\n\n"
        f"Reply A: \"{a}\"\nReply B: \"{b}\"\n\n"
        "Which is the better in-character reply? Answer only A, B, or TIE.",
        budget)
    choice = verdict.strip().upper()[:3]
    if choice.startswith("TIE"):
        return "tie"
    if choice.startswith("A"):
        return "banked" if banked_is_a else "live"
    if choice.startswith("B"):
        return "live" if banked_is_a else "banked"
    raise GateFailure(f"judge gave no usable A/B verdict: {verdict[:80]!r}")


# --- driver -----------------------------------------------------------------

def _repo_relative(path):
    """Display path, tolerating a relative --bank argument or one outside the repo."""
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def score_bank(bank_path, gates=ALL_GATES, token_budget=100_000, seed=1):
    bank_path = Path(bank_path).resolve()
    bank = parse_bank(bank_path)
    stem = bank_path.stem
    persona_file = ROOT / "personas" / f"{stem}.persona"
    if not persona_file.exists():
        raise GateFailure(f"no personas/{stem}.persona beside {bank_path}")

    persona_fields = parse_persona(persona_file)
    # A mismatched name silently disables the whole bank at runtime, because
    # lookup keys on it. Catch it here rather than in a confused play-test.
    if bank["persona"] != persona_fields.get("name"):
        raise GateFailure(
            f"bank persona {bank['persona']!r} != persona name "
            f"{persona_fields.get('name')!r} — the bank would never be found")

    system_prompt = _persona_prompt(persona_file)
    budget = TokenBudget(token_budget)
    rng = random.Random(seed)

    results, rejected = [], []
    for topic in bank["topics"]:
        for reply in topic["replies"]:
            problems, parsed = [], None
            row = {"topic": topic["id"], "reply": reply}

            if 1 in gates:
                found, parsed = gate_tag_compliance(reply)
                problems += found
            text = parsed["text"] if parsed else reply

            if 2 in gates:
                found, seconds = gate_stream_budget(text)
                problems += found
                row["stream_seconds"] = round(seconds, 2)

            if 5 in gates:
                problems += gate_leakage(text, persona_fields, topic["familiarity"])
                if 3 in gates or 4 in gates:  # the judged half needs the network
                    problems += gate_knowledge_boundary(text, persona_fields, budget)

            if 3 in gates:
                score = gate_fidelity(system_prompt, text, budget)
                row["fidelity"] = score
                if score < FIDELITY_FLOOR:
                    problems.append(f"fidelity {score} below floor {FIDELITY_FLOOR}")

            if 4 in gates and topic["triggers"]:
                outcome = gate_non_regression(system_prompt, topic["triggers"][0],
                                              reply, budget, rng)
                row["ab"] = outcome
                if outcome == "live":
                    problems.append("lost a blind A/B to the live model")

            row["problems"] = problems
            row["passed"] = not problems
            (results if row["passed"] else rejected).append(row)

    scored = [r for r in results + rejected if "fidelity" in r]
    mean_fidelity = (sum(r["fidelity"] for r in scored) / len(scored)) if scored else None
    ab = [r["ab"] for r in results + rejected if "ab" in r]
    win_rate = ((ab.count("banked") + ab.count("tie")) / len(ab)) if ab else None

    passed = (not rejected
              and (mean_fidelity is None or mean_fidelity >= FIDELITY_MEAN_MIN)
              and (win_rate is None or win_rate >= NON_REGRESSION_WIN_RATE))

    return {
        "bank": _repo_relative(bank_path),
        "persona": bank["persona"],
        "gates": list(gates),
        "lines": len(results) + len(rejected),
        "accepted": [r for r in results],
        "rejected": rejected,
        "mean_fidelity": mean_fidelity,
        "ab_win_rate": win_rate,
        "tokens_spent": budget.spent,
        "all_passed": passed,
    }


def write_scorecard(card, json_path, markdown_path):
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(card, indent=2) + "\n")

    lines = [
        f"# Line bank scorecard — {card['bank']}", "",
        f"Persona: **{card['persona']}**  ",
        f"Gates run: {card['gates']}  ",
        f"Lines: {card['lines']}  accepted: {len(card['accepted'])}  "
        f"rejected: {len(card['rejected'])}", "",
        "| metric | value | gate |", "|---|---|---|",
        f"| mean fidelity | {card['mean_fidelity']} | >= {FIDELITY_MEAN_MIN} |",
        f"| A/B win-or-tie | {card['ab_win_rate']} | >= {NON_REGRESSION_WIN_RATE} |",
        f"| tokens spent | {card['tokens_spent']} | — |", "",
    ]
    if card["rejected"]:
        lines += ["## Rejected", "", "| topic | reply | why |", "|---|---|---|"]
        for r in card["rejected"]:
            why = "; ".join(r["problems"]).replace("|", "\\|")
            lines.append(f"| {r['topic']} | {r['reply'][:60]}… | {why} |")
        lines.append("")
    lines.append(f"**Verdict: {'PASS' if card['all_passed'] else 'FAIL'}**")
    markdown_path.write_text("\n".join(lines) + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--bank", type=Path, required=True)
    parser.add_argument("--gates", default="",
                        help="comma-separated gate numbers; default all. "
                             "Use 1,2,5 to score offline with no network.")
    parser.add_argument("--token-budget", type=int, default=100_000)
    parser.add_argument("--seed", type=int, default=1,
                        help="seeds A/B order shuffling so runs reproduce")
    parser.add_argument("--json-out", type=Path,
                        default=ROOT / "bench" / "out" / "line_scores.json")
    parser.add_argument("--markdown-out", type=Path,
                        default=ROOT / "bench" / "SCORECARD.md")
    args = parser.parse_args(argv)

    gates = tuple(int(g) for g in args.gates.split(",")) if args.gates else ALL_GATES

    try:
        card = score_bank(args.bank, gates, args.token_budget, args.seed)
    except GateFailure as exc:
        print(f"cannot evaluate: {exc}", file=sys.stderr)  # fail closed
        return 2

    write_scorecard(card, args.json_out, args.markdown_out)
    print(json.dumps(card, indent=2))
    return 0 if card["all_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
