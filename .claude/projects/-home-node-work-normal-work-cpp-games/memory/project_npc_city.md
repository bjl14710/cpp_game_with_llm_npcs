---
name: project-npc-city
description: "State and resume procedure for the LLM-NPC city game build (21-commit plan, staging dir, Ollama install pending)"
metadata: 
  node_type: memory
  type: project
  originSessionId: 6c7eea96-0ab5-4832-b11a-f8140647c134
---

Building a first-person 3D city with 10 LLM NPCs in `cpp_game_with_llm_npcs`,
following the approved 21-commit plan at
`/home/node/.claude/plans/tender-herding-cupcake.md`; the user's verbatim
prompt is saved at `/home/node/work/normal_work/cpp_games/NPC_CITY_PROMPT.txt`.

**Why:** user wants small reviewable commits (2-3 files) pushed to branch
`initial_npcs_and_world`, characters/communication first, constant testing.

**How to apply:** before any commit work, read
`/home/node/work/normal_work/cpp_games/npc_staging/NOTES.md` — it maps every
pre-written/staged file to its plan commit and records the discovered commit
reordering (app commits: 16 → 18 → 19 → 17). All core sources, tests, and
personas were pre-written into the repo working tree while the Bash tool was
blocked by a classifier outage (2026-06-11/12); nothing committed yet, doctest
not vendored, Ollama not installed. Resume = retry Bash, then: repo check,
vendor doctest, `make -C tests test`, fix compile errors, start the commit
sequence with selective `git add` per commit.
