# Plan: NPC Trait System + Response Rating Loop
Date: 2026-07-12 (autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
Personality traits become first-class DATA — a trait is a named bundle of
behavior rules and few-shot example exchanges, stored as human-editable
files — composed into an NPC's system prompt in a defined order alongside
backstory, memory, and gossip. Any character (shipped persona or
player-created) can carry multiple traits. A lightweight in-conversation
rating loop (mark a reply good/bad) grows each trait's example candidates
from replies the player actually liked and logs disliked ones for review —
all human-curated, nothing auto-promoted into live prompts. This is
prompt-conditioning by design, not fine-tuning; the escalation path to a
LoRA is documented, not built.

## Goal
The player attaches named traits (grumpy, poetic, gossipy…) to any
character and sees them consistently shape replies — and can rate replies
in-game so the trait library improves under their curation.

## Out of Scope (this version)
- Any model training/fine-tuning (LoRA path is DOCUMENTED only, step 6).
- Auto-promotion of rated lines into live prompts (explicit brief rule —
  candidates land in a review file the human curates into trait files).
- Trait-driven BEHAVIOR changes outside dialogue (no movement/schedule
  effects; traits condition the prompt only).
- A trait-authoring UI (traits are files; the creator only PICKS them).
- Retrofitting the ten shipped personas' free-text adjective `traits =`
  key away — it stays as flavor text; new structured traits are additive
  via a new `trait =` key (decision logged below).

## Affected Areas
- New `src/core/Trait.hpp/.cpp` — TraitDef {id, name, behaviorRules[],
  exampleExchanges[{userLine, npcLine}]}; `loadAllTraits(dir)` parsing
  `traits/*.trait` (same key=value style as .persona: `name =`, repeated
  `rule =`, repeated `example_user =` / `example_npc =` pairs);
  `renderTraitBlock(...)` + `renderTraitReinforcement(...)`.
- New `traits/*.trait` — 6-8 starter traits (grumpy, cheerful, poetic,
  gossipy, taciturn, salesman, nervous, deadpan), each with 2-3 rules and
  1-2 curated example exchanges.
- `src/core/Persona.hpp` — `std::vector<std::string> traitIds;` +
  renderSystemPrompt composes: identity/backstory → trait RULES + examples
  → knowledge/gossip → memory summary → **trait reinforcement AFTER
  memory** (brief item 4: a 1-2 line re-assertion so long-conversation
  drift doesn't dilute traits). Persona.hpp is header-only today —
  rendering needs the trait registry; decision: renderSystemPrompt gains a
  `const std::vector<TraitDef>&` default-empty parameter threaded from the
  call sites (Npc holds a pointer to the loaded trait registry), keeping
  Persona free of file I/O.
- `src/core/PersonaLoader.{hpp,cpp}` — repeated `trait = <id>` header key
  (unknown ids demote at spawn with a logged reason — same rule as looks);
  renderPersonaText round-trips.
- `src/core/Npc.{hpp,cpp}` — carries the trait registry reference into
  prompt rendering; exposes activeTraitIds() for the rating capture.
- `src/app/main.cpp` — load traits at startup; creator hooks pass chosen
  trait ids; rating-key handling in Dialogue mode.
- `src/app/Menu.{hpp,cpp}` — creator page: a trait row (cycle/toggle up to
  3 traits) in both NPC and avatar-adjacent flows; editing an EXISTING
  character = load its persona text into the creator (follow-up if heavy —
  minimum: traits pickable at creation; log if edit-existing is deferred).
- `src/app/DialogUI.{hpp,cpp}` — after a reply completes: hint line
  "[+] like  [-] note" (unobtrusive, only while the last message is an NPC
  reply); key handling reported to main.
- New `src/core/RatingLog.{hpp,cpp}` — appends JSONL rows to
  `saves/ratings/candidates.jsonl` (good: trait ids, persona id, player
  line, npc reply, timestamp) and `saves/ratings/rejected.jsonl` (bad: same
  + full prompt context reference). saves/ is already gitignored.
- New `docs/design/TRAIT_FINE_TUNE_PATH.md` — the documented (NOT built)
  escalation: what accumulates (rated exchange pairs keyed by trait +
  persona), how it would become LoRA training rows, and the measured signal
  (trait-adherence failures despite conditioning) that would justify it.
- Tests: new `tests/test_traits.cpp` (parse/round-trip .trait files,
  composition ORDER assertions — rules before memory, reinforcement after
  memory — unknown-trait demotion, rating-log append format);
  `tests/test_persona.cpp`/`test_persona_look.cpp` style follow-ons for the
  `trait =` key round-trip.

## Implementation Order
1. **Trait core** — TraitDef + loader + starter trait files + parse/
   round-trip tests. *Committable.*
2. **Prompt composition** — Persona.traitIds + ordered assembly with
   post-memory reinforcement; order pinned by string-index tests; persona
   `trait =` key + round-trip. *Committable.*
3. **Creator integration** — trait row on the creator page; chosen ids
   flow through renderPersonaText into the store; spawn path resolves ids
   (unknown → demote + log). *Committable.*
4. **Rating capture** — DialogUI affordance + RatingLog JSONL writers +
   tests; docs note in the dialogue flow describing curation (copy a
   candidate's exchange into the trait file to promote it). *Committable.*
5. **Prompt-order check** — already pinned in step 2 tests; this step is
   the explicit acceptance sweep + a `persona_prompt` CLI flag to dump the
   assembled prompt for eyeball verification. *Committable.*
6. **Escalation doc** — TRAIT_FINE_TUNE_PATH.md. *Committable.*

## Acceptance Criteria
- [ ] `traits/*.trait` files parse; malformed files are skipped with named
      errors (loader mirrors loadAllPersonas).
- [ ] A persona with `trait = grumpy` renders a system prompt containing,
      in order: backstory → grumpy rules → grumpy examples → memory →
      grumpy reinforcement (string-index test).
- [ ] Multiple traits compose in persona-file order; >3 traits = parse
      error (cap keeps prompts bounded; logged decision).
- [ ] Creator can attach traits; created character's stored persona text
      round-trips them; spawned NPC's prompt shows them.
- [ ] In dialogue, rating a reply good appends a well-formed JSONL row to
      candidates.jsonl with the active trait ids; bad → rejected.jsonl.
      NOTHING changes in live prompts as a result (test: prompt identical
      before/after rating).
- [ ] Unknown trait id on a stored/shipped persona demotes with a logged
      reason; the NPC still spawns.
- [ ] `make -C tests test` green; game builds.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Persona names a trait with no .trait file | Demote that id at spawn, log, NPC spawns with remaining traits (same rule as stale looks). |
| Trait file with rules but no examples | Valid — examples optional; the block renders rules only. |
| Rating pressed with no completed reply | Ignored (no partial/streaming rows). |
| Rating during multi-trait persona | Candidate row records ALL active trait ids; curation decides which trait file it joins. |
| saves/ratings/ unwritable | Same degradation as stores: log once, ratings no-op this session. |
| Player rates the same reply twice | Second press ignored (one rating per reply). |
| Memory summary mentions contradicting behavior | Reinforcement block after memory is exactly the countermeasure — pinned by order test. |

## Open Questions
None blocking — decisions logged: additive `trait =` key (legacy `traits =`
adjectives untouched), registry threaded by reference (Persona stays
I/O-free), 3-trait cap, JSONL review files, edit-existing-character may
demote to follow-up if the creator surgery is heavy (logged if so).

## Suggested GitHub Issues
1. **feat(traits): trait files, loader, and starter library** — TraitDef + traits/ + tests. (Concept: *behavior as curated data files*.)
2. **feat(traits): ordered prompt composition with post-memory reinforcement** — persona trait ids + assembly order pinned by tests. (Concept: *prompt assembly order as a tested contract*.)
3. **feat(creator): trait picker on the creator page** — attach/round-trip/spawn. (Concept: *data-driven UI rows*.)
4. **feat(ratings): in-dialogue rating capture to curated review files** — DialogUI affordance + JSONL logs, no auto-promotion. (Concept: *human-in-the-loop few-shot curation*.)
5. **docs(traits): LoRA escalation path** — what accumulates, when it's justified. (Concept: *conditioning-vs-training decision criteria*.)
