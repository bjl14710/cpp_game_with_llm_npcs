# Frame Budget — where the time actually goes

Status: **NOT YET MEASURED.** This is the skeleton for phase 3 of
`.claude/plans/cpp20-upgrade.md`. Every number below is a placeholder marked
`TBD`. Do not cite this document until the TBDs are gone.

The point of this file is that the answer is currently **not known**. Two
plausible explanations are recorded below so they can be tested, not so they
can be assumed.

---

## Resolve this first: the recorded baseline is contradicted

Two places in the repo disagree, and both are load-bearing for the
twenty-one-residents milestone.

| Source | Claim |
|---|---|
| `tools/bench_npc_render.py` docstring | 26.4 ms/frame at 10 personas, 35.3 at 21, ~0.81 ms per NPC, measured 2026-08-07 on an M1 Pro |
| `CMakeLists.txt:9-26` | every recorded render number was measured on an **unoptimised** build; the same commit at `-O2` measures **~17 ms** |

CMake now defaults to `RelWithDebInfo`, so a run today does not measure the
same thing the docstring describes. **The 0.81 ms/NPC figure inherits the same
problem** and cannot be trusted either.

- [ ] Re-measure on `RelWithDebInfo` and record it here.
- [ ] Correct or annotate the `bench_npc_render.py` docstring so the stale
      figure stops being quoted. It is currently the most-cited number in the
      project and it was taken without `-O`.
- [ ] State whether the twenty-one-residents gate (26.4 ms) still means
      anything, given it was set against an unoptimised build.

---

## Method

Fill in exactly what was run, so this is reproducible rather than anecdotal.

- Build type: `TBD` (must be `RelWithDebInfo` or `Release` — say which)
- Command: `python3 tools/bench_npc_render.py --short 300 --long 900`
- Frame counts: **quote 300/900.** The script's own docstring warns that
  120/360 reports 17.8 ms where 300/900 reports 26.4 ms on the same build,
  because early frames carry shader compilation. The subtraction cancels
  startup, not warm-up. Comparing across different counts is how the earlier
  baseline went wrong.
- Machine: `TBD`
- Resident count: `TBD`
- Profiler: `TBD` (`/usr/bin/sample <pid> 10 -file out.sample`, or Instruments
  Time Profiler)

## Measured baseline

| Residents | ms/frame | fps | startup |
|---|---|---|---|
| TBD | TBD | TBD | TBD |

Marginal cost per NPC: `TBD` ms.

## Cost attribution

Ranked, from the profiler. Percentages of a frame, not of each other.

| Rank | Site | ms/frame | % | Scales with |
|---|---|---|---|---|
| TBD | TBD | TBD | TBD | TBD |

## Hypotheses under test

Recorded before measuring so the result can contradict them. **If the profile
supports neither, say so plainly and report what it actually shows.**

### H1 — immediate-mode city geometry (fixed cost)

`src/app/RaylibRenderer.cpp` makes ~85 draw calls, many `DrawCube`,
`DrawCylinder` and their `Wires` variants, for streets, plazas, buildings,
fountains and lamps. raylib's 3D shape helpers upload geometry on every call
with no batching, and the city is static — it is rebuilt from scratch each
frame.

- Predicts: cost roughly **flat** in NPC count.
- If true, the fix is baking static geometry into meshes once, and it is
  unrelated to the resident count.
- Verdict: `TBD`

### H2 — characters drawn twice (scales with residents)

Each character appears to be drawn twice: `DrawModelEx` at
`RaylibRenderer.cpp:331`, then a per-mesh rim pass via `DrawMesh` at `:350`.

- Predicts: cost **linear** in NPC count, and consistent with a per-NPC
  marginal figure.
- If true, this gates the twenty-one-residents milestone directly.
- Verdict: `TBD`

## Memory

Not measured by anything in the project today, and named explicitly in the
request that produced this work — so it gets a section rather than being
quietly dropped.

- [ ] Resident set size at 10 residents, after startup settles: `TBD`
- [ ] Growth per additional resident: `TBD`
- [ ] Whether per-frame allocation is happening at all (`heap`/Instruments
      Allocations, or `MallocStackLogging`): `TBD`
- [ ] Largest single consumer (likely model/texture data — `Assets.hpp` holds
      several `unordered_map<std::string, Model>`): `TBD`

Note for whoever measures this: per-frame *allocation* is the interesting
number, not peak footprint. A steady 400 MB is fine; 400 KB churned every
frame is not.

## What this document does NOT contain

**No fixes.** Deciding what to do about the numbers is a separate plan, written
once the numbers exist. Optimising before this file is filled in is the exact
mistake that produced the contradiction at the top of it.
