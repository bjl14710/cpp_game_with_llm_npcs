# Frame Budget — where the time actually goes

Status: **MEASURED** 2026-08-14 (timing and memory), superseding the
2026-08-10 timing pass whose headline attribution carried an instrumentation
bug (corrected below — the conclusions survived, the magnitude did not).

Budget: **16.67 ms** (60 fps). A budget is a deadline, not an average — the
verdicts below are on p99, with the mean for context.

---

## The contradiction at the top of the old skeleton, resolved

The two recorded baselines disagreed because **both were wrong, differently**:

| Source | Claim | What it actually was |
|---|---|---|
| `tools/bench_npc_render.py` (old docstring) | 26.4 ms/frame at 10 personas | Real, but measured on an **unoptimised** build (CMake had no default build type) |
| `CMakeLists.txt` note | same commit at `-O2` ≈ 17 ms | **The frame limiter.** `FLAG_VSYNC_HINT` + `SetTargetFPS(60)` pin wall-clock near 16.67 ms whenever the work fits — wall-clock cannot see the real cost on a healthy build |

- [x] Re-measured on `RelWithDebInfo` — via frame **work** (span samples from
      the `ENABLE_PROFILING` build), not wall-clock. Numbers below.
- [x] `bench_npc_render.py` corrected — issue #266 rewrote it to measure
      work by default and to say out loud when a wall-clock result is the cap.
- [x] The twenty-one-residents gate re-derived — the 26.4 figure means
      nothing; the real gate arithmetic is on #173 (short version: ~0.95 ms
      of character rendering per resident ⇒ 21 ≈ 20 ms ⇒ gated on #170).

## Method

- Build: `RelWithDebInfo`, C++20, Apple clang, arm64 (M1 Pro), macOS
- Instrumentation: `src/profiling/scope_timer.h` via
  `cmake -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_PROFILING=ON`
- Workload: 10 wandering town residents, `--hour 12`, `--frames 900`,
  cameras `--camera 0 30 180` (far) and `--camera 0 13 180` (near)
- Protocol: **3 runs, median reported**, spread checked (≤0.67 ms on frame
  means). Frame work = sum of the per-frame span samples for
  `sim` + `render.3d` + `render.3d.draw` + `render.ui2d`. `present`
  (EndDrawing) is **excluded**: under vsync it is slack that shrinks as work
  grows — reading it as cost inverts the truth.
- Dates/commits: baseline at dev `898931b`; "after" is PR #268
  (`feature/issue-171-outline-skip-at-range`).

## Measured baseline — frame work, ms

| Workload | dev `898931b` | after #171 gate | p99 before → after | frames over 16.67 before → after |
|---|---|---|---|---|
| far (z=30) | 13.39 mean | **9.90** | 18.02 → 14.22 | 11/900 → 4/900 |
| near (z=13) | 13.51 mean | **10.18** | 17.41 → 12.31 | 13/900 → **0/900** |

## Cost attribution (dev `898931b`, far camera, corrected)

| Rank | Site | ms/frame | % of budget | Scales with |
|---|---|---|---|---|
| 1 | inverted-hull outline pass (both character paths) | ≈5.0 | ≈30% | residents in view |
| 2 | base character draw (pose, recipes/meshes, decal) | ≈8.0 | ≈48% | residents in view |
| 3 | `present` (frame limiter slack — not work) | 3.55 | — | inversely with work |
| 4 | city (all ~85 immediate-mode draws) | 0.34 | 2.0% | fixed |
| 5 | UI 2D | 0.04 | 0.2% | fixed |
| 6 | simulation, all of it (AI + gossip + location log) | 0.01 | 0.1% | residents |

**Correction recorded here on purpose:** the 08-10 report attributed
12.44 ms (74.6% of budget, "96% of character rendering") to the outline
pass. That scope had no enclosing braces, so its RAII timer ran to the end
of `drawCompositeCharacter` and swallowed the base draw and face decal. The
fix (in PR #268) brackets exactly the hull pass; the split above is derived
from the gated A/B (outline removed at range = 3.5 ms + still-in-range
outline = 1.5 ms).

## Hypotheses — verdicts

### H1 — immediate-mode city geometry (fixed cost): **REFUTED**

0.34 ms/frame, 2% of budget, flat in resident count. Baking static geometry
into meshes would buy nothing measurable. Drop the idea, do not defer it.

### H2 — characters drawn twice (scales with residents): **CONFIRMED**

Character rendering was ~13 ms of a 13.4 ms frame, ~0.5 ms of which was the
outline pass being issued per part per resident (corrected magnitude:
≈5 ms/frame across ten). #171 gates it beyond 25 units; what remains is the
~0.8–0.9 ms/resident base draw, which is #170's territory.

## Memory (measured 2026-08-14, same build family, display held awake¹)

| Metric | Value | How taken |
|---|---|---|
| Peak footprint, 10 residents, 900 frames | **623 MB** (max RSS 423 MB) | `/usr/bin/time -l`, `peak memory footprint` |
| Peak footprint, 0 residents (empty `--map`), 900 frames | 559 MB (max RSS 386 MB) | same |
| Growth per additional resident | **≈6.4 MB** (64 MB / 10 — roster average, not marginal) | delta of the two rows above |
| Growth across run length | **none**: 609 → 623 → 622 MB at 300/900/1800 frames | three bounded runs |
| Per-frame allocation growth | **none**: malloc heap 68,744 nodes / 98.43 MB → 68,068 / 98.37 MB across ~900 frames mid-run (net −61 KB) | two `heap` snapshots 15 s apart on a live 3000-frame run |
| Largest consumer | graphics memory, not malloc: the malloc heap is ~94 MB of the ~620 MB footprint; the rest is HiDPI+MSAA framebuffers and GPU-uploaded meshes/textures. Within malloc, the 2 KB size class dominates (27,296 nodes ≈ 54 MB), consistent with CPU-side mesh/skinning data behind `Assets`' `Model` maps | `heap` size-class table |

**Not measured:** the per-frame malloc *churn rate* (allocations made and
freed within a frame). The net-zero heap and flat footprint bound its
*leak* consequence at zero, but a hot loop can still pay malloc time while
netting nothing; counting that needs `malloc_history`/Instruments
Allocations and was not sampled. The steady 2.5 KB[8128] + 2 KB[27296]
populations suggest stable pools, not churn, but that is inference, not
measurement.

¹ Methodology trap, recorded because it burned this measurement's first
attempt: with the display asleep, footprint collapses to ~198 MB — the
window never gets real backing stores and the numbers describe a game that
is not rendering. Wrap every run in `caffeinate -dimsu`.

## What this document does NOT contain

**No fixes.** Deciding what to do about the numbers is a separate plan,
written once the numbers exist. The follow-ups that exist as of this
writing: #170 (cull whole characters — top item), #265 (batch the hull
pass, bounded at 1.5–1.9 ms), #266 (bench tool), #173 (gate arithmetic).
