#!/usr/bin/env python3
"""Schema-validity benchmark for world-generation models (plan:
llm-world-generation, issue #127).

Drives the C++ worldgen_cli — so the bench and the game share ONE
validation contract — over a FIXED prompt corpus, per candidate model:
  * validity rate (valid within the CLI's 3-attempt retry loop)
  * first-attempt validity rate
  * mean seconds per run
The generation model should be picked by these numbers, never by label.

Usage:
  python3 tools/bench_schema_models.py [--models m1 m2 ...] [--out FILE]
Models default to whatever the local Ollama has pulled.
Results: bench/out/schema_results.json + a markdown table on stdout.
"""

import argparse
import json
import pathlib
import subprocess
import sys
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
CLI = ROOT / "build" / "worldgen_cli"

# Fixed corpus so runs compare across models. (mode, description)
CORPUS = [
    ("gen-cast", "a grumpy old sailor who runs the docks, and two curious "
                 "kids who pester him"),
    ("gen-cast", "a cheerful market trio: a fishmonger, a florist, and a "
                 "pickpocket pretending to be a mime"),
    ("gen-map", "a tiny fishing village: a couple of shops near a fountain "
                "square, a bench, some greenery"),
    ("gen-village", "a fishing village with a grumpy old sailor and two "
                    "kids playing near the fountain"),
]


def installed_models() -> list[str]:
    try:
        with urllib.request.urlopen("http://127.0.0.1:11434/api/tags",
                                    timeout=5) as response:
            return [m["name"] for m in json.load(response)["models"]]
    except Exception:
        return []


def run_one(mode: str, description: str, model: str) -> dict:
    proc = subprocess.run(
        [str(CLI), mode, description, model],
        capture_output=True, text=True, cwd=ROOT, timeout=600)
    stats = {}
    for line in proc.stderr.splitlines():
        if line.startswith("[worldgen-stats] "):
            stats = json.loads(line[len("[worldgen-stats] "):])
    stats.setdefault("valid", False)
    stats.setdefault("attempts", 0)
    stats.setdefault("seconds", 0.0)
    stats["mode"] = mode
    return stats


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--models", nargs="*")
    parser.add_argument("--out", default="bench/out/schema_results.json")
    args = parser.parse_args()

    if not CLI.exists():
        print("build worldgen_cli first (cmake --build build)", file=sys.stderr)
        return 1
    models = args.models or installed_models()
    if not models:
        print("no models available (is Ollama running?)", file=sys.stderr)
        return 1

    results = {}
    for model in models:
        runs = []
        for mode, description in CORPUS:
            stats = run_one(mode, description, model)
            runs.append(stats)
            print(f"  {model} {mode}: valid={stats['valid']} "
                  f"attempts={stats['attempts']} {stats['seconds']:.1f}s",
                  flush=True)
        valid = sum(1 for r in runs if r["valid"])
        first = sum(1 for r in runs if r["valid"] and r["attempts"] == 1)
        results[model] = {
            "runs": runs,
            "validity_rate": valid / len(runs),
            "first_attempt_rate": first / len(runs),
            "mean_seconds": sum(r["seconds"] for r in runs) / len(runs),
        }

    out = ROOT / args.out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=2))

    print("\n| model | valid | first-try | mean s |")
    print("|-------|-------|-----------|--------|")
    for model, r in sorted(results.items(),
                           key=lambda kv: -kv[1]["validity_rate"]):
        print(f"| {model} | {r['validity_rate']:.0%} "
              f"| {r['first_attempt_rate']:.0%} | {r['mean_seconds']:.1f} |")
    print(f"\nresults -> {out}")
    winner = max(results.items(), key=lambda kv: kv[1]["validity_rate"])
    print(f"measured winner: {winner[0]} "
          f"({winner[1]['validity_rate']:.0%} valid)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
