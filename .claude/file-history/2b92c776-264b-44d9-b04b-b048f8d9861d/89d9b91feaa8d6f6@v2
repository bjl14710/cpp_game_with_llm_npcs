# Plan: LLM World Generation — Maps and Casts from Natural Language
Date: 2026-07-12 (autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: BLOCKED — depends on the sandbox map editor (not yet planned/built)
Estimated complexity: L (on top of its dependencies)

## ⚠ Dependencies (read first)
1. **Sandbox map editor** — its map JSON save format IS this feature's
   output contract for maps. That /idea was interrupted before its plan
   was written; no `.claude/plans/sandbox-map-editor.md` exists yet.
   **This plan cannot start (map half) until that plan+implementation
   land.** Re-run `/idea` for the sandbox editor to unblock.
2. **Trait system** (`.claude/plans/npc-traits-and-ratings.md`) — trait
   ids are part of the character output contract. The character half can
   proceed with looks+personas only if traits aren't in yet (degraded,
   logged), but the full contract wants both.
The CHARACTER-generation half (steps 2 of Scope) only needs existing
systems (persona format, look format, part catalog) + optionally traits,
so implementation MAY start there while the map editor lands.

## The Idea (one paragraph)
"Make me a fishing village with a grumpy old sailor and two kids" — a
local LLM turns that into content by EMITTING STRUCTURED SPECS, never by
placing anything itself: a map file in the sandbox editor's own JSON
format (piece ids from the piece catalog, positions on the grid) and
character records in the existing persona/look/trait formats (part ids
from the shared catalog, trait ids from the trait library). A validation
layer — the deliverable as much as the generation — checks every id,
bound, and collision rule; invalid output goes back to the model with the
validation errors (capped retries), and results load as an EDITABLE
sandbox map so the player fixes by hand what the model got wrong. Same
schema-as-contract discipline as NPC dialogue (lesson 0022).

## Goal
The player types a description in sandbox mode and gets a playable,
editable map populated with in-character NPCs — without touching the
piece palette or creator by hand.

## Out of Scope (this version)
- The sandbox editor itself (dependency #1).
- Model TRAINING of any kind; this is generation + validation.
- Generating new PARTS/PIECES/TRAITS (the model composes existing ids
  only — the catalogs are the vocabulary, closed-world by design).
- Cloud billing/keys setup (cloud-first validation only "if configured" —
  the OpenAI-compatible backend lives on branch
  8-pluggable-llm-and-one-click, NOT in this stack; if it isn't reachable,
  validate on the local model and log that cloud was skipped. Decision
  logged, per the brief's "if configured").
- Streaming partial map previews (generate → validate → load, atomically).

## Affected Areas
- New `src/core/WorldGen.{hpp,cpp}` — prompt builders (description +
  piece catalog digest + part/trait catalog digest + JSON schema + rules)
  and response handling; `parseGeneratedMap`, `parseGeneratedCast`.
- New `src/core/WorldGenValidate.{hpp,cpp}` — THE deliverable:
  * map: every piece id exists; every placement in bounds and on-grid; no
    footprint overlaps; NPC positions walkable (not inside a collider).
  * cast: persona parses via parsePersonaText (the existing parser IS the
    validator for personas); look validates via lookIsValid; trait ids
    exist; names non-empty and unique in the batch.
  * Returns a LIST of specific errors (id + reason + location) — these
    are the retry feedback verbatim.
- `src/core/LlmClient` usage — a second request kind ("internal"
  generation like summaries/facts today: non-streaming, routed by request
  id). No client changes expected (internal path exists).
- New `tools/bench_schema_models.py` — mirrors bench_npc_models.py:
  N generation prompts × M models (current dialogue model, user-suggested
  coding model "Ornith" if pulled, any other local candidates) →
  schema-validity rate, retry-to-valid rate, latency. **Model picked by
  measured validity, not by label** (brief); result + numbers to
  OVERNIGHT_REPORT.md and bench/REPORT.md.
- Sandbox UI (depends on editor) — a "Generate…" entry in the sandbox
  menu: text input → progress line → on success load as editable map; on
  failure after retries, a plain toast with the top validation errors.
- Tests: `tests/test_worldgen_validate.cpp` — the validator exhaustively
  (bad ids, out of bounds, overlaps, NPC-in-wall, duplicate names, valid
  fixtures pass); `tests/test_worldgen_parse.cpp` — parsing tolerates
  markdown fences/prose around JSON (models do this), missing optional
  fields default, malformed JSON = named error. All offline/fake — no
  model calls in tests.

## Implementation Order
1. **Validator first** (map + cast, against fixtures) — it defines the
   contract everything else serves. *Committable now for cast; map part
   lands with the editor's format.*
2. **Cast generation** — prompt template + parse + validate + retry loop
   (cap 3, errors fed back verbatim); CLI hook in persona_prompt or a
   small tool for headless testing. *Committable (needs traits optionally).*
3. **Schema benchmark** — bench_schema_models.py over available local
   models (+ cloud first if the pluggable backend is configured);
   pick the generation model by validity rate; wire the choice into
   config (e.g. `worldgen_model` key in llm.cfg, defaulting to the
   dialogue model). *Committable.*
4. **Map generation** — (BLOCKED on editor) prompt with piece-catalog
   digest; validate; retry; load path. *Committable after editor.*
5. **Combined flow** — one description → map + cast + placements (cast
   members become placed NPCs in the map file). *Committable.*
6. **In-game entry** — sandbox menu "Generate…" text flow; results always
   land in EDIT mode. *Committable.*

## Acceptance Criteria
- [ ] Validator rejects each failure class with a specific, actionable
      message (unit-tested per class), and accepts known-good fixtures.
- [ ] Generated cast members load through the EXISTING loaders
      (parsePersonaText/lookIsValid) — no generation-only code paths in
      the load direction.
- [ ] Invalid model output triggers ≤3 retries with the validator's
      errors included in the retry prompt; after that, a plain failure
      report (no partial loads, ever).
- [ ] bench_schema_models.py produces validity rates for ≥2 models; the
      configured generation model is the measured winner; numbers in
      OVERNIGHT_REPORT.md.
- [ ] "fishing village + grumpy sailor + two kids" (or equivalent) end to
      end: map loads in the editor, NPCs stand in it with in-character
      personas/looks, everything hand-editable.
- [ ] Nothing loads without passing validation (grep-level check: the
      load path has exactly one entrance, through the validator).
- [ ] `make -C tests test` green (all worldgen tests offline).

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Model wraps JSON in markdown fences/prose | Parser extracts the first balanced JSON object; fences tolerated (tested). |
| Model invents a piece/part/trait id | Validator names it + the nearest valid ids in the retry feedback; never silently substituted. |
| Placements overlap / out of bounds | Listed per-placement in errors; retry; never auto-nudged (the model fixes it or fails — auto-repair hides model quality, hurting the benchmark; logged decision). |
| All retries fail | Plain toast + full error list to stderr/report; nothing loads. |
| Description asks for more than caps (50 NPCs) | Prompt states caps; validator enforces; over-cap = validation error fed back. |
| Local model not pulled / server down | Same degradation as dialogue: named error, feature disabled this session. |
| Cloud backend unconfigured (main-line build) | Skip cloud validation leg, log it, proceed local-only (brief allows: "if configured"). |
| Traits not yet merged when cast gen lands | Cast generates persona+look only; trait field omitted from the schema until the dependency merges (logged). |

## Open Questions
- The sandbox map editor plan must be (re)written — its JSON format
  decisions (grid size, piece schema fields) become this feature's
  contract. Everything else: decisions logged above (retry cap 3, no
  auto-repair, closed-world catalogs, model-by-measurement).

## Suggested GitHub Issues
1. **feat(worldgen): validation layer for generated casts and maps** — the contract, exhaustively tested offline. (Concept: *validators as the real API of generative features*.)
2. **feat(worldgen): cast generation with error-feedback retries** — prompt → parse → validate → retry(≤3). (Concept: *closed-world schema prompting*.)
3. **chore(bench): schema-validity benchmark across local models** — pick the generation model by measurement. (Concept: *task-fit model selection by measured validity*.)
4. **feat(worldgen): map generation against the sandbox format** — BLOCKED on the editor. (Concept: *piece catalogs as a generation vocabulary*.)
5. **feat(worldgen): combined description → populated, editable map** — end-to-end flow + sandbox entry point. (Concept: *generate-validate-load pipelines*.)
