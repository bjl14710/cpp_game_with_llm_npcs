# Sandbox / Traits / Groups / World-Gen — Shared Contracts

Read before implementing ANY issue from these four plans so all overnight
runs make the same choices. Plans: sandbox-map-editor.md,
npc-traits-and-ratings.md, group-conversations.md, llm-world-generation.md
(all in .claude/plans/). Scaffold headers are the interface spec:
PieceCatalog.hpp, SandboxMap.hpp, Trait.hpp, GroupSession.hpp,
WorldGenValidate.hpp — implement TO them; change them only with a logged
reason.

## The map JSON contract (sandbox editor ⇄ LLM generation)
```json
{ "version": 1, "name": "Fishing Village", "tile": 8,
  "pieces": [{"piece": "shop_bakery", "x": -3, "z": 2}],
  "npcs":   [{"source": "persona:baker", "x": -20.0, "z": 18.0,
              "facing": 180.0}] }
```
- Flat, explicit, no derived state — an LLM must be able to emit it
  directly. Never add fields the loaders compute themselves.
- Pieces are TILE coordinates (8-unit grid, axis-aligned, no rotation in
  v1); NPCs are world units (not grid-bound).
- `source` forms: `persona:<stem>` (shipped roster) |
  `character:<characterId>` (CharacterStore).
- Hand-loads are REPAIRABLE (bad entries dropped with a toast, map opens
  in edit mode); LLM-loads are STRICT (any validateMap/validateCast error
  → retry with feedback, ≤3, then plain failure; no auto-repair, no
  partial loads).

## Compile-to-existing-systems rule (sandbox)
A solid piece becomes a normal `Building` {id: assetId + "#" + n, AABB
from footprint, height} — collision (City) and rendering (Assets) follow
with ZERO new runtime. The only renderer change allowed: strip the '#'
suffix in the curated-model lookup. Visual pieces (road paint) emit no
Building. If an editor feature seems to need a new render/collision path,
stop — re-read the plan.

## World repopulation seam
World cannot be reassigned (Npc holds LlmClient&): `World::loadCity(City)`
swaps contents (city, clears npcs_/projectiles_). Main's roster spawning
and per-NPC side arrays (npcLooks, npcLastPos, wasCaught, savedTurns) must
be extracted into lambdas (`spawnTownRoster`, `spawnMapNpcs`,
`resetNpcSideArrays`) BEFORE sandbox play lands — town ↔ map ↔ town must
round-trip. Placed NPCs: schedules SUPPRESSED (town coordinates are
nonsense in custom maps); dialogue/mood/memory/gossip/follow all live.

## Trait prompt-order contract
Assembly order is TESTED, not conventional:
`identity/backstory < trait rules < trait examples < memory summary <
trait reinforcement` — pin with string-index comparisons. Reinforcement
(1-2 lines) AFTER memory is the anti-drift requirement from the brief.
Traits: ≤3 per persona (parse error above), additive `trait =` key
(legacy free-text `traits =` adjectives untouched), unknown ids demote at
spawn with a logged reason (stale-look rule). traits/grumpy.trait is the
format spec. Ratings: JSONL under saves/ratings/, human-curated —
rating a reply must provably NOT change the live prompt (test byte
equality).

## Group turn rules
Player + ≤3 NPCs. Addressed-by-name (leading name, full-name wins ties,
no-match = unaddressed) else round-robin. NPC↔NPC consecutive turns cap
at 2 (GroupSession::kMaxConsecutiveNpcTurns), then the floor returns to
the player. Turns strictly sequential over the single-flight LlmClient —
one streamed call per turn; log per-turn ms for the report. Solo
DialogueSession stays byte-identical (do not refactor it into
GroupSession).

## World-gen validator discipline
The validator IS the feature: one entrance to the load path, through
validateMap/validateCast. Error strings must be specific enough to be
retry feedback verbatim ("unknown part id 'hair_dreads' (nearest:
hair_braids)"). Generation model chosen by measured schema-validity
(tools/bench_schema_models.py), never by label; validation logic lives in
C++ once — the bench calls it through a CLI, no Python re-implementation.

## Standing environment rules (same as every session)
Suite: `make -C tests test` (un-skip scaffold stubs in the implementing
commit). Screenshots: `caffeinate -u -t 3` then
`./build/cpp_game_with_llm_npcs --frames 90 shot.png --camera x z yaw
--hour 12` (PNG lands in CWD). Never `git add .` — explicit paths only;
docs/learning/, OVERNIGHT_REPORT.md, .claude/**, docs/qa/** stay
local-only. GitHub via PyGithub recipe. Draft PRs only, stacked; never
main.
