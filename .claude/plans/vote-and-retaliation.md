# Plan: The Vote and the Retaliation Rule
Date: 2026-08-07 (rewritten and amended 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

At the end of each match day the living players nominate one NPC as the killer
and must agree unanimously. Agree on the right one and they win. Agree on the
wrong one and the accused NPC dies — and in multiplayer **the player who
nominated them dies too**. Run out of days and the killer wins, with a reveal.

## Goal

A group can end a day by arguing over a name, commit to it together, and find
out immediately whether they were right.

## Decisions taken

| Question | Answer |
|---|---|
| How is the vote decided? | **Unanimous, or no accusation that day.** Ties become structurally impossible instead of needing a rule. |
| Who dies on a wrong accusation? | **The nominator** — in multiplayer. See the single-player amendment. |
| What can a dead player do? | **Spectate freely** — no NPC dialogue, no vote. Reuses the existing `AppMode::Dead`. |
| Day limit exhausted? | **Killer wins, and is revealed.** |
| Single-player difficulty | **A wrong accusation does NOT kill the player** (2026-08-08). See below. |

## Why unanimity, in one number

**`kMaxPlayers = 4`.** Four voters choosing among ~20 suspects means a 1-1-1-1
split is the *normal* outcome of plurality, not an edge case — most days would
resolve to "tie" anyway. Unanimity deletes the entire tie branch, turns the vote
into a negotiation, and **degrades perfectly to single-player**: one living
player is unanimous by definition, so the same code path serves both modes.

## The single-player amendment (2026-08-08)

In single-player the nominator is the only player, so the multiplayer
retaliation rule ends the match on the first wrong guess — one attempt.

**So in single-player a wrong accusation costs the day and the accused NPC's
life, but not the player's.** Multiplayer keeps the harsher rule.

Note what this does *not* require: **no attempt counter.** One vote per day with
`dayLimit = 3` already *is* three attempts, tunable to four through the existing
match setting. The rule change is the whole feature.

Concretely: `VoteOutcome::Wrong` in single-player kills the accused NPC, leaves
the player alive, and rolls the day over. The match still ends when the day
limit is exhausted (killer wins) or the guess is right (player wins).

## Key facts

- `kMaxPlayers = 4` (host plus three remote).
- **`AppMode::Dead` already exists** with its own render and input branches,
  entered when `world.player().hp <= 0`. Spectating extends it.
- `Player` has `hp`, `hpMax`, `alive()`; the header says "0 means dead → game
  over".
- **The wire protocol has no vote messages and no player-to-player chat.**
- `NpcState::Dead` already renders a corpse — killing the accused needs no new
  rendering.
- The murder plan exposes `voteIsCorrect(setup, accused)` as the **only**
  sanctioned read of the killer.

## The shared contract with the murder setup

Exactly one function: `bool voteIsCorrect(const MysterySetup&, const std::string&)`.

Everything else stays host-side. Resolution runs **on the host only**; clients
send a nomination and a confirmation and never receive the killer's identity
until the match ends. `MysterySetup` is never serialized. The reveal sends **one
name in one message**, after the outcome is decided.

## Out of Scope

- Cutscenes for the wrong accusation or the win. This fires the outcome events.
- Player-to-player chat, for dead or living players.
- Networked match lifecycle — lobby, ready-up, join rules.
- The mystery generation.
- Making the killer lie or resist accusation.
- Rematch, scoring, or persistence.

## Design

```cpp
enum class VoteOutcome { NoAccusation, Correct, Wrong };

struct Ballot {
    std::string nominee;                 // NPC persona name; empty = none
    int nominator = -1;                  // player id who proposed
    std::map<int, bool> confirmations;
};

struct VoteResult {
    VoteOutcome outcome = VoteOutcome::NoAccusation;
    std::string accused;
    int nominator = -1;
    int playerKilled = -1;               // -1 unless Wrong AND multiplayer
};

// Pure. Takes the verdict as a bool rather than the setup, so the killer is
// not in scope here and there is exactly one place voteIsCorrect is consulted.
VoteResult resolveVote(const Ballot& ballot,
                       const std::vector<int>& livingPlayers,
                       bool nomineeIsKiller,
                       bool singlePlayer);
```

New message types: `VoteOpen`, `VoteNominate`, `VoteState`, `VoteConfirm`,
`VoteResolved`, and `MatchOver` — the **only** message that ever carries the
killer's name.

## Implementation Order

1. **`resolveVote` + tests.** Pure, no networking, no mystery, no UI. Every
   unanimity, nomination and death rule is decided and pinned here, including the
   single-player amendment.
2. Vote message types + framing round-trip tests.
3. Host-side ballot state in `NetServer`. The single `voteIsCorrect` call lives
   here and nowhere else.
4. Client send/receive and the ballot UI.
5. Apply consequences — accused NPC → `NpcState::Dead`; nominator → `hp = 0` in
   multiplayer only.
6. Spectator mode on the existing `AppMode::Dead`.
7. Match end and reveal.

## Acceptance Criteria

- [ ] All living players confirm the same nominee → `Correct` or `Wrong`, never
      `NoAccusation`.
- [ ] Any living player has not confirmed → `NoAccusation`, nobody dies.
- [ ] A single living player who nominates and confirms resolves — unanimity must
      not deadlock single-player.
- [ ] **Single-player `Wrong`: the accused NPC dies, the player survives, the day
      rolls over.**
- [ ] **Multiplayer `Wrong`: the accused NPC dies AND the nominator dies**, and
      no other player is harmed.
- [ ] Multiplayer `Wrong` where the nominator has since died → a living player is
      chosen deterministically (lowest living id) and the match does not stall.
- [ ] `Correct` ends the match immediately, before the next day.
- [ ] Day limit exhausted → `MatchOver` reveals the killer; players lose.
- [ ] **No message sent to any client contains the killer's identity before
      `MatchOver`** — asserted by scanning encoded payloads in a loopback test.
      This is the security property of the whole mode.
- [ ] A dead player receives snapshots, cannot open dialogue, and is excluded
      from the unanimity count.
- [ ] All players disconnect → the match ends rather than hanging.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Nobody nominates | `NoAccusation`; day rolls over |
| A second player nominates someone else | Replaces the nomination and **clears all confirmations** — agreeing to one name must never silently carry to another |
| The nominee is already dead | Rejected at nomination |
| Nominator dies before resolution | Lowest living player id. Never random — a random pick is untestable |
| A client sends a nomination outside the Vote phase | Ignored with a logged warning; clients are never trusted to know the phase |
| A client sends a non-roster nominee | Rejected. Never use a client-supplied name as a lookup key |
| Phase ends mid-confirmation | The clock is authoritative; whatever state the ballot is in resolves |

## Open Questions

1. Should the vote UI show who confirmed? Visible makes it a negotiation; hidden
   makes it a commitment. I lean visible.
2. Can a player change their confirmation before the phase ends? Assumed yes.
3. Should a wrong accusation state that the accused was innocent? Implied by the
   retaliation, but an explicit line lands harder.
4. Is single-player's default `dayLimit` 3 or 4? The user said "3 or 4"; it is a
   match setting either way, so this is only a default.
