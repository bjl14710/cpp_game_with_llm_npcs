# Issue Planning Summary
Date: 2026-08-07 23:54
Repo: bjl14710/cpp_game_with_llm_npcs
Focus: idea 3 — twenty-one distinct residents
Milestone: #21 Detective mode: twenty-one distinct residents
Plan: .claude/plans/twenty-one-residents.md
Scaffold: feature/residents-scaffold @ c1ad0c4 (pushed)

## Issues Created
- #170: perf(render): distance-cull NPC rendering
  Concept: Deriving a cull radius from an existing fog falloff constant
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/170

- #171: perf(render): skip the inverted-hull outline pass at range
  Concept: Cost of screen-space-invariant effects at distance
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/171

- #172: test(roster): diversity gate on silhouette, trait set and style
  Concept: Authoring constraints enforced as tests
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/172

- #173: content(personas): author eleven new residents and flip the roster to 21
  Concept: Combinatorial budget of a socketed character catalog
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/173

## Dependency order
cull -> outline-skip -> (GATE: 26.4 ms/frame) -> author residents.
The diversity gate issue is INDEPENDENT and can run in parallel.

## What Was NOT Planned
- Frustum culling. Distance only; frustum has its own correctness questions
  and belongs in a separate change.
- A general LOD system, mesh LODs, impostors or instancing. 21 characters is
  not a culling problem.
- Procedural crowds. A separate unported legacy feature that would spend the
  render budget this milestone buys back.
- A fifth core head. Only if the 24-silhouette budget genuinely runs out
  during authoring; pre-emptively adding one is not planned.
- Relationships between residents. Not testable and not needed until the
  mystery casts them.

---

# Issue Planning Summary
Date: 2026-08-07 23:38
Repo: bjl14710/cpp_game_with_llm_npcs
Focus: idea 2 — Clue-like named zones
Milestone: #20 Detective mode: named zones and the alibi log
Plan: .claude/plans/clue-like-zones.md
Scaffold: feature/zones-scaffold @ fd287eb (branch from this, not dev)

## Issues Created
- #163: feat(zones): named regions over the downtown block grid
  Concept: Deriving a spatial partition from the constants that authored the map
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/163

- #164: feat(zones): transition-based location log for every agent
  Concept: Interval records versus sampling for time-range queries
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/164

- #165: feat(zones): tick the location log for every agent including the player
  Concept: Observing simulation state without coupling to it
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/165

- #166: feat(city): render building signage from Building::name
  Concept: Billboarded world-space labels under a fixed-metrics bitmap font
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/166

## Dependency order
zones-lookup -> location-log -> tick-location-log. The signage issue is
INDEPENDENT of all three and can be done in parallel or first.

## What Was NOT Planned
- Per-NPC testimony / who-saw-whom. Ground truth only this milestone; the
  witnessing layer is .claude/plans/alibis-and-testimony.md.
- Building interiors. Enterable buildings live only in unported SFML branches
  and are an L/XL of their own; zones deliberately do not need them.
- Per-building sub-zones and named street segments. Both are strict
  refinements of this partition and change no API, so they wait for evidence
  that block granularity is too coarse.
- A font atlas. Wanted by signage AND cutscene captions AND the aesthetics
  handoff — it deserves its own issue rather than being smuggled into one.

---

# Issue Planning Summary
Date: 2026-08-07 22:50
Repo: bjl14710/cpp_game_with_llm_npcs
Focus: game-day boundaries and phases
Milestone: #19 Detective mode: game-day phases
Plan: .claude/plans/detective-day-phases.md

## Issues Created
- #154: feat(match): day counter, phase machine and match-paced world hour
  Concept: Deriving simulation time from phase progress rather than wall time
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/154

- #155: feat(match): drive the world clock from the match, not a fixed time scale
  Concept: Inverting a clock's ownership without disturbing its consumers
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/155

- #156: feat(match): gather every resident at the plaza for the vote phase
  Concept: Temporary destination overrides layered over authored schedules
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/156

- #157: feat(match): day/phase HUD and match settings
  Concept: Diegetic versus HUD signalling for time pressure
  URL: https://github.com/bjl14710/cpp_game_with_llm_npcs/issues/157

## Dependency order
Sequential. #1 is pure and self-contained; #2 wires it in; #3 and #4 both
depend on #2 but are independent of each other.

## What Was NOT Planned
- The vote itself, the murder, alibis, cutscenes — each has its own plan in
  .claude/plans/ and its own milestone when queued.
- Networked phase replication. detective-day-phases deliberately leaves the
  seam (three plain numbers) but the wire work belongs to the networked-match
  plan.
- The three open questions from the plan (dialogue interruption at dusk, the
  authored plaza marker, day-1 start hour). None block issue 1.

---

# Issue Planning Summary
Date: 2026-07-10
Repo: bjl14710/cpp_game_with_llm_npcs
Focus: mii-style-visual-overhaul (visuals + character design)

## Milestone
#12 — Mii-style visual overhaul & player avatar
Plan: .claude/plans/mii-style-visual-overhaul.md
Skill: .claude/skills/mii-style/SKILL.md
Branch: feature/mii-style-visual-overhaul (scaffold already on it, uncommitted),
stacked on feature/punch-and-jump. Chain: #94 <- #99 <- #100 <- this.

## Issues Created (work in number order)
- #101 pack seam + recipe dispatch — concept: content-pack seams via tagged data
- #102 Mii proportions + pastel palettes — concept: art direction as pure data
- #103 inverted-hull outlines — concept: inverted-hull outlining in immediate mode
- #104 Mouth category + six-item look — concept: extending a category-spec system
- #105 catalog expansion (hair/eyes/bodies/palettes) — concept: scaling a content catalog
- #106 player avatar via shared creator — concept: one UI, two write targets
- #107 visual gate + playtest findings — concept: self-directed playtesting

## What Was NOT Planned (and why)
- Engine migration: Part A verdict is NO (recorded in the plan) — raylib
  supports the toon target; migration would strand the C++ core + 176 tests.
- Post-process outline shader: inverted hull is v1; shader is the logged
  fallback if hull outlines fail on boxes.
- Remote-player avatar replication: needs net protocol change; logged seam.
- City/building restyle: only characters this milestone; sky/light constants
  may be gently tuned if characters clash (allowed within #102).
- Acting on #107 playtest findings: observations-only by explicit user brief.

## Session-6/7 environment facts that matter tonight
- GitHub via zsh -c 'source ~/.zshrc; export GH_TOKEN="$TOKEN"; python3 ...'
- NEVER git add . — stage explicit src/tests/personas paths only.
- Screenshot recipe: caffeinate -u -t 3; ./build/cpp_game_with_llm_npcs
  --frames 90 shot.png --camera x z yaw --hour 12 (PNG lands in CWD).
- tests/test_style_pack.cpp stubs: un-skip each in the same commit as its step.

---
# Planning addendum: 2026-07-12 — four milestones queued
- #13 Sandbox map editor: #109-#114 (branch feature/sandbox-map-editor, scaffolded)
- #14 Traits + ratings: #115-#119 (feature/npc-traits, stack on sandbox)
- #15 Groups + follow: #120-#124 (feature/group-conversations)
- #16 LLM world-gen: #125-#129 (feature/llm-world-generation)
Build order 13->14->15->16; shared contracts in .claude/skills/sandbox-worldgen/SKILL.md.
Scaffold uncommitted on feature/sandbox-map-editor; each milestone commits its own subset.

---
# Planning addendum: 2026-07-14 — stylized characters queued
- Milestone #17 "Stylized character assets": issues #137-#143 on
  feature/stylized-characters (scaffolded, off dev). Skill:
  .claude/skills/stylized-characters/SKILL.md (ENGINE = RAYLIB, not Godot
  — premise correction logged in the plan).
- NOT queued yet: worldgen-fix-and-catalog plan (silent-Generate fix +
  trees/catalog) — run /plan-github for it or fold into the same night.
- Recommended run order if both go tonight: worldgen fix FIRST (smaller,
  live bug), stylized second (XL, renderer-wide).
