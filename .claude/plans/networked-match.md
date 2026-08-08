# Plan: Detective Mode as a Networked Match
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued. Build LAST of the mystery set.
Estimated complexity: L

## The Idea

Turn free-roam multiplayer into a match: a lobby, a fixed roster once it starts,
a phase clock every client agrees on, and a shared vote. The transport already
exists and is in good shape. What is missing is everything match-shaped.

## Goal

Four people can sit in a lobby, start a three-day mystery together, see the same
day tick over at the same moment, and vote as a group — with no client ever
holding the answer.

## Decisions taken

| Question | Answer |
|---|---|
| Can the host cheat? | **Yes, and the plan says so.** The host is trusted. Anti-cheat effort goes entirely into making sure no *non-host* client can obtain the answer. |
| Late join? | **Refused with a clear reason.** Fixed roster from match start; the lobby is where you get in. |
| How does a match start? | Opening cutscene, then interrogation — same as single-player (2026-08-08). |

## What already exists, and is good

- Host-authoritative transport: `NetServer` accepts up to `kMaxPlayers - 1`
  remote players, `NetFraming` is length-prefixed, `test_net_loopback.cpp`
  exercises server+client together.
- **`HostChatRouter` already gets the hard part right.** Its comment: *"Replies
  stream back to the requesting player only; everyone gets NpcMoodUpdate +
  NpcSpeechBubble so bystanders see the conversation happen."* Reply text is
  already per-player, and the host owns the single `World` and `LlmClient`, so
  **system prompts and role blocks never leave the host process at all.**
- `messageTypeFromString` returns `nullopt` for unknown names *"so a newer peer's
  messages fail loudly at decode instead of desyncing"* — new types inherit that.
- `Welcome` already carries a refusal reason, so refusing a late join needs no
  new field.

## The three leaks, ranked

**1. Mood broadcast — measured, and worse here than in solo play.** The role-layer
probe measured that a guilty NPC emits `[[MOOD: angry]]` on turn one while an
innocent stays warm. `NpcMoodUpdate` is **broadcast to every client**, so a player
need not even do the interrogating — they can watch faces across the plaza while
someone else asks. Same leak, wider blast radius.

The fix belongs to the role layer's `secret_keeper` roles. **But this plan must
not make it worse**, and its leak test must cover the mood stream, not just
payload contents.

**2. Ground truth in replicated state.** `MysterySetup` must never enter
`WorldSnapshot`. The day-phases plan already anticipates this: keep match state to
`day`, `phase`, `phaseRemainingSeconds` — three plain numbers — and add exactly
those. Nothing else.

**3. The host's own memory.** Unfixable while the host plays. Accepted and
documented. Consequence: **the murder plan's compile-gated debug reveal must be
off in any build that can host a networked match**, or host advantage goes from
theoretical to one keypress.

## Out of Scope

- Dedicated server mode. The only real fix for host trust, and separate, larger
  work.
- Reconnect. A disconnect is final for the match.
- Matchmaking, public servers, NAT traversal. Direct connect only.
- Fixing the mood leak — the role layer's job.
- Spectators who were never in the match. Refused at the door.
- Persisting match results.
- Voice or text chat between players.

## Design

```
Lobby ──(host starts)──> InMatch ──(win / loss / all gone)──> Ended ──> Lobby
```

The server owns this. Clients render what they are told and never infer the phase.

| Type | Direction | Purpose |
|---|---|---|
| `LobbyState` | server → all | Who is connected, who is ready, match settings |
| `LobbyReady` | client → server | Ready / not ready |
| `MatchStart` | server → all | Roster, day limit, day length |
| `MatchPhase` | server → all | `day`, `phase`, `phaseRemainingSeconds` |
| `PlayerDied` | server → all | A player died, and why |

Plus the vote types from the vote plan.

**Phase replication:** the host runs the one `MatchClock`. Clients **do not** run
their own — they render `MatchPhase`, sent on every transition *and* periodically
(~1 Hz) so a client that missed a packet resynchronises with no special recovery
path. Transitions are events; the periodic send is a correction.

**Disconnect handling** (decided in the vote plan; implemented here):

- A disconnecting player leaves `livingPlayers` immediately; unanimity is
  recomputed against who remains.
- **If the nominator disconnects before resolution, retaliation falls on the
  lowest living player id** — deterministic, never random, so it is testable.
- If every player disconnects, the match ends rather than running empty.

## The leak test

A loopback test that plays a scripted match end to end and **captures every byte
sent to a non-host client**, then asserts:

- No payload contains the killer's persona name in any field, at any point before
  `MatchOver`.
- `WorldSnapshot` contains no mystery fields — asserted **structurally**, so
  adding one later fails the test rather than silently shipping the answer.
- The mood-update stream is captured too, so a later change that starts
  broadcasting something guilt-correlated shows up here.

Payload capture rather than eyeballing encoders is the point: the property is
"the answer never crossed the boundary", and only inspecting the boundary proves
it.

## Implementation Order

1. **`MatchSession` + tests.** Pure lifecycle: lobby membership, ready state,
   start conditions, disconnect rules, end conditions. No networking, no UI.
   Every disconnect edge case is decided and pinned here.
2. Lobby message types and the lobby UI.
3. **Refuse late joins** with a reason in `Welcome`. Small, and it closes the
   roster question before anything depends on it.
4. **Phase replication.** `MatchPhase` on transition plus periodic correction;
   clients stop running a clock.
5. Wire the vote through the session — living players come from `MatchSession`,
   not a raw connection list.
6. **The leak test.** The acceptance gate for the whole plan.

## Acceptance Criteria

- [ ] Four players in a lobby all receive the same `MatchStart` roster and
      settings.
- [ ] A fifth connecting mid-match gets a `Welcome` refusal with a readable
      reason and is disconnected.
- [ ] Every client's rendered day and phase match the host's within one periodic
      correction.
- [ ] A client that misses a `MatchPhase` resynchronises on the next periodic
      send, with no special recovery path.
- [ ] **A client never runs its own `MatchClock`** — asserted by the client
      having no clock instance at all, not by comparing values.
- [ ] Nominator disconnects before resolution → lowest living player id; the vote
      still resolves.
- [ ] All players disconnect → match ends, server returns to lobby.
- [ ] **No byte sent to any non-host client contains the killer's name before
      `MatchOver`.**
- [ ] `WorldSnapshot` contains `day`, `phase`, `phaseRemainingSeconds` and no
      other match state.
- [ ] A shipped build capable of hosting has the debug reveal compiled out.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Host disconnects | The match ends for everyone. No host migration |
| Player disconnects during Vote | Dropped from `livingPlayers`; unanimity recomputed |
| Client sends `VoteNominate` outside the Vote phase | Ignored with a logged warning |
| Client sends a non-roster nominee | Rejected. Never a lookup key |
| Clock drift | Only the host's clock is authoritative; clients never extrapolate past the value given |
| An older client connects | `messageTypeFromString` returns `nullopt`; fails loudly at decode. Existing behaviour, inherited |
| Map sync and match start race | Map transfer happens in the lobby, never mid-match |

## Open Questions

1. Does the lobby let players pick settings, or only the host? Host-only is
   simpler and matches "host presses Host Game" today.
2. Should the periodic `MatchPhase` rate be tied to the snapshot rate? Probably —
   one timer is easier to reason about than two.
3. What does a client render during `Ended` before returning to lobby? The reveal
   needs a beat to land.
