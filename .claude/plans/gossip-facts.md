# Plan: Gossip — structured facts on the world bus, with propagation
Date: 2026-07-06 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
NPCs learn structured facts from conversations with the player and from
each other. Facts are normalized records — {fact_id, subject, content,
source, world_time_learned} — living on the SAME WorldState bus the world
clock rides (shipping tonight in feature/world-time), with a per-agent
knowledge set saying who has heard what. After a conversation closes, the
NPC's model PROPOSES facts as structured JSON; code validates and commits
them (never a direct write). NPC prompts inject the facts that NPC knows.
A conservative propagation tick spreads facts between NPCs who are
physically near each other (schedules make them actually meet), gated by
elapsed world time and chance — tell the baker a secret and it reaches the
hardware store owner plausibly, not instantly.

## Goal
The player tells one NPC something, plays on, and later hears a different
NPC bring it up — with the spread visibly gradual and traceable to who
told whom.

## Autonomy decisions (most-defensible picks, logged)
- **Facts are first-class on the bus**: WorldState (from feature/world-time,
  in flight on this chain — prerequisite satisfied) gains a structured
  fact section: `KnownFact {factId, subject, content, source,
  learnedAtSeconds}` plus knowledge tracking (`grantKnowledge(agent,
  factId)`, `knows`, `factsKnownBy`). Agents are persona names + "player".
  Subjects are normalized (lowercase, [a-z0-9_]) because the journal's
  contradiction check compares them exactly.
- **Propose → validate → commit**: on conversation close (the same hook
  that already requests memory summaries), a second LLM request asks for
  0–2 facts as a strict JSON array [{subject, content, direction}],
  direction ∈ {npc_learned, player_learned}. Code parses with
  non-throwing nlohmann, validates (normalized subject, content ≤ 140
  chars, ≤ 2 facts, direction known), and only then commits: npc_learned
  → knower npc, source "player"; player_learned → knowers npc + player,
  source npc name. Malformed output is dropped with a stderr note —
  facts are a bonus, never a crash.
- **Read path**: `Persona::renderSystemPrompt(memory, gossip)` gains the
  gossip block ("Things you have heard around town: …"), injected before
  the ACTION protocol like memory; Npc carries a gossip string refreshed
  by main whenever that NPC's knowledge set changes. No prompt work at
  request time beyond string concat.
- **Conservative propagation** (per the instruction): a tick every 15
  real seconds considers NPC pairs within 6 world units; a fact transfers
  only if the teller has known it > 30 game minutes, the hearer doesn't
  know it, and a rng roll passes 35% — at most ONE fact per pair per
  tick. Content never mutates in transit (no telephone-game distortion
  v1). Pure function `propagateGossip(state, agentsAt, nowSeconds, rng)`
  in core so tests drive it with fixed rolls and positions.
- **No NPC→NPC message passing**: propagation only flips knowledge bits
  in the shared store (the journal reads the same store with zero new
  wiring) — enforced by there being no other channel at all.
- **Persistence**: facts + knowledge persist in saves/facts.sqlite3 via a
  FactStore following ConversationStore's exact pattern (two tables:
  facts, knowledge). A journal wiped by every restart would be useless;
  the pattern is established and cheap. Loaded at startup, saved on
  commit/propagation (upserts are tiny).
- **Multiplayer**: facts are host-local (guests' chats route through the
  host's NPCs already via HostChatRouter, so host-side commit covers
  them); replication of the knowledge sets is deferred with the protocol
  bump. Logged limitation.
- **Branch**: `feature/gossip-facts` stacked on `feature/world-time`.
  One draft PR closing this plan's tickets.

## Out of Scope (this version)
Journal UI (next plan, reads this store); fact mutation/distortion in
transit; forgetting/decay; NPC-initiated lies; affinity; semantic dedup
of near-identical subjects (exact subject match only); replication.

## Affected Areas
- `src/core/WorldState.{hpp,cpp}` — KnownFact section + knowledge sets.
- NEW `src/core/Gossip.{hpp,cpp}` — validation (normalizeSubject,
  validateProposedFacts JSON→records) + propagateGossip.
- NEW `src/core/FactStore.{hpp,cpp}` — SQLite persistence (facts,
  knowledge), ConversationStore pattern.
- `src/core/Persona.hpp` — renderSystemPrompt gossip block.
- `src/core/Npc.{hpp,cpp}` — gossip_ string + setGossip/gossip.
- `src/app/main.cpp` — fact-extraction request on dialogue close +
  factRoutes drain (propose→validate→commit), propagation tick, gossip
  string refresh, FactStore load/save wiring.
- Tests: NEW test_gossip.cpp (validation accepts/rejects, propagation
  radius/age/chance/one-per-pair, subject normalization), NEW
  test_fact_store.cpp (round-trip, degradation); extend test_persona.cpp
  (gossip block renders before ACTION protocol, empty = identical).

## Implementation Order
1. WorldState fact section + Gossip validation/propagation + tests.
2. FactStore + tests.
3. Persona/Npc read path + tests.
4. main wiring (extraction request, routes, tick, refresh, persistence).
5. Live sanity run (tell an NPC a fact, check store contents), suite,
   screenshots not needed (no visuals), PR, report.

## Acceptance Criteria
- [ ] Valid proposal JSON commits normalized facts with the right
      knowers/source; malformed JSON, bad directions, oversize content,
      and >2 facts are rejected without side effects. Tests green.
- [ ] Propagation: no transfer beyond 6 units, before 30 game minutes of
      teller knowledge, or on a failed roll; at most one fact per pair
      per tick; deterministic under a seeded rng. Tests green.
- [ ] An NPC's prompt contains exactly the facts that NPC knows; an NPC
      with no facts renders an unchanged prompt. Tests green.
- [ ] Facts and knowledge survive a store round-trip; unopenable DB
      degrades to session-only gossip. Tests green.
- [ ] Full suite green; game builds and smoke-runs.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| LLM returns prose instead of JSON | Dropped with stderr note; no commit. |
| Duplicate subject+content proposed again | Same fact_id (hash of subject+content) — commit is an idempotent no-op beyond knowledge grant. |
| NPC hears a fact it already knows | grantKnowledge no-op. |
| Fact about a subject nobody defined | Fine — subjects are free-form normalized keys, not an enum. |
| Store unopenable | ok()==false; gossip works this session, not persisted. |
| Dead NPC in a propagation pair | Skipped (the dead don't gossip). |

## Open Questions
None — decisions above were made under the autonomy instructions.

## Suggested GitHub Issues
1. feat(core): structured facts + knowledge sets on the WorldState bus
2. feat(core): propose→validate→commit fact extraction + conservative
   propagation
3. feat(core): FactStore persistence for facts and knowledge
4. feat(game): wire extraction, propagation tick, and prompt injection
