#!/usr/bin/env python3
"""Measure the per-frame cost of rendering the NPC roster.

Plan: .claude/plans/twenty-one-residents.md. This encodes the measurement that
plan is built on, so "re-measure" is one command instead of a procedure someone
has to remember.

WHY TWO FRAME COUNTS. A single timed run bundles startup (model loading, asset
decode, Ollama warm-up — about 3 s) with per-frame cost, and startup dominates
a short run. Timing at two frame counts and taking the difference cancels it:

    marginal_ms = (t_long - t_short) / (frames_long - frames_short) * 1000
    startup_s   = t_short - frames_short * marginal_ms / 1000

Baseline measured 2026-08-07 on an M1 Pro, at the current roster of 10:

    10 personas   26.4 ms/frame   (37.9 fps)   3.2 s startup
    21 personas   35.3 ms/frame   (28.3 fps)   4.3 s startup

That is ~0.81 ms per NPC per frame, and the game is ALREADY under 60 fps at
ten. The plan's gate is that 21 residents must reach 26.4 ms/frame — today's
ten-resident cost — before a single new persona is authored.

COMPARE LIKE WITH LIKE. The marginal figure depends on the frame counts
chosen: early frames carry shader compilation and texture upload, so a
120/360 run reports a lower marginal cost than 300/900 on the same build
(17.8 vs 26.4 ms measured). The subtraction cancels startup, not warm-up.
Always quote the default 300/900 when comparing against the baseline above,
and say which counts you used if you deviate.

Requires a display; the game opens a window even in --frames mode. Stdlib only.

Usage:
  python3 tools/bench_npc_render.py                    # measure as-is
  python3 tools/bench_npc_render.py --gate 26.4        # fail if slower
  python3 tools/bench_npc_render.py --short 300 --long 900
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "build" / "cpp_game_with_llm_npcs"

# The plan's gate: 21 residents must be no worse than today's ten.
BASELINE_MS = 26.4


def build_type():
    """Whatever CMAKE_BUILD_TYPE the binary under test was configured with.

    Reported on every run because it silently invalidates comparisons. The
    project had no default, so CMake's empty-string default applied and
    `cmake --build build` produced an UNOPTIMISED binary — which is how the
    26.4 ms/frame baseline above was measured. The same commit at -O2 measures
    around 17 ms. A number without its build type is not a measurement.
    """
    cache = ROOT / "build" / "CMakeCache.txt"
    if not cache.exists():
        return "unknown (no CMakeCache.txt)"
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("CMAKE_BUILD_TYPE:"):
            value = line.split("=", 1)[1].strip()
            return value if value else "EMPTY — unoptimised, no -O flag"
    return "unknown"


def timed_run(frames, camera, hour):
    """Wall-clock seconds for a bounded run. Returns None if the game failed."""
    cmd = [str(GAME), "--frames", str(frames),
           "--camera", *[str(c) for c in camera], "--hour", str(hour)]
    start = time.monotonic()
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    elapsed = time.monotonic() - start
    if result.returncode != 0:
        print(f"game exited {result.returncode}:\n{result.stderr[-800:]}",
              file=sys.stderr)
        return None
    # A persona that fails to load is silently absent from the frame, which
    # would make the measurement a lie. Surface it rather than average it in.
    bad = [line for line in result.stderr.splitlines() if "persona error" in line]
    if bad:
        print(f"  warning: {len(bad)} persona(s) failed to load — "
              f"they are NOT in this measurement", file=sys.stderr)
    return elapsed


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--short", type=int, default=300)
    parser.add_argument("--long", type=int, default=900)
    parser.add_argument("--camera", nargs=3, type=float, default=[0, 30, 180],
                        metavar=("X", "Z", "YAW"))
    parser.add_argument("--hour", type=float, default=12.0)
    parser.add_argument("--repeats", type=int, default=3,
                        help="measurement passes; the median is reported "
                             "(default 3, because a single pass is not a number)")
    parser.add_argument("--gate", type=float, default=None,
                        help=f"fail if marginal ms/frame exceeds this "
                             f"(the plan's gate is {BASELINE_MS})")
    args = parser.parse_args(argv)

    if not GAME.exists():
        sys.exit(f"{GAME} not built — run: cmake --build build -j")
    if args.long <= args.short:
        sys.exit("--long must exceed --short, or the difference is meaningless")

    personas = len(list((ROOT / "personas").glob("*.persona")))
    print(f"roster: {personas} persona file(s)")

    print(f"  build      {build_type()}")

    samples = []
    for i in range(args.repeats):
        label = f"  [{i + 1}/{args.repeats}]"
        print(f"{label} timing {args.short} frames...")
        short = timed_run(args.short, args.camera, args.hour)
        print(f"{label} timing {args.long} frames...")
        long_ = timed_run(args.long, args.camera, args.hour)
        if short is None or long_ is None:
            return 2
        ms = (long_ - short) / (args.long - args.short) * 1000.0
        samples.append((ms, short))
        print(f"{label} {short:6.2f} s / {long_:6.2f} s  ->  {ms:5.1f} ms/frame")

    # MEDIAN, not mean. A single scheduling hiccup or a display waking from
    # sleep produces an outlier large enough to swamp the effect being measured
    # — one observed run reported a NEGATIVE marginal cost because the 900-frame
    # pass happened to be faster than the 300-frame one.
    ordered = sorted(s[0] for s in samples)
    marginal_ms = ordered[len(ordered) // 2]
    spread = ordered[-1] - ordered[0]
    startup_s = min(s[1] for s in samples) - args.short * marginal_ms / 1000.0
    fps = 1000.0 / marginal_ms if marginal_ms > 0 else float("inf")

    print(f"\n  marginal   {marginal_ms:.1f} ms/frame  ({fps:.1f} fps)   median of {args.repeats}")
    print(f"  spread     {spread:.1f} ms  (slowest minus fastest)")
    print(f"  startup    {startup_s:.1f} s")
    # A gate is meaningless when the noise is wider than the thing being gated.
    if args.repeats > 1 and spread > abs(marginal_ms) * 0.15:
        print(f"  WARNING    spread is {spread / abs(marginal_ms) * 100:.0f}% of the "
              f"measurement — treat any comparison finer than that as noise")
    if personas:
        print(f"  per NPC    {marginal_ms / personas:.2f} ms/frame "
              f"(crude: assumes all cost is per-NPC)")

    if args.gate is not None:
        ok = marginal_ms <= args.gate
        print(f"\n  gate {args.gate} ms/frame: {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
