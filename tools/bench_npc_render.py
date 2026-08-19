#!/usr/bin/env python3
"""Measure the per-frame cost of rendering the NPC roster.

WHAT THIS MEASURES, AND THE TRAP THE OLD VERSION FELL INTO. The game sets
FLAG_VSYNC_HINT *and* SetTargetFPS(60) (src/app/main.cpp), so EndDrawing()
blocks on vblank and raylib sleeps to pace the loop: wall-clock frame time
is pinned near 16.67 ms whenever the work fits inside it. Timing the process
from outside therefore reports THE CAP, NOT THE COST, on any healthy build.
This tool's original 26.4 ms/frame baseline (2026-08-07) was only "real"
because an unoptimised binary — CMake had no default build type then —
genuinely exceeded the cap. The same commit at -O2 fits the cap, and every
wall-clock number taken since is the frame limiter, not the frame. Issue
#266 records the full story.

So the default mode now measures WORK, not wall-clock: it runs the
ENABLE_PROFILING binary, reads the per-frame span samples from the scope
dump (src/profiling/scope_timer.h), and reports frame work as the sum of
the sim + render spans, excluding `present` — `present` is the limiter's
slack, which SHRINKS as real work grows; reading it as cost inverts the
truth. A frame budget is a deadline, so the gate is on p99, not the mean.

The old two-frame-count wall-clock mode survives behind --wall for the two
cases where it still means something: a build whose work exceeds the cap
(that is exactly what it detects), and startup cost. Its marginal
subtraction cancels startup, not warm-up: early frames carry shader
compilation, so always quote the default 300/900 when comparing wall runs.

Requires a display; the game opens a window even in --frames mode.
Stdlib only.

Usage:
  cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_PROFILING=ON
  cmake --build build-prof -j
  python3 tools/bench_npc_render.py                    # work-time (default)
  python3 tools/bench_npc_render.py --gate 16.67       # fail if p99 misses
  python3 tools/bench_npc_render.py --wall             # legacy wall-clock
"""
import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = ROOT / "build" / "cpp_game_with_llm_npcs"
PROF_GAME = ROOT / "build-prof" / "cpp_game_with_llm_npcs"

# The frame-work spans instrumented in src/app/main.cpp. Each records exactly
# one sample per frame, in frame order, so summing index-aligned samples
# reconstructs per-frame work. `present` (EndDrawing) is deliberately absent:
# under vsync it is slack absorption, not work.
WORK_SPANS = ("sim", "render.3d", "render.3d.draw", "render.ui2d")

BUDGET_MS = 16.67  # 60 fps deadline


def build_type(build_dir):
    """Whatever CMAKE_BUILD_TYPE the binary under test was configured with.

    Reported on every run because it silently invalidates comparisons: the
    26.4 ms/frame story above happened because nobody stated the build type.
    A number without its build type is not a measurement.
    """
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        return "unknown (no CMakeCache.txt)"
    for line in cache.read_text(errors="replace").splitlines():
        if line.startswith("CMAKE_BUILD_TYPE:"):
            value = line.split("=", 1)[1].strip()
            return value if value else "EMPTY — unoptimised, no -O flag"
    return "unknown"


def run_game(binary, frames, camera, hour, env=None):
    """One bounded run. Returns wall seconds, or None if the game failed."""
    cmd = [str(binary), "--frames", str(frames),
           "--camera", *[str(c) for c in camera], "--hour", str(hour)]
    start = time.monotonic()
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                            env=env)
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


def frame_work_ms(dump_path):
    """Per-frame work in ms from a scope_timer JSON dump.

    Sums the WORK_SPANS samples index-aligned. Errors loudly when the spans
    are missing — that means the binary was built without ENABLE_PROFILING
    and the dump measured nothing.
    """
    with open(dump_path) as f:
        data = json.load(f)
    scopes = {s["name"]: s["samples"] for s in data["scopes"]}
    missing = [n for n in WORK_SPANS if n not in scopes]
    if missing:
        sys.exit(f"dump lacks span(s) {missing} — was the binary built with "
                 f"-DENABLE_PROFILING=ON?")
    spans = [scopes[n] for n in WORK_SPANS]
    n = min(len(s) for s in spans)
    return [sum(s[i] for s in spans) / 1e6 for i in range(n)]


def pct(sorted_vals, p):
    """Percentile by nearest-rank on an already-sorted list."""
    return sorted_vals[min(len(sorted_vals) - 1, int(p * len(sorted_vals)))]


def work_mode(args):
    """Default mode: frame WORK from the profiling build's span samples."""
    if not PROF_GAME.exists():
        sys.exit(
            f"{PROF_GAME} not built. Work-time measurement needs the "
            f"profiling binary:\n"
            f"  cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo "
            f"-DENABLE_PROFILING=ON\n"
            f"  cmake --build build-prof -j\n"
            f"(or use --wall for the legacy wall-clock mode and its caveats)")

    print(f"  build      {build_type(ROOT / 'build-prof')}  (profiling)")
    runs = []
    for i in range(args.repeats):
        label = f"  [{i + 1}/{args.repeats}]"
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
            dump = tmp.name
        env = dict(os.environ, LLM_NPC_PROF_JSON=dump)
        print(f"{label} running {args.frames} frames...")
        if run_game(PROF_GAME, args.frames, args.camera, args.hour, env) is None:
            return 2
        totals = sorted(frame_work_ms(dump))
        Path(dump).unlink(missing_ok=True)
        runs.append(totals)

    # MEDIAN run by mean, so one scheduling hiccup cannot swamp the result.
    runs.sort(key=statistics.fmean)
    med = runs[len(runs) // 2]
    means = [statistics.fmean(r) for r in runs]
    mean, p50 = statistics.fmean(med), pct(med, 0.50)
    p99, worst = pct(med, 0.99), med[-1]
    over = sum(1 for t in med if t > BUDGET_MS)

    print(f"\n  frame work  mean {mean:.2f}  p50 {p50:.2f}  p99 {p99:.2f}  "
          f"max {worst:.2f} ms   (median of {args.repeats} runs)")
    print(f"  run means   {', '.join(f'{m:.2f}' for m in means)}  "
          f"(spread {max(means) - min(means):.2f} ms)")
    print(f"  deadline    {over}/{len(med)} frames over {BUDGET_MS} ms")

    if args.gate is not None:
        ok = p99 <= args.gate
        print(f"\n  gate p99 <= {args.gate} ms: {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    return 0


def wall_mode(args):
    """Legacy mode: marginal wall-clock between two frame counts.

    Only meaningful when the work EXCEEDS the frame cap — otherwise this
    measures the limiter (see module docstring). Kept for that case and for
    startup cost, which the subtraction isolates.
    """
    if not GAME.exists():
        sys.exit(f"{GAME} not built — run: cmake --build build -j")
    if args.long <= args.short:
        sys.exit("--long must exceed --short, or the difference is meaningless")

    print(f"  build      {build_type(ROOT / 'build')}")
    print(f"  NOTE: wall-clock is pinned to ~{BUDGET_MS} ms by vsync whenever "
          f"the work fits.\n  A result near {BUDGET_MS} means 'fits the cap', "
          f"not 'costs {BUDGET_MS}'.")

    samples = []
    for i in range(args.repeats):
        label = f"  [{i + 1}/{args.repeats}]"
        print(f"{label} timing {args.short} frames...")
        short = run_game(GAME, args.short, args.camera, args.hour)
        print(f"{label} timing {args.long} frames...")
        long_ = run_game(GAME, args.long, args.camera, args.hour)
        if short is None or long_ is None:
            return 2
        ms = (long_ - short) / (args.long - args.short) * 1000.0
        samples.append((ms, short))
        print(f"{label} {short:6.2f} s / {long_:6.2f} s  ->  {ms:5.1f} ms/frame")

    # MEDIAN, not mean: one display waking from sleep produces an outlier big
    # enough to swamp the effect (a negative marginal cost was once observed).
    ordered = sorted(s[0] for s in samples)
    marginal_ms = ordered[len(ordered) // 2]
    spread = ordered[-1] - ordered[0]
    startup_s = min(s[1] for s in samples) - args.short * marginal_ms / 1000.0

    print(f"\n  marginal   {marginal_ms:.1f} ms/frame   median of {args.repeats}")
    print(f"  spread     {spread:.1f} ms  (slowest minus fastest)")
    print(f"  startup    {startup_s:.1f} s")
    if marginal_ms <= BUDGET_MS + 1.0:
        print(f"  verdict    at or under the {BUDGET_MS} ms cap — this number "
              f"is the LIMITER.\n             Use the default work-time mode "
              f"for the real cost.")
    if args.repeats > 1 and spread > abs(marginal_ms) * 0.15:
        print(f"  WARNING    spread is {spread / abs(marginal_ms) * 100:.0f}% "
              f"of the measurement — treat finer comparisons as noise")

    if args.gate is not None:
        ok = marginal_ms <= args.gate
        print(f"\n  gate {args.gate} ms/frame: {'PASS' if ok else 'FAIL'}")
        return 0 if ok else 1
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--wall", action="store_true",
                        help="legacy wall-clock marginal mode (see docstring "
                             "for when it is meaningful)")
    parser.add_argument("--frames", type=int, default=900,
                        help="frames per work-mode run (default 900)")
    parser.add_argument("--short", type=int, default=300,
                        help="wall mode: short frame count")
    parser.add_argument("--long", type=int, default=900,
                        help="wall mode: long frame count")
    parser.add_argument("--camera", nargs=3, type=float, default=[0, 30, 180],
                        metavar=("X", "Z", "YAW"))
    parser.add_argument("--hour", type=float, default=12.0)
    parser.add_argument("--repeats", type=int, default=3,
                        help="measurement passes; the median is reported "
                             "(default 3, because a single pass is not a number)")
    parser.add_argument("--gate", type=float, default=None,
                        help="work mode: fail if frame-work p99 exceeds this "
                             "(the budget is 16.67). wall mode: fail if the "
                             "marginal ms/frame exceeds this")
    args = parser.parse_args(argv)

    personas = len(list((ROOT / "personas").glob("*.persona")))
    print(f"roster: {personas} persona file(s)")

    return wall_mode(args) if args.wall else work_mode(args)


if __name__ == "__main__":
    sys.exit(main())
