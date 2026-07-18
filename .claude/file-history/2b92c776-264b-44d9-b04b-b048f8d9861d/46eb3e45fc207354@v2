# Plan: NPC Conversation Memory + Local Model Benchmark
Date: 2026-07-05 (overnight, autonomous — decisions logged per run instructions)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
NPCs forget everything when the process restarts. This adds per-NPC persistent
memory — a running first-person summary injected back into the system prompt,
plus the full transcript for debugging — stored in SQLite and surviving
restarts. Alongside it, a reproducible benchmark compares local Ollama models
(current qwen2.5:3b baseline vs qwen3:8b vs a Gemma vs a Mistral) against the
game's REAL reply contract, and the winner (by numbers) becomes the default
model in config/llm.cfg. Fully local; the C++ render-side contract is
untouched.

## Autonomy decisions (made without asking, per run instructions)
1. **Brief/reality mismatch**: the brief says "Gemma via Ollama with
   format:json emitting {dialogue, mood, gesture}". The repo actually runs
   `qwen2.5:3b-instruct` (config/llm.cfg) with plain-text replies carrying
   `[MOOD:]`/`[ACTION:]` directive tags parsed by `parseDirectives`
   (src/core/NpcAction.hpp). DECISION: benchmark the real tag contract —
   "directive-compliance rate" replaces "valid-JSON rate" — because
   requirement 3 ("no changes to the C++ contract") pins the tag protocol.
2. **Model roster**: qwen2.5:3b-instruct (baseline), qwen3:8b (default swap
   target per instructions), gemma3:4b (the Gemma representative). Mistral
   Small (22-24B) needs >13 GB resident and is substituted with
   mistral:7b-instruct unless the machine reports ≥32 GB RAM at bench time.
3. **Persona consistency scoring**: no cloud judges allowed (fully-local
   constraint), so consistency is proxy-scored by script: fourth-wall breaks
   (AI/language-model mentions), name/identity drift, sustained directive
   compliance over 20 turns, and reply-length conformance to the persona's
   speaking style. Logged as weaker than an LLM judge; adequate to rank.
4. **SQLite**: vendored single-file amalgamation in external/ (public
   domain), matching the repo's existing header-only vendoring pattern —
   macOS system sqlite3 headers exist but MinGW's don't, and the build must
   stay cross-platform with zero package managers.
5. **Summary generation**: the NPC's own model writes its memory — an
   internal LLM request ("summarize what you learned, 3 sentences, first
   person") fired when a conversation ends, stored per NPC, injected on the
   next launch as a "What you remember" section of the system prompt.
6. **Branch base**: feature/issue-38-raylib-characters (the current stack
   tip) so main.cpp hooks land on the raylib loop and the whole line merges
   linearly. Core pieces are renderer-agnostic regardless.

## Goal
Quit the game, relaunch, and the baker remembers your last conversation —
with the best-measured local model answering.

## Out of Scope (this version)
- Cloud models/judges (fully local requirement), JSON reply mode, changes to
  the delta/reply C++ interface, multiplayer replication of memories (host's
  world owns them — guests talk to the host's NPCs), cross-NPC gossip,
  vector/RAG memory, forgetting policies.

## Affected Areas
- `external/sqlite3.c`, `external/sqlite3.h` — vendored amalgamation (new)
- `src/core/ConversationStore.{hpp,cpp}` — SQLite-backed store (new):
  one table, `conversations(npc_id TEXT PRIMARY KEY, summary TEXT,
  transcript_json TEXT, updated_at TEXT)`
- `src/core/Npc.{hpp,cpp}` — optional memory summary injected into the
  system prompt; conversation-end hook exposing history for summarization
- `src/core/Persona.hpp` — renderSystemPrompt(memory) overload (additive)
- `src/app/main.cpp` — load summaries at startup, request summary + save on
  dialogue close, save transcripts on quit
- `tools/persona_prompt.cpp` — tiny CLI printing a persona's rendered system
  prompt (single source of truth for the bench harness) (new)
- `tools/bench_npc_models.py` — stdlib-only benchmark harness (new)
- `bench/` — gitignored output dir for run reports; committed README (new)
- `tests/test_conversation_store.cpp` — store round-trip/corruption tests (new)
- `CMakeLists.txt`, `tests/Makefile` glob picks up core files automatically;
  sqlite3.c needs adding to both core builds
- `config/llm.cfg` — model swapped to the benchmark winner

## Implementation Order
1. Vendor sqlite3 amalgamation; `ConversationStore` + doctest tests
   (round-trip, missing file, corrupt row, restart survival).
2. `persona_prompt` CLI + `bench_npc_models.py` (compliance/consistency/
   latency vs the real contract); kick off the model pulls + bench run in
   the background.
3. Memory injection: `renderSystemPrompt(memory)`, Npc summary plumbing,
   main.cpp load/save/summarize hooks.
4. Apply bench verdict to config/llm.cfg (+ bench report committed to
   bench/REPORT.md); update docs.

## Acceptance Criteria
- [ ] `make -C tests test` green including new ConversationStore tests.
- [ ] Talk to an NPC, quit, relaunch: the NPC's system prompt contains the
      remembered summary (observable via persona_prompt + saves/ inspection,
      and in conversation).
- [ ] saves/conversations.sqlite3 survives restart; transcripts inspectable
      via `sqlite3 ... 'select * from conversations'`.
- [ ] bench/REPORT.md contains per-model compliance %, consistency score,
      and latency (TTFT + total) with N stated, and names the winner.
- [ ] config/llm.cfg model equals the winner; the game runs with it.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| saves/ missing | Created on first save; load returns empty memory silently. |
| SQLite open/write fails | Log once, play continues memoryless (never crash). |
| Corrupt row / bad JSON transcript | Row ignored + logged; NPC starts fresh. |
| Summary request fails (Ollama down at quit) | Skip save of summary, keep transcript; retry next session end. |
| Model in llm.cfg not pulled | Existing behavior: Ollama error surfaces in dialogue; README documents `ollama pull`. |
| Bench: model won't pull/fit in RAM | Skipped with reason in REPORT.md; ranking proceeds over the rest. |

## Open Questions
None blocking — decisions above; revisit persona-consistency scoring with an
LLM judge when cloud access is in scope.

## Suggested GitHub Issues
1. feat(memory): SQLite ConversationStore with per-NPC summaries — steps 1,3
2. chore(bench): local model benchmark vs the directive contract — steps 2,4
