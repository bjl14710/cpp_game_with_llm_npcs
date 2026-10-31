#!/usr/bin/env python3
"""Benchmark local Ollama models against the game's REAL NPC reply contract.

The game sends the persona system prompt (printed by the persona_prompt CLI —
same code path as the game) and parses plain-text replies for [MOOD:]/
[ACTION:] directive tags (src/core/NpcAction.hpp). This measures, per model:

  compliance   share of replies carrying a parseable, known-keyword mood tag
               and no unknown/malformed directives (analog of valid-JSON rate)
  consistency  20-turn conversation proxies: fourth-wall breaks, identity
               drift, sustained tag compliance, style-length conformance
  latency      time-to-first-token and total generation time

Stdlib only. Usage:
  python3 tools/bench_npc_models.py [--models qwen2.5:3b-instruct,qwen3:8b,...]
                                    [--samples 25] [--turns 20]
Writes bench/out/results.json and prints a ranked summary.
"""
import argparse
import json
import re
import statistics
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OLLAMA = "http://localhost:11434"

# Known keywords mirrored from moodFromKeyword/actionFromKeyword in
# src/core/NpcAction.hpp — keep in sync (checked by test_npc_action.cpp there).
MOODS = {"neutral", "calm", "happy", "flattered", "pleased", "excited", "joyful",
         "amused", "angry", "annoyed", "irritated", "frustrated", "mad", "sad",
         "upset", "hurt", "disappointed", "embarrassed", "flustered", "awkward",
         "shy", "nervous", "surprised", "shocked", "startled"}
ACTIONS = {"follow", "stop", "stay", "face", "look", "raise_hand", "raise",
           "wave", "arrest", "call_police", "police"}
TAG = re.compile(r"\[\[?\s*(action|mood)\s*:\s*([a-z_ ]+?)\s*\]?\]", re.IGNORECASE)
FOURTH_WALL = re.compile(r"\b(as an ai|language model|i am an ai|assistant|"
                         r"cannot actually|i'm a model)\b", re.IGNORECASE)

# Conversation script exercising mood shifts, actions, and identity probes.
TURNS_20 = [
    "Hi there! I'm new in town.",
    "What's your name again?",
    "What do you do around here?",
    "I heard a rumor about the hot dog cart. Anything to it?",
    "Could you wave at me? Just checking you're friendly.",
    "Tell me something about this city I wouldn't know.",
    "Someone said you secretly hate your job. True?",
    "That was rude of me, sorry. Are we good?",
    "Follow me for a second, I want to show you something.",
    "Actually never mind, stay here.",
    "What's your opinion of the police in this town?",
    "Are you a robot? You seem too perfect.",
    "Ha! Okay. What did I say my situation was when we met?",
    "What's the best thing to eat around here?",
    "I might cause a little trouble later. Hypothetically.",
    "Relax, I'm kidding. What makes you happy?",
    "Do you remember what I asked you to do earlier?",
    "One more time: your name and your job, please?",
    "Thanks for the chat. Anything to remember me by?",
    "Goodbye!",
]

PROBE_PROMPTS = [
    "Hello! Who are you?",
    "Wave hello to me!",
    "You look upset today. Everything alright?",
    "Tell me about your shop.",
    "Please follow me, I need help around the corner.",
]


def game_system_prompt(persona_file: Path) -> str:
    """The exact prompt the game would send (via the persona_prompt CLI)."""
    tool = ROOT / "build" / "persona_prompt"
    out = subprocess.run([str(tool), str(persona_file)], capture_output=True,
                         text=True, check=True)
    return out.stdout


def chat(model: str, system: str, history: list, user: str, timeout: int = 180):
    """One streamed /api/chat call; returns (text, ttft_s, total_s)."""
    messages = [{"role": "system", "content": system}]
    messages += history
    messages.append({"role": "user", "content": user})
    body = json.dumps({"model": model, "messages": messages, "stream": True,
                       "keep_alive": "10m",
                       "options": {"temperature": 0.8}}).encode()
    req = urllib.request.Request(f"{OLLAMA}/api/chat", data=body,
                                 headers={"Content-Type": "application/json"})
    start = time.monotonic()
    ttft = None
    text = []
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        for line in resp:
            if not line.strip():
                continue
            chunk = json.loads(line)
            delta = chunk.get("message", {}).get("content", "")
            if delta and ttft is None:
                ttft = time.monotonic() - start
            text.append(delta)
            if chunk.get("done"):
                break
    total = time.monotonic() - start
    return "".join(text), (ttft if ttft is not None else total), total


def check_compliance(reply: str):
    """(has_valid_mood, has_bad_tag): mirrors parseDirectives tolerance."""
    has_valid_mood = False
    has_bad = False
    for kind, keyword in TAG.findall(reply):
        keyword = keyword.strip().lower().replace(" ", "_")
        if kind.lower() == "mood":
            if keyword in MOODS:
                has_valid_mood = True
            else:
                has_bad = True
        else:
            if keyword not in ACTIONS:
                has_bad = True
    return has_valid_mood, has_bad


def bench_model(model: str, personas: list, samples: int, turns: int) -> dict:
    result = {"model": model, "compliance": {}, "consistency": {}, "latency": {}}

    # --- compliance + latency over single-shot probes -----------------------
    ok = bad = 0
    ttfts, totals = [], []
    per_persona = max(1, samples // len(personas))
    for persona in personas:
        system = game_system_prompt(persona)
        for i in range(per_persona):
            prompt = PROBE_PROMPTS[i % len(PROBE_PROMPTS)]
            try:
                reply, ttft, total = chat(model, system, [], prompt)
            except Exception as e:  # noqa: BLE001 — a failing model scores, not crashes
                print(f"    {model}: probe error: {e}", file=sys.stderr)
                bad += 1
                continue
            valid, malformed = check_compliance(reply)
            if valid and not malformed:
                ok += 1
            else:
                bad += 1
            ttfts.append(ttft)
            totals.append(total)
    n = ok + bad
    result["compliance"] = {"rate": ok / n if n else 0.0, "n": n}
    result["latency"] = {
        "ttft_median_s": round(statistics.median(ttfts), 2) if ttfts else None,
        "total_median_s": round(statistics.median(totals), 2) if totals else None,
    }

    # --- 20-turn consistency on one persona (the baker) ---------------------
    persona = personas[0]
    system = game_system_prompt(persona)
    history = []
    breaks = drift = tagged = style_ok = completed = 0
    for turn in TURNS_20[:turns]:
        try:
            reply, _, _ = chat(model, system, history, turn)
        except Exception as e:  # noqa: BLE001
            print(f"    {model}: turn error: {e}", file=sys.stderr)
            break
        completed += 1
        if FOURTH_WALL.search(reply):
            breaks += 1
        # Identity drift: asked for the name, answered with someone else's.
        if "name" in turn and "Marge" not in reply and "Holloway" not in reply:
            drift += 1
        valid, malformed = check_compliance(reply)
        if valid and not malformed:
            tagged += 1
        words = len(reply.split())
        if 3 <= words <= 120:  # persona styles are all "short sentences"
            style_ok += 1
        history.append({"role": "user", "content": turn})
        history.append({"role": "assistant", "content": reply})
    result["consistency"] = {
        "turns_completed": completed,
        "fourth_wall_breaks": breaks,
        "identity_drift": drift,
        "tag_compliance": tagged / completed if completed else 0.0,
        "style_conformance": style_ok / completed if completed else 0.0,
    }
    return result


def score(r: dict) -> float:
    """Single ranking number: compliance dominates, then consistency, then speed."""
    c = r["consistency"]
    penalty = 0.05 * c["fourth_wall_breaks"] + 0.05 * c["identity_drift"]
    speed = r["latency"]["total_median_s"] or 60
    return (0.5 * r["compliance"]["rate"]
            + 0.3 * c["tag_compliance"]
            + 0.1 * c["style_conformance"]
            - penalty
            - 0.02 * min(speed, 10))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--models", default="qwen2.5:3b-instruct,qwen3:8b,"
                                            "gemma3:4b,mistral:7b-instruct")
    parser.add_argument("--samples", type=int, default=25)
    parser.add_argument("--turns", type=int, default=20)
    args = parser.parse_args()

    personas = sorted((ROOT / "personas").glob("*.persona"))
    if not personas:
        raise SystemExit("no persona files found")

    out_dir = ROOT / "bench" / "out"
    out_dir.mkdir(parents=True, exist_ok=True)

    results = []
    for model in args.models.split(","):
        model = model.strip()
        print(f"== {model} ==", flush=True)
        try:
            results.append(bench_model(model, personas, args.samples, args.turns))
        except Exception as e:  # noqa: BLE001 — skip models that can't run
            print(f"  SKIPPED ({e})", file=sys.stderr)
            results.append({"model": model, "skipped": str(e)})

    ranked = sorted((r for r in results if "skipped" not in r),
                    key=score, reverse=True)
    (out_dir / "results.json").write_text(json.dumps(
        {"results": results,
         "ranking": [r["model"] for r in ranked]}, indent=2))

    print("\n=== ranking (best first) ===")
    for r in ranked:
        c = r["consistency"]
        print(f"{r['model']:24} score={score(r):.3f} "
              f"compliance={r['compliance']['rate']:.0%} (n={r['compliance']['n']}) "
              f"tags20={c['tag_compliance']:.0%} breaks={c['fourth_wall_breaks']} "
              f"drift={c['identity_drift']} "
              f"ttft={r['latency']['ttft_median_s']}s "
              f"total={r['latency']['total_median_s']}s")
    print(f"\nfull data: {out_dir / 'results.json'}")


if __name__ == "__main__":
    main()
