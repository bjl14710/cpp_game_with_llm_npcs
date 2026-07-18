# Plan: Follow Mechanic + Group (N-way) Conversations
Date: 2026-07-12 (autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
Conversations stop being strictly one-on-one: the player can ask an NPC to
follow them (the `NpcAction::Follow` movement behavior already exists —
asking just needs a player-initiated path subject to the NPC's mood/
persona), walk to another NPC, and open a GROUP conversation. The group is
a participant list over the same structured-JSON dialogue pipeline: each
turn, ONE participant's prompt is assembled (their persona + traits + their
own memory + their view of the facts + the shared transcript with speakers
labeled) and they reply. NPCs can answer each other — capped at 2
consecutive NPC-to-NPC turns before the floor returns to the player — and
everything writes memory and world-bus facts exactly like solo talk,
attributed to whoever said it.

## Goal
The player can bring Marge over to Officer Brooks and have one three-way
conversation where the two NPCs also react to each other — instead of two
disconnected one-on-ones.

## Out of Scope (this version)
- Ambient NPC↔NPC chatter without the player (brief item 4: include ONLY
  if it falls out of the group mechanism naturally; expectation is it
  won't quite — the trigger/overhear UI is its own feature — so it is
  listed as the follow-up, not forced).
- Groups larger than 4 total participants (player + 3 NPCs) — latency cap
  per the brief.
- Multiplayer group conversations (host-authoritative chat routing for
  guests is untouched; sandbox/guest scope stays solo).
- NPC-initiated group joins (an NPC walking up to join uninvited).
- Voice/interrupt mechanics — strict turn-based only.

## Affected Areas
- `src/core/Npc.{hpp,cpp}` — a player-initiated follow request:
  `requestFollow()` gated on mood/persona (hostile/fleeing/afraid NPCs
  refuse; police on duty refuse while arresting) returning accept/refuse +
  a canned short reply, OR (preferred, logged) reuse the LLM action path:
  the player says "follow me" in dialogue and the model emits the existing
  `follow` action keyword through propose-validate-commit — no new
  mechanic, willingness comes from the persona for free. The VALIDATE step
  gains the mood gate (won't follow if Hostile/Afraid). Follow movement
  itself already exists.
- New `src/core/GroupSession.{hpp,cpp}` (core, testable) — participant
  list (npc indices), speaker-labeled transcript, turn resolution:
  * Player message addressed to a NAME (leading "Marge," / "Officer…")
    → that NPC replies next; otherwise round-robin over participants.
  * After an NPC reply, the addressed-or-next NPC MAY be given the floor
    (NPC→NPC), consecutive NPC turns capped at 2, then the floor returns
    to the player (input unlocked).
  * Per-speaker prompt assembly: THAT NPC's persona/traits/memory/gossip +
    the transcript rendered with speaker names; the last other-speaker
    line is the user-turn input (same request/stream shape LlmClient
    already serves — one in-flight request at a time, sequential by
    design, so latency is one model call per turn, streamed).
- `src/core/DialogueSession.hpp` stays for solo; main dispatches to
  GroupSession when >1 participant is in range/party (decision: don't
  refactor DialogueSession into GroupSession — solo path stays
  byte-identical, group is additive; log it).
- `src/core/HostChatRouter` / request routing in `src/app/main.cpp` —
  routes by request id already; group turns tag which participant a reply
  belongs to (map requestId → {group, npcIndex}, mirroring pendingRoutes).
- Fact/memory writes (`main.cpp` close-of-conversation + fact extraction)
  — per participant: each NPC's memory summarizes THE TRANSCRIPT from
  their own perspective on close; fact extraction runs per reply with
  source = the speaking NPC (the bus API already takes a source name).
- `src/app/DialogUI.{hpp,cpp}` — transcript shows speaker names (it may
  already; verify), a party line ("Talking with: Marge, Dana"), and a
  turn indicator while an NPC-to-NPC exchange plays out.
- `src/app/main.cpp` — party tracking (which NPCs follow the player),
  T-near-an-NPC while ≥1 follower → open group with follower(s) + the
  faced NPC; latency per turn measured and logged to stderr for the
  report.
- Tests: new `tests/test_group_session.cpp` — addressing (name vs
  round-robin), NPC-consecutive-turn cap, transcript labeling, per-speaker
  prompt assembly (uses FakeBackend/fake client pattern from
  test_llm_client), fact attribution; follow-request validation gate in
  `tests/test_npc_action.cpp`.

## Implementation Order
1. **Follow via the existing action path** — mood gate in validation, plus
   docs: saying "follow me" works because the action system already does;
   tests for the gate. *Committable.*
2. **GroupSession core** — participants, labeled transcript, addressing,
   round-robin, NPC-turn cap; fully unit-tested against a fake client.
   *Committable.*
3. **Main-loop wiring** — party tracking, open-group flow, request
   routing, streaming into DialogUI with speaker names. *Committable.*
4. **Memory + facts per participant** — per-perspective summaries on
   close; per-reply fact attribution. *Committable.*
5. **Latency logging + report** — per-turn ms logged; observed numbers in
   OVERNIGHT_REPORT.md; ambient-chatter follow-up written up honestly.
   *Committable.*

## Acceptance Criteria
- [ ] "Follow me" in dialogue makes a willing NPC follow (existing
      movement); a Hostile/Afraid NPC refuses in-character (validation
      gate test).
- [ ] With a follower, talking to a second NPC opens ONE session listing
      both; the transcript labels every line with its speaker.
- [ ] "Marge, what do you think?" gives Marge the next turn; an
      unaddressed message round-robins (unit tests).
- [ ] An NPC reply can be answered by another NPC at most 2 consecutive
      times before input returns to the player (unit test).
- [ ] Each participant's persisted memory after the conversation is
      written from their own perspective; facts revealed by NPC X land on
      the bus attributed to X (tests).
- [ ] Solo dialogue behavior is byte-identical (existing tests untouched
      and green).
- [ ] `make -C tests test` green; game builds; observed per-turn latency
      recorded in OVERNIGHT_REPORT.md.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| A participant dies/arrested mid-conversation | Removed from participants; a line notes them leaving; session continues or closes if <2 remain. |
| Player walks out of range | Same as solo: conversation closes; each participant summarizes. |
| Addressed name matches nobody in the group | Treated as unaddressed (round-robin) — no error. |
| Two participants share a first name | First match in participant order wins; full-name match preferred (documented). |
| Model reply while another request is streaming | Turns are strictly sequential — next request only after the current reply completes (existing single-flight client behavior). |
| Follower NPC's schedule fires | Following suppresses schedule movement (Follow already overrides movement); schedule resumes when dismissed ("stay"/"stop" action — exists). |
| Group full (3 NPCs) | Follow requests refused with a canned line; logged decision, cap = brief's latency guidance. |

## Open Questions
None blocking — decisions logged: follow rides the existing action
pipeline (no parallel mechanic), DialogueSession untouched (group is
additive), NPC-turn cap = 2, group cap = player + 3, ambient chatter =
follow-up unless it genuinely falls out of step 3.

## Suggested GitHub Issues
1. **feat(npc): mood-gated follow via the existing action pipeline** — validation gate + tests. (Concept: *player intent through the same propose-validate-commit door*.)
2. **feat(dialogue): GroupSession — participants, addressing, turn caps** — core + unit tests vs fake client. (Concept: *turn-taking as testable core state*.)
3. **feat(app): group conversation wiring — party, routing, labeled UI** — main + DialogUI. (Concept: *request-id routing with per-speaker fan-in*.)
4. **feat(memory): per-perspective summaries and attributed facts in groups** — close-path + bus writes. (Concept: *one transcript, N private memories*.)
5. **chore(report): latency measurements + ambient-chatter follow-up write-up** — numbers in the report. (Concept: *latency budgeting for multi-agent LLM turns*.)
