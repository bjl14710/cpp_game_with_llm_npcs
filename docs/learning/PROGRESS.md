# Silmulator — Learning Progress

## Learner Profile
- Builder/developer of the Silmulator project itself
- Experienced enough to be writing/fixing Python + Qt code
- Learning goal: understand how to USE Silmulator as an end-user (the HIL/SIL workflow)

## Sessions

### Session 1 (2026-06-20)
**Lesson:** [0001 — How to Use Silmulator](lessons/0001-how-to-use-silmulator.html)
**Covered:**
- What SIL/HIL/CIL means and why Silmulator exists
- The three-step workflow: Place → Connect → Drive
- Adding hardware blocks to the canvas
- Drawing wires between ports
- Sending SCPI commands via the console
- Loading source code (snippet + project folder modes)
- Saving and reloading layouts

**Engagement:** First lesson — baseline not yet measured.
**Noted gaps:** Unknown until quiz responses reviewed.

### Session 2 (2026-06-20)
**Lesson:** [0002 — Connections &amp; Documentation](lessons/0002-connections-and-documentation.html)
**Covered:**
- ConnectionPort internals: port_id, parent_block, connected_to, fractional x/y positioning
- WireItem: ZValue=-1, update_position(), connect_ports() guards, remove_block() wire cleanup
- disconnect() contract — both sides must be nulled or ghost state occurs
- Reading real _setup_ports() patterns across Oscilloscope, MSP430
- How to_dict()/from_dict() serialize and restore connections via port_map
- hardware_docs/ folder purpose, layout, and workflow loop to custom blocks
- Custom block JSON spec: ports, commands (literal / empty / template), discovery at startup
- Contrasting Port vs Wire vs Qt Signal

**Engagement:** Second lesson — quiz results pending.
**Noted gaps:** Unknown until quiz responses reviewed.

### Drill 1 (2026-06-20)
**Drill:** [0001 — How to Use Silmulator](drills/0001-how-to-use-silmulator.html)
**Channels drilled:** workflow · SCPI code path · SIL/HIL/CIL · real-code · bugs
**Questions:** 20 (12 MC + 8 written recall)
**MC score:** pending user run
**Written self-ratings:** pending user run
**Noted gaps:** pending

## Suggested Next Lessons
1. **SCPI command structure** — the IEEE 488.2 tree syntax that every instrument in Silmulator uses; grounded in `silmulator/scpi/parser.py` and the device command handlers
2. **Writing a complete SIL test** — paste real embedded C code into the Code Input block, wire it to simulated hardware, and interpret the responses
3. **Custom blocks** — create a JSON spec for a device that isn't in the library yet (grounded in `silmulator/gui/blocks/custom_block.py`)
4. **Reading the waveform** — inject a sine wave into the oscilloscope and interpret the CURVE? data transfer

---

## Session 5 (2026-07-07 → 07-08) — aim / styles / preview (#91–#93, PR #94)

### Lesson 0023 — The Single Authoritative Source, and the Moonwalk Bug
**Concept:** a value used in two places must be computed in exactly one place —
a shared function (space) or a latched event (time). The moment a second
reconstruction exists, the copies are only coincidentally equal and eventually
drift (the "moonwalk" bug class, cf. #61).
**Grounded in:** `lookDirection(yaw,pitch)` shared by camera + weapon (#91);
the drag-vs-click latch at `IsMouseButtonPressed` (#93); `shortestAngleDelta`
short-arc ease; derive-on-read `defaultYaw = player.yawDeg + 180`.
**Artifacts (local-only):** lessons/0023, drills/0023, vocab/0022.
**Engagement:** overnight autonomous — no interactive quiz run.

## Session 6 (2026-07-08) — shared character library (#95–#98, PR #99) + live fixes (PR #100)

### Lesson 0024 — One Shared Asset Pool: Collapsing Two Pipelines
**Concept:** when two consumers need "the same" content, don't align two
pools — make one pool and give every consumer an authored-or-derived
resolver into it. Growth then reaches every consumer by construction.
**Grounded in:** `drawCharacter` vs `drawCompositeCharacter` audit; the
`customLooks` dispatch hook widened until the rigged branch was
unreachable; `lookForPersona` (authored → FNV-1a-hashed deterministic
fallback); roster test asserting authored/valid/pairwise-distinct.
**Artifacts (local-only):** lessons/0024, drills/0024, vocab/0023.
**Engagement:** overnight/live autonomous — no interactive quiz run.

## Session 7 (2026-07-10) — Mii-style visual overhaul (#101–#107, PR #108)

### Lesson 0025 — Inverted-Hull Outlines in an Immediate-Mode Renderer
**Concept:** cartoon outlines via a second, inflated, front-culled draw of
the SAME geometry — plus the batched-renderer discipline (drain before
state changes, group passes) that makes it correct and cheap.
**Grounded in:** drawCompositeCharacter's two-pass loop (1.06 hull,
rlSetCullFace, rlDrawRenderBatchActive), the drawPartRecipe dispatch that
let one function serve both passes, box corner-gap risk, the un-outlined
emote billboard as the counterexample.
**Artifacts (local-only):** lessons/0025, drills/0025, vocab/0024.
**Engagement:** overnight autonomous — no interactive quiz run.

## Session 8 (2026-07-13/14) — four milestones (#109–#129, PRs #131–#134)

### Lesson 0026 — Generate, Validate, Retry: the Validator IS the Feature
**Concept:** LLM content generation as schema-filling behind a single
validated load path, with model-grade errors as retry feedback; the
contract-fault-vs-model-fault diagnostic (identical cross-model failures
indict YOUR format); closed-world vocabularies; model choice by measured
validity. Grounded in WorldGenValidate/WorldGen/worldgen_cli and the live
iteration that took qwen3:8b from 0% to first-attempt-valid by fixing the
contract (object-per-character) and the vocabulary (style families).
**Choice logged:** one lesson set for the 4-milestone session; the sandbox
concept (compile-to-existing-systems) is documented in PR #131 and overlaps
lessons 0016/0020.
**Artifacts (local-only):** lessons/0026, drills/0026, vocab/0025.

### Lesson 0027 — Bind Pose vs Rest Pose in glTF Skinning
**Concept:** the two default skeleton configurations a skinned glTF
carries (bind pose in inverseBindMatrices + vertices; rest pose in node
TRS), the skinning equation and the IBM's role, the frozen-limb signature
of an engine that assumes bind==rest (raylib never reads IBMs), and the
one-time vertex re-bake (restGlobal x IBM per joint) that fixes both the
static stance and rest-relative animation deltas. Grounded in the session-9
detective story: T-posed Quaternius characters with moving fingers,
ground-truthed from raw JSON (wrist rest position at hip height), fixed in
Assets::rebakeVerticesToRestPose; the shared-rig insight that gave mixed
head/body figures bone attachment for free.
**Artifacts (local-only):** lessons/0027, drills/0027, vocab/0026.

## Session 9 (2026-08-21) — overnight run

### Lesson 0028 — Inverting a Clock's Ownership Without Disturbing Its Consumers
**Concept:** how a producer can be swapped under six consumers at zero cost
when they are coupled to a shared VALUE rather than to a producer call or a
tick event; the inversion itself (free roam ticks the hour, a match derives it
from phase progress so dusk lands exactly when the vote opens); enforcing
"exactly one owner" through topology — a single `advanceWorldClock` write site
plus `std::optional<MatchClock>` as the sole representation of whether a match
is running, rather than a bool flag that can drift; why a new enum value
(`MatchPhase::Intro`) is cheaper than the same condition repeated at every
downstream switch, and why Intro holds the clock at dawn while Vote/Resolution
hold it at dusk; the diegetic countdown (dayEndHour inside DayNight's dusk
band) and the test that pins it; and proving an untouched path is untouched
with a byte-identical `--frames` capture rather than an argument.
Grounded in issue #155: MatchClock existed and was entirely unused until this
change wired it into the loop.
**Artifacts (local-only):** lessons/0028, drills/0028, vocab/0027.

### Lesson 0029 — Layering a Temporary Override Over Authored Content
From issue #156 (plaza gather for the vote phase), PR #310.

Covered: why rewriting authored schedules and adding a mode flag both fail; the
nullable-destination-override shape and its restore-by-reset property; "out-ranked,
not suspended" and how it stays visible in the activity label; splitting a bundled
guard without smuggling in an incidental behaviour change; keying a release on the
state ENTERED so untried transitions are covered; the map fact that the plaza's
computed centre sits inside Gus's cart (and the `Mystery.cpp` warning that had been
reporting it for months); spacing vs. formation logic, and why a gap in the ring is
the feature; the vacuous-test trap of a movement fixture spawned inside a collider.

Files: lessons/0029, drills/0029 (15 questions), vocab/0028 (12 terms), GLOSSARY.

### Lesson 0030 — Diegetic Versus HUD Signalling for Time Pressure
From issue #157 (day/phase HUD and match settings), PR #311.

Covered: diegetic vs HUD signalling and the precision/attention trade; how one
constant (`dayEndHour` inside DayNight's dusk band) turned an existing light model
into a countdown, and why #155's clock inversion was the prerequisite; what the sky
CANNOT say, and why two HUD lines is the answer; the stated HUD budget as a design
guard; raylib's integer glyph advance and the 10/20/30/44 ladder; making "settings
cannot affect a running match" structural via a single read site (weak vs strong
invariants); trailing defaults hiding missing cases (Sandbox and Model were titled
"Multiplayer" for months) and the no-`default:` switch that fixes it; magic layout
offsets hiding overflow (Quit was 14px off-screen) and derived layout; the harness
boundary — `tests/Makefile` globs only `src/core/*.cpp`, so all of `src/app/` is
verified by capture or not at all.

Files: lessons/0030, drills/0030 (15 questions), vocab/0029 (13 terms), GLOSSARY.

### Lesson 0031 — One Implementation Across a Language Boundary
From issue #304 (persona_prompt --state), PR #312.

Covered: why re-implementing prompt assembly in Python would produce an invalid
instrument rather than merely duplicated code (placement is what the probe
measures); the `--parse` precedent and the codebase's own admitted counterexample
in bench_npc_models.py; inverting the unknown-id policy at a measurement boundary
(the game demotes, the tool refuses) and why zero-byte refusal matters; the
byte-identity check as the thing that keeps the seam honest; residue testing against
a known harness boundary; the pointer-into-argument hazard in findRole.

Files: lessons/0031, drills/0031 (10 questions), vocab/0030 (8 terms), GLOSSARY.
