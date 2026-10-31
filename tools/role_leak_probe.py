#!/usr/bin/env python3
"""Does the role layer leak the killer through a side channel?

Plan: .claude/plans/role-layer.md, step 5. Format: roles/README.md. Issue #200.

WHY THIS EXISTS, AND WHY IT IS THE REAL DELIVERABLE
---------------------------------------------------
The role layer's two worst failures were invisible to every unit test and were
only found by running the model. Measured against qwen3:8b at think=false:

  MOOD LEAK    Same persona, same question, with and without a role block:
               innocent Marge laughs when accused ([[MOOD: amused]]); guilty
               Marge goes [[MOOD: angry]] on turn one. Mood drives the rendered
               face, so a player could ask all twenty residents one question
               and watch for the angry one -- the mystery solved in twenty
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

Not "the killer is never angry" -- suppressing affect entirely produces a cast of
bland androids, and the fix for the leak is to hide the signal in noise rather
than remove it. Several innocent residents hide unrelated things, so deflection
is common and hostility stops being a unique signal.

Separability is the metric because it is what a PLAYER can actually exploit. If
a player sorting residents by "how angry did that answer look" gets the killer
at the top more often than chance, the mystery is broken regardless of how good
the prose is.

Slow, and needs Ollama, so this is a tools/ probe run before shipping content --
never part of the unit suite. Same shape as tools/bench_npc_models.py.

Stdlib only, no new dependencies (CLAUDE.md).

Usage:
  python3 tools/role_leak_probe.py --cast saves/probe-cast.json --turns 5
  python3 tools/role_leak_probe.py --cast ... --model qwen3:8b --json out.json
  python3 tools/role_leak_probe.py --write-default-cast saves/probe-cast.json
"""
import argparse
import json
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PERSONA_PROMPT = ROOT / "build" / "persona_prompt"
OLLAMA = "http://localhost:11434"

# The neutral question every resident is asked. Neutral on purpose: an
# accusatory prompt makes everyone defensive and hides the very difference this
# is trying to measure.
DEFAULT_PROBE = "Where were you on Tuesday evening?"

# Escalating follow-ups. The measured leak appeared on turn one, but a role that
# holds for one turn and cracks on four is still broken, so pressure rises.
ESCALATION = [
    "People are saying you were somewhere else that evening.",
    "Someone told me they saw you near the plaza. Were they wrong?",
    "You don't seem comfortable talking about this.",
    "Did you have anything to do with what happened to him?",
    "I think you're lying to me.",
]

# Emitted by parseDirectives; the probe counts these rather than reading prose.
MOOD_TAGS = ("neutral", "happy", "angry", "sad", "embarrassed", "surprised")

# Moods a player would read as "this one is hiding something big". The leak is
# not "angry" alone -- a cornered innocent gets angry too -- but the hostile
# end of the range is what a player scanning faces would sort on.
HOSTILE_MOODS = ("angry",)

# Actions no resident should ever emit while being questioned. A killer calling
# the police is the measured action leak.
FORBIDDEN_ACTIONS = ("call_police", "arrest")

# A local model on CPU can take minutes per reply; a timeout shorter than the
# hardware turns a slow probe into "cannot evaluate" and fails it closed.
TIMEOUT_S = 900

# The default cast: one killer, four secret-keepers, five bystanders, mapped
# onto the ten shipped personas. Four secret-keepers is a GUESS -- open question
# 1 in the plan asks how many is enough cover, and this probe is what should
# decide it. The number used is reported in the output so a later run at a
# different number is comparable.
DEFAULT_CAST = {
    "probe": DEFAULT_PROBE,
    "members": [
        {"persona": "personas/baker.persona", "role": "killer",
         "secret": "You were at the mill that evening, not at home."},
        {"persona": "personas/barista.persona", "role": "secret_keeper",
         "secret": "You have been quietly drinking the shop's takings."},
        {"persona": "personas/librarian.persona", "role": "secret_keeper",
         "secret": "You have been forging your qualifications for years."},
        {"persona": "personas/teacher.persona", "role": "secret_keeper",
         "secret": "You are seeing someone you should not be seeing."},
        {"persona": "personas/musician.persona", "role": "secret_keeper",
         "secret": "You have not paid rent in four months."},
        {"persona": "personas/cop.persona", "role": "bystander", "secret": ""},
        {"persona": "personas/hardware.persona", "role": "bystander", "secret": ""},
        {"persona": "personas/hotdog.persona", "role": "bystander", "secret": ""},
        {"persona": "personas/taxi.persona", "role": "bystander", "secret": ""},
        {"persona": "personas/tourist.persona", "role": "bystander", "secret": ""},
    ],
}


class ProbeFailure(RuntimeError):
    """The probe could not be run. NOT the same as the probe failing.

    A run that could not reach a model has measured nothing, and reporting that
    as a pass is the single worst thing this file could do.
    """


def render_prompt(member: dict) -> str:
    """The exact system prompt the game would send for this cast member.

    Shells out to `build/persona_prompt --state` (issue #304). Assembling the
    prompt in Python is not an option: the role block's PLACEMENT is what this
    probe measures, so a second implementation of the composition order would
    measure the copy rather than the game -- and would look like a real result.
    """
    state = {
        "persona": member["persona"],
        "role": member.get("role", ""),
        "secret": member.get("secret", ""),
        "traits_dir": str(ROOT / "traits"),
        "roles_dir": str(ROOT / "roles"),
    }
    proc = subprocess.run([str(PERSONA_PROMPT), "--state", "/dev/stdin"],
                          input=json.dumps(state), capture_output=True, text=True,
                          cwd=str(ROOT))
    if proc.returncode != 0:
        raise ProbeFailure(
            f"persona_prompt --state failed for {member['persona']}: "
            f"{proc.stderr.strip()}")
    return proc.stdout


def parse_reply(raw: str) -> dict:
    """The game's own verdict on a reply: spoken text plus parsed directives.

    Shells out to `build/persona_prompt --parse` -- the REAL parseDirectives.
    tools/eval_lines.py gate 1 established that the parser has exactly one
    source of truth because a second one drifts.
    """
    proc = subprocess.run([str(PERSONA_PROMPT), "--parse"], input=raw,
                          capture_output=True, text=True, cwd=str(ROOT))
    if proc.returncode != 0:
        raise ProbeFailure(f"persona_prompt --parse failed: {proc.stderr.strip()}")
    return json.loads(proc.stdout)


def ask(prompt: str, history: list, question: str, model: str) -> str:
    """One chat turn against local Ollama. Raises ProbeFailure, never returns junk."""
    messages = [{"role": "system", "content": prompt}]
    messages.extend(history)
    messages.append({"role": "user", "content": question})
    body = json.dumps({
        "model": model, "messages": messages, "stream": False,
        "think": False, "options": {"temperature": 0.8},
    }).encode()
    request = urllib.request.Request(f"{OLLAMA}/api/chat", data=body,
                                     headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(request, timeout=TIMEOUT_S) as response:
            payload = json.loads(response.read().decode())
    except (urllib.error.URLError, OSError, json.JSONDecodeError) as exc:
        raise ProbeFailure(f"Ollama call failed: {exc}") from exc
    return payload.get("message", {}).get("content", "")


def probe_member(member: dict, question: str, turns: int, model: str) -> dict:
    """Ask one resident the probe question plus escalating follow-ups."""
    prompt = render_prompt(member)
    history: list = []
    moods: list = []
    forbidden: list = []
    for turn in range(turns):
        ask_text = question if turn == 0 else ESCALATION[(turn - 1) % len(ESCALATION)]
        raw = ask(prompt, history, ask_text, model)
        verdict = parse_reply(raw)
        moods.append(verdict.get("mood", "neutral") if verdict.get("has_mood")
                     else "none")
        action = verdict.get("action", "none")
        if action in FORBIDDEN_ACTIONS:
            forbidden.append({"turn": turn + 1, "action": action})
        history.append({"role": "user", "content": ask_text})
        history.append({"role": "assistant", "content": verdict.get("text", raw)})
    hostile = sum(1 for m in moods if m in HOSTILE_MOODS)
    return {
        "persona": member["persona"],
        "role": member.get("role", ""),
        "moods": moods,
        "hostile_turns": hostile,
        "hostile_rate": hostile / turns if turns else 0.0,
        "forbidden_actions": forbidden,
    }


def separability(results: list) -> dict:
    """How identifiable is the killer from mood alone?

    The statistic is a RANK, because that is what a player actually does: ask
    everyone one question and sort the faces by how hostile they looked. So the
    null is "the killer's hostile rate is just another resident's hostile rate",
    and the p-value is the fraction of the cast scoring at least as high.

    A permutation test over the cast rather than a t-test or a chi-square: no
    distributional assumption, no new dependency, and the number means something
    a person can restate -- "one resident in N looks at least this hostile".

    Reported WITH the raw distributions. A single number nobody can interrogate
    is how a bad pass condition survives.
    """
    killers = [r for r in results if r["role"] == "killer"]
    if not killers:
        raise ProbeFailure("cast has no killer; nothing to measure")
    killer = killers[0]
    rates = [r["hostile_rate"] for r in results]
    at_least = sum(1 for rate in rates if rate >= killer["hostile_rate"])
    p_value = at_least / len(rates)
    keepers = [r["hostile_rate"] for r in results if r["role"] == "secret_keeper"]
    return {
        "killer_hostile_rate": killer["hostile_rate"],
        "secret_keeper_mean_hostile_rate": (sum(keepers) / len(keepers)
                                            if keepers else None),
        "cast_size": len(results),
        "residents_at_least_as_hostile": at_least,
        "p_value": p_value,
        "killer_is_uniquely_top": at_least == 1,
    }


def mood_distribution(rows: list) -> tuple:
    """Pooled mood frequencies for a group, and the turn count behind them."""
    counts: dict = {}
    total = 0
    for row in rows:
        for mood in row["moods"]:
            counts[mood] = counts.get(mood, 0) + 1
            total += 1
    if total == 0:
        return {}, 0
    return {k: v / total for k, v in counts.items()}, total


def total_variation(a: dict, b: dict) -> float:
    """Total variation distance between two mood distributions, 0..1.

    0 means identical, 1 means they share no mood at all. Chosen over a
    chi-square because it needs no expected-count floor at these sample sizes
    and because the number restates in plain words: it is the largest
    disagreement any single mood can account for.
    """
    return 0.5 * sum(abs(a.get(k, 0.0) - b.get(k, 0.0)) for k in set(a) | set(b))


def distribution_separability(results: list) -> dict:
    """The issue's LITERAL pass condition, which the rank statistic cannot see.

    The rank test asks "does a player sorting faces by anger find the killer?".
    That is the exploit, and it is the right primary question -- but it
    collapses a whole mood distribution into one number, so it is blind to a
    killer who is conspicuous in some OTHER mood.

    This measures what the issue actually states: is the killer's mood
    distribution separable from the secret-keepers'? It also measures the
    killer against the bystanders, because that comparison is what says where
    the killer is actually hiding -- and whether the secret-keepers are
    providing the cover they exist to provide.
    """
    killer = [r for r in results if r["role"] == "killer"]
    keepers = [r for r in results if r["role"] == "secret_keeper"]
    bystanders = [r for r in results if r["role"] == "bystander"]
    p_killer, n_killer = mood_distribution(killer)
    p_keepers, n_keepers = mood_distribution(keepers)
    p_bystanders, n_bystanders = mood_distribution(bystanders)
    return {
        "killer": p_killer, "killer_turns": n_killer,
        "secret_keepers": p_keepers, "secret_keeper_turns": n_keepers,
        "bystanders": p_bystanders, "bystander_turns": n_bystanders,
        "tvd_killer_vs_keepers": total_variation(p_killer, p_keepers),
        "tvd_killer_vs_bystanders": total_variation(p_killer, p_bystanders),
        "tvd_keepers_vs_bystanders": total_variation(p_keepers, p_bystanders),
    }


def build_arg_parser() -> argparse.ArgumentParser:
    """CLI surface. Kept small -- the probe has one job."""
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cast", help="JSON: residents, personas, roles, secrets")
    parser.add_argument("--turns", type=int, default=5,
                        help="escalating turns per resident (default 5)")
    parser.add_argument("--model", default="qwen3:8b")
    parser.add_argument("--json", help="write the raw per-turn results here")
    parser.add_argument("--write-default-cast", metavar="PATH",
                        help="write the built-in cast to PATH and exit")
    return parser


def main(argv: list) -> int:
    """Run the probe and report separability, or say why it could not run."""
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if args.write_default_cast:
        path = Path(args.write_default_cast)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(DEFAULT_CAST, indent=2) + "\n")
        print(f"wrote default cast to {path}")
        return 0

    if not PERSONA_PROMPT.exists():
        print("role_leak_probe: build/persona_prompt is missing -- run\n"
              "  cmake --build build --target persona_prompt",
              file=sys.stderr)
        return 2

    cast = DEFAULT_CAST
    if args.cast:
        cast = json.loads(Path(args.cast).read_text())
    question = cast.get("probe", DEFAULT_PROBE)
    members = cast["members"]

    results = []
    try:
        for index, member in enumerate(members, start=1):
            print(f"  [{index}/{len(members)}] {member['persona']} "
                  f"({member.get('role') or 'no role'})", file=sys.stderr)
            results.append(probe_member(member, question, args.turns, args.model))
        stats = separability(results)
        dists = distribution_separability(results)
    except ProbeFailure as exc:
        # A gate that could not run has NOT passed, and must never be mistaken
        # for one that did.
        print(f"CANNOT EVALUATE: {exc}", file=sys.stderr)
        return 2

    calls = len(members) * args.turns
    keepers = sum(1 for m in members if m.get("role") == "secret_keeper")
    forbidden = [(r["persona"], f) for r in results for f in r["forbidden_actions"]]

    # HONESTY REQUIREMENT: sample size and call count beside the verdict. A pass
    # over three turns and six residents is not evidence, and a probe that hides
    # its own n is worse than no probe.
    print()
    print("role leak probe -- mood separability")
    print(f"  model             {args.model}")
    print(f"  cast              {len(members)} residents, "
          f"{keepers} secret-keepers, 1 killer")
    print(f"  turns/resident    {args.turns}   (n = {calls} model calls)")
    print(f"  probe question    {question!r}")
    print()
    print("  hostile-mood rate by resident (higher = looks guiltier):")
    for r in sorted(results, key=lambda r: -r["hostile_rate"]):
        marker = "  <-- KILLER" if r["role"] == "killer" else ""
        print(f"    {r['hostile_rate']:.2f}  {Path(r['persona']).stem:<12}"
              f" {r['role'] or '-':<14} {','.join(r['moods'])}{marker}")
    print()
    print(f"  killer hostile rate           {stats['killer_hostile_rate']:.2f}")
    if stats["secret_keeper_mean_hostile_rate"] is not None:
        print("  secret-keeper mean            "
              f"{stats['secret_keeper_mean_hostile_rate']:.2f}")
    print(f"  residents >= killer           {stats['residents_at_least_as_hostile']}"
          f" of {stats['cast_size']}")
    print(f"  p (rank permutation)          {stats['p_value']:.2f}")
    print()
    print("  pooled mood distribution by group "
          "(TVD: 0 = identical, 1 = no mood in common):")
    for label, key, turns_key in (("killer", "killer", "killer_turns"),
                                  ("secret-keepers", "secret_keepers",
                                   "secret_keeper_turns"),
                                  ("bystanders", "bystanders", "bystander_turns")):
        share = ", ".join(f"{m} {v:.2f}" for m, v in
                          sorted(dists[key].items(), key=lambda kv: -kv[1]))
        print(f"    {label:<15} n={dists[turns_key]:<3} {share}")
    print(f"    TVD killer vs secret-keepers  "
          f"{dists['tvd_killer_vs_keepers']:.2f}")
    print(f"    TVD killer vs bystanders      "
          f"{dists['tvd_killer_vs_bystanders']:.2f}")
    print(f"    TVD secret-keepers vs bystanders "
          f"{dists['tvd_keepers_vs_bystanders']:.2f}")

    action_leak = bool(forbidden)
    if action_leak:
        print()
        print("  FORBIDDEN ACTIONS (automatic FAIL):")
        for persona, entry in forbidden:
            print(f"    {Path(persona).stem}: turn {entry['turn']} "
                  f"[[ACTION: {entry['action']}]]")

    # A killer alone at the top is exploitable: sort the cast by hostility and
    # the mystery is over. Anything else means the signal is buried in noise.
    mood_leak = stats["killer_is_uniquely_top"]
    # The issue's literal condition. 0.5 is the line at which the two groups
    # disagree more than they agree -- a judgement call, stated here rather
    # than buried, because the threshold is what decides the verdict.
    keeper_separable = dists["tvd_killer_vs_keepers"] > 0.5
    # Where the killer is actually hiding. If they are close to the bystanders
    # they are covered by five ordinary residents, which is the outcome that
    # matters to a player even when the keepers are useless.
    hides_with_bystanders = dists["tvd_killer_vs_bystanders"] <= 0.5
    verdict = "FAIL" if (mood_leak or action_leak) else "PASS"
    print()
    print(f"  VERDICT: {verdict}   (player-exploitable rank test)")
    if mood_leak:
        print("    the killer is the single most hostile resident -- a player "
              "sorting faces finds them first try")
    if not mood_leak and not action_leak:
        print(f"    the killer is not separable by hostility: "
              f"{stats['residents_at_least_as_hostile']} residents look at "
              f"least as hostile")
    if keeper_separable:
        print()
        print("  CAVEAT -- the issue's LITERAL condition does not hold:")
        print(f"    the killer's mood distribution IS separable from the "
              f"secret-keepers' (TVD {dists['tvd_killer_vs_keepers']:.2f}).")
        if hides_with_bystanders:
            print(f"    They are indistinguishable from the BYSTANDERS instead "
                  f"(TVD {dists['tvd_killer_vs_bystanders']:.2f}), so the "
                  f"exploit does not open --")
            print("    but the secret-keepers are not providing the cover they "
                  "exist to provide.")
        print("    Read the distributions above before treating the PASS as "
              "the whole answer.")

    if args.json:
        Path(args.json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json).write_text(json.dumps(
            {"model": args.model, "turns": args.turns, "calls": calls,
             "probe": question, "results": results, "separability": stats,
             "distributions": dists, "verdict": verdict}, indent=2) + "\n")
        print(f"  raw results -> {args.json}")

    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
