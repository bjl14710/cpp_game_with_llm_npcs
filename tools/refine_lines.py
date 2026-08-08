#!/usr/bin/env python3
"""Nightly job: grow the NPC line banks from the day's real play, then PR them.

Plan: .claude/plans/npc-line-bank.md, issue #152. Format: banks/README.md.

Pipeline:

  1 read      saves/conversations.sqlite3 transcripts + saves/ratings/*.jsonl
  2 cluster   group near-duplicate player lines into candidate topics
  3 diff      drop anything the existing banks already cover
  4 author    Claude writes 3-5 reply variants + 8-12 trigger phrasings per gap
  5 score     every candidate through tools/eval_lines.py — all five gates
  6 write     regenerate banks/*.bank with the survivors
  7 verify    make -C tests test
  8 PR        draft PR against dev, scorecard in the body. NEVER merges.

FAILS CLOSED. Ollama down, judge key missing, token budget exhausted, or tests
failing: restore the working tree, open no PR, exit non-zero. A half-scored
bank must never land — once a line sits in a .bank file nothing distinguishes
"scored and passed" from "never scored".

Stdlib + PyGithub only — no new dependencies (CLAUDE.md).

Usage:
  python3 tools/refine_lines.py --dry-run          # steps 1-5, writes nothing
  python3 tools/refine_lines.py                    # full run, opens a draft PR
  python3 tools/refine_lines.py --max-topics 5     # smaller review
"""
import argparse
import json
import os
import re
import sqlite3
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import eval_lines  # noqa: E402  — scoring lives in exactly one place

ROOT = Path(__file__).resolve().parent.parent
BANKS = ROOT / "banks"
SAVES = ROOT / "saves"

# A nightly diff nobody reviews is worse than no nightly diff, and an unbounded
# token spend on an unattended job is how you get a surprise bill.
DEFAULT_MAX_NEW_TOPICS = 10
DEFAULT_TOKEN_BUDGET = 100_000

# Base branch for the nightly PR. Never main — repo rule.
PR_BASE = "dev"

# Two player lines this similar are the same topic. Deliberately higher than
# the runtime match threshold (0.62): clustering should be conservative, since
# a wrongly merged topic produces replies that answer the wrong question.
CLUSTER_THRESHOLD = 0.78

# A topic needs to come up this often before it earns a bank entry. Banking a
# one-off is how a bank fills with noise.
MIN_OCCURRENCES = 2


class AbortRun(Exception):
    """Stop before anything is written. The only way this job fails."""


# --- step 1: read the day's play --------------------------------------------

def read_transcripts():
    """Player lines per NPC from ConversationStore, plus the ratings log.

    ConversationStore stores one row per NPC: (npc_id, summary,
    transcript_json, updated_at); the transcript is a JSON array of
    {role, content} and player lines are role == "user".
    """
    lines = defaultdict(list)  # npc_id -> [player line, ...]

    db = SAVES / "conversations.sqlite3"
    if db.exists():
        try:
            con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
            for npc_id, raw in con.execute(
                    "SELECT npc_id, transcript_json FROM conversations"):
                try:
                    for turn in json.loads(raw or "[]"):
                        if turn.get("role") == "user" and turn.get("content"):
                            lines[npc_id].append(turn["content"])
                except (json.JSONDecodeError, TypeError):
                    continue  # a corrupt row is skipped, never fatal
            con.close()
        except sqlite3.Error as exc:
            raise AbortRun(f"cannot read {db}: {exc}") from exc

    # Thumbs-up exchanges are a strong signal for what to bank; thumbs-down for
    # what to avoid. RatingLog's contract is unchanged — this reads the log as
    # INPUT SIGNAL; the human still approves by merging the PR.
    liked = []
    ratings = SAVES / "ratings" / "candidates.jsonl"
    if ratings.exists():
        for row in ratings.read_text().splitlines():
            try:
                entry = json.loads(row)
                if entry.get("player"):
                    liked.append((entry.get("persona", ""), entry["player"]))
            except json.JSONDecodeError:
                continue
    return lines, liked


# --- step 2: cluster --------------------------------------------------------

def persona_files_by_name():
    """Map a persona's display NAME to its file stem.

    ConversationStore keys rows by Persona::name ("Marge Holloway"), while
    banks and persona files are named by stem ("baker"). Without this mapping
    every NPC looks like it has no persona file. Characters with no file at all
    — generated `gen_*` villagers, sandbox test NPCs — are correctly absent and
    get skipped.
    """
    by_name = {}
    for path in sorted((ROOT / "personas").glob("*.persona")):
        name = eval_lines.parse_persona(path).get("name")
        if name:
            by_name[name] = path.stem
    return by_name


def cluster(player_lines):
    """Group near-duplicate player lines into candidate topics.

    Reuses the game's own normalisation and similarity through the C++ so
    clusters and runtime matching cannot disagree — a topic the clusterer
    invents but the matcher can never reach is dead weight in the bank.
    """
    clusters = []  # [{"canonical": str, "members": [str, ...]}]
    for line in player_lines:
        norm = _normalise(line)
        if not norm:
            continue
        for group in clusters:
            if _similarity(norm, group["norm"]) >= CLUSTER_THRESHOLD:
                group["members"].append(line)
                break
        else:
            clusters.append({"canonical": line, "norm": norm, "members": [line]})
    return [c for c in clusters if len(c["members"]) >= MIN_OCCURRENCES]


# The matcher is C++ and has no CLI of its own yet. Rather than write a second
# scorer in Python — the exact duplication eval_lines avoids for parseDirectives
# — clustering uses the same blended shape: token-set Jaccard averaged with
# character-trigram Jaccard over the same normalisation.
#
# TODO(linebank): when a `topic_match` CLI exists beside persona_prompt, call it
# here and delete these two functions. Tracked as a follow-up, not a silent gap.
_STOPWORDS = {
    "a", "an", "and", "are", "as", "at", "be", "been", "but", "by", "can",
    "could", "did", "do", "does", "for", "from", "had", "has", "have", "i",
    "if", "in", "is", "it", "its", "me", "my", "of", "on", "or", "that", "the",
    "this", "to", "was", "were", "will", "with", "would", "what", "you", "your",
}


def _normalise(raw):
    folded = "".join(c.lower() if c.isalnum() or ord(c) >= 128 else " " for c in raw)
    return " ".join(w for w in folded.split() if w not in _STOPWORDS)


def _similarity(a, b):
    if not a or not b:
        return 0.0
    ta, tb = set(a.split()), set(b.split())
    tok = len(ta & tb) / len(ta | tb) if (ta | tb) else 0.0
    ga = {a[i:i + 3] for i in range(max(len(a) - 2, 1))}
    gb = {b[i:i + 3] for i in range(max(len(b) - 2, 1))}
    tri = len(ga & gb) / len(ga | gb) if (ga | gb) else 0.0
    return 0.5 * tok + 0.5 * tri


# --- step 3: diff against existing coverage ---------------------------------

def existing_coverage(stem):
    """Triggers already banked for one persona, normalised for comparison."""
    path = BANKS / f"{stem}.bank"
    if not path.exists():
        return set()
    bank = eval_lines.parse_bank(path)
    return {_normalise(t) for topic in bank["topics"] for t in topic["triggers"]}


def uncovered(clusters, covered):
    """Clusters no existing trigger already reaches."""
    gaps = []
    for group in clusters:
        if not any(_similarity(group["norm"], c) >= CLUSTER_THRESHOLD for c in covered):
            gaps.append(group)
    return gaps


# --- step 4: author ---------------------------------------------------------

AUTHOR_PROMPT = """You are writing cached dialogue for a game character.

Here is the character's complete sheet — voice, personality, knowledge limits:

{system_prompt}

Players keep asking this character variations of:
{examples}

Write JSON only, no prose, in exactly this shape:
{{"topic_id": "snake_case_id",
  "triggers": ["8 to 12 different ways a player might phrase this"],
  "replies": ["3 to 5 in-character answers"]}}

Rules for the replies:
- Match the character sheet's voice exactly. This is the whole point.
- Each must end with exactly one mood tag: [[MOOD: neutral]], [[MOOD: happy]],
  [[MOOD: angry]], [[MOOD: sad]], [[MOOD: embarrassed]] or [[MOOD: surprised]].
- Under 140 characters each.
- Never reference having met the player before — these are memory-agnostic.
- Never mention being an AI, a model, or a game.
- Stay inside the character's stated knowledge limits.

Rules for the triggers: short, lowercase, no punctuation, genuinely different
phrasings. Trigger breadth is what makes the cache hit at all."""


def author_topic(system_prompt, examples, budget):
    """Claude writes variants and triggers for one uncovered topic."""
    raw = eval_lines.judge(
        AUTHOR_PROMPT.format(system_prompt=system_prompt,
                             examples="\n".join(f"- {e}" for e in examples[:8])),
        budget)
    match = re.search(r"\{.*\}", raw, re.DOTALL)
    if not match:
        raise AbortRun(f"author returned no JSON object: {raw[:120]!r}")
    try:
        topic = json.loads(match.group())
    except json.JSONDecodeError as exc:
        raise AbortRun(f"author returned malformed JSON: {exc}") from exc
    for field in ("topic_id", "triggers", "replies"):
        if not topic.get(field):
            raise AbortRun(f"author omitted `{field}`")
    return topic


# --- step 6: write ----------------------------------------------------------

def render_bank(persona_name, topics):
    """Render a .bank file. Stable ordering — topics sorted by id, replies kept
    in authored order — so an unchanged night produces an EMPTY diff. A job that
    churns the file every night trains you to stop reading it.
    """
    out = [f"# Generated by tools/refine_lines.py — human-approved via PR.",
           f"# Format: banks/README.md",
           f"persona = {persona_name}", ""]
    for topic in sorted(topics, key=lambda t: t["topic_id"]):
        out.append(f"topic = {topic['topic_id']}")
        if topic.get("familiarity", "any") != "any":
            out.append(f"  familiarity = {topic['familiarity']}")
        for trigger in topic["triggers"]:
            out.append(f"  trigger = {trigger}")
        for reply in topic["replies"]:
            out.append(f"  reply = {reply}")
        out.append("")
    return "\n".join(out)


# --- step 7: verify ---------------------------------------------------------

def run_tests():
    result = subprocess.run(["make", "-C", "tests", "test"],
                            cwd=ROOT, capture_output=True, text=True)
    if result.returncode != 0:
        raise AbortRun("test suite failed:\n" + result.stdout[-2000:])


# --- step 8: PR -------------------------------------------------------------

def open_draft_pr(branch, body):
    """Draft PR against dev via PyGithub. Never merges, never touches main."""
    try:
        from github import Github
    except ImportError as exc:
        raise AbortRun("PyGithub not installed") from exc
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if not token:
        raise AbortRun("no GH_TOKEN")

    url = subprocess.run(["git", "remote", "get-url", "origin"], cwd=ROOT,
                         capture_output=True, text=True, check=True).stdout.strip()
    slug = (url.replace("git@github.com:", "").replace("https://github.com/", "")
               .rstrip("/").removesuffix(".git"))
    repo = Github(token).get_repo(slug)
    pr = repo.create_pull(title=f"content(linebank): nightly refinement {branch}",
                          body=body, base=PR_BASE, head=branch, draft=True)
    return pr.html_url


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                          text=True, check=True).stdout.strip()


# --- driver -----------------------------------------------------------------

def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dry-run", action="store_true",
                        help="steps 1-5 only: report what would change, write nothing")
    parser.add_argument("--max-topics", type=int, default=DEFAULT_MAX_NEW_TOPICS,
                        help=f"cap new topics per run (default {DEFAULT_MAX_NEW_TOPICS}); "
                             "what gets deferred is logged, never silently dropped")
    parser.add_argument("--token-budget", type=int, default=DEFAULT_TOKEN_BUDGET)
    args = parser.parse_args(argv)

    try:
        transcripts, liked = read_transcripts()
        if not transcripts and not liked:
            # A quiet day is a success, not a failure.
            print("no new transcripts — nothing to refine")
            return 0

        budget = eval_lines.TokenBudget(args.token_budget)
        authored, deferred = defaultdict(list), 0

        by_name = persona_files_by_name()
        read_count = sum(len(v) for v in transcripts.values())
        cluster_count = 0
        print(f"read {read_count} player line(s) across "
              f"{len(transcripts)} character(s)")

        for npc_name, lines in sorted(transcripts.items()):
            stem = by_name.get(npc_name)
            if stem is None:
                # Generated villagers and sandbox test NPCs have no persona
                # file; they are not part of the shipped cast and get no bank.
                print(f"  skip {npc_name!r}: not in personas/")
                continue
            persona_file = ROOT / "personas" / f"{stem}.persona"

            clusters = cluster(lines)
            cluster_count += len(clusters)
            gaps = uncovered(clusters, existing_coverage(stem))
            if not gaps:
                continue
            print(f"  {npc_name} ({stem}): {len(gaps)} uncovered topic(s)")

            for group in gaps:
                if sum(len(v) for v in authored.values()) >= args.max_topics:
                    deferred += 1
                    continue
                if args.dry_run:
                    authored[stem].append({"topic_id": "(dry-run)",
                                           "triggers": [], "replies": [],
                                           "examples": group["members"][:3]})
                    continue
                system_prompt = eval_lines._persona_prompt(persona_file)
                authored[stem].append(
                    author_topic(system_prompt, group["members"], budget))

        total = sum(len(v) for v in authored.values())
        if deferred:
            print(f"  deferred {deferred} topic(s) over the --max-topics cap")
        if total == 0:
            # Be precise about WHY there is nothing to do. "Already covered" and
            # "nothing recurred" are very different states, and conflating them
            # hides an empty corpus behind a reassuring message.
            if cluster_count == 0:
                print(f"no topic recurred at least {MIN_OCCURRENCES}x — nothing "
                      f"to bank. The corpus is still too thin; seed topics by "
                      f"hand (issue #153) rather than waiting on play data.")
            else:
                print(f"all {cluster_count} recurring topic(s) already covered "
                      f"by existing banks — no PR")
            return 0

        if args.dry_run:
            print(f"\ndry run: would author {total} topic(s)")
            for npc_id, topics in authored.items():
                for t in topics:
                    print(f"  {npc_id}: {t['examples']}")
            return 0

        # --- step 6: write ---------------------------------------------------
        # Every touched bank is snapshotted first so any later failure can put
        # the working tree back exactly as it was.
        touched, snapshots = [], {}
        for npc_id, new_topics in authored.items():
            path = BANKS / f"{npc_id}.bank"
            snapshots[path] = path.read_text() if path.exists() else None

            existing = eval_lines.parse_bank(path) if path.exists() else None
            persona_name = (existing["persona"] if existing else
                            eval_lines.parse_persona(
                                ROOT / "personas" / f"{npc_id}.persona")["name"])
            merged = [{"topic_id": t["id"], "familiarity": t["familiarity"],
                       "triggers": t["triggers"], "replies": t["replies"]}
                      for t in (existing["topics"] if existing else [])]
            known = {t["topic_id"] for t in merged}
            merged += [t for t in new_topics if t["topic_id"] not in known]

            BANKS.mkdir(parents=True, exist_ok=True)
            path.write_text(render_bank(persona_name, merged))
            touched.append(path)

        def restore():
            for path, before in snapshots.items():
                if before is None:
                    path.unlink(missing_ok=True)
                else:
                    path.write_text(before)

        # --- step 5 (continued): score what was just written -----------------
        # Scoring happens against the real file so it measures exactly what the
        # game would load, not an in-memory approximation of it.
        try:
            cards = []
            for path in touched:
                card = eval_lines.score_bank(path, token_budget=args.token_budget)
                cards.append(card)
                if not card["all_passed"]:
                    raise AbortRun(
                        f"{path.name} failed the rubric: "
                        f"{len(card['rejected'])} line(s) rejected")

            # --- step 7: verify ----------------------------------------------
            run_tests()
        except Exception:
            restore()
            raise

        # --- step 8: commit, push, draft PR ----------------------------------
        head = git("rev-parse", "--abbrev-ref", "HEAD")
        branch = f"linebank/nightly-{git('rev-parse', '--short', 'HEAD')}"
        try:
            git("checkout", "-b", branch)
            # Explicit paths only — never `git add .`. saves/, docs/learning/
            # and .claude/ must stay local.
            git("add", *[str(p.relative_to(ROOT)) for p in touched],
                "bench/SCORECARD.md")
            git("commit", "-m",
                f"content(linebank): {total} new topic(s) from nightly refinement")
            git("push", "-u", "origin", branch)
        except subprocess.CalledProcessError as exc:
            git("checkout", head)
            restore()
            raise AbortRun(f"git failed, nothing pushed: {exc.stderr or exc}") from exc

        body = _pr_body(cards, total, deferred)
        url = open_draft_pr(branch, body)
        git("checkout", head)
        print(f"draft PR opened: {url}")
        return 0

    except (AbortRun, eval_lines.GateFailure) as exc:
        print(f"aborted before writing: {exc}", file=sys.stderr)
        return 1


def _pr_body(cards, total, deferred):
    """The review surface. Numbers first, so review is 'read this, read the
    diff' rather than 'go run something'."""
    lines = [f"Nightly line-bank refinement: **{total} new topic(s)**.", ""]
    if deferred:
        lines += [f"> {deferred} further topic(s) were deferred by the "
                  f"`--max-topics` cap and will be offered again tomorrow.", ""]
    lines += ["| bank | lines | accepted | mean fidelity | A/B win-or-tie |",
              "|---|---|---|---|---|"]
    for c in cards:
        lines.append(f"| `{c['bank']}` | {c['lines']} | {len(c['accepted'])} | "
                     f"{c['mean_fidelity']} | {c['ab_win_rate']} |")
    lines += ["",
              "Every line passed all five gates in `tools/eval_lines.py`, "
              "including a blind A/B against a live `qwen3:8b` answer to the "
              "same player line. Full scorecard in `bench/SCORECARD.md`.", "",
              "Draft on purpose — nothing here reaches a player until a human "
              "merges it."]
    return "\n".join(lines)


if __name__ == "__main__":
    sys.exit(main())
