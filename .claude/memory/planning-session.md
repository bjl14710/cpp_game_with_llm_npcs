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
