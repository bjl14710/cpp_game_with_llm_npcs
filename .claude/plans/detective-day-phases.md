# Plan: Detective Mode — Game-Day Phases
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: PARTLY SHIPPED — #154 done (PR #167); #155, #156, #157 open
Estimated complexity: M

## The Idea

Detective mode needs a match structure: numbered days, each split into a long
investigation phase and a short vote. A day/night cycle already exists —
`DayNight.hpp` has four bands with dawn/dusk blends, and personas carry
`schedule =` entries — but the game has **no concept of a day at all**:
`WorldState::advanceTime` is a pure `fmod` over 24 hours, so "day 2" is not
currently representable and the wrap is silent.

The clock relationship is **inverted** from free roam: instead of the world
clock advancing at a fixed 60× while everything reacts, the match owns pacing
and *drives* the clock.

## Goal

A player can start a 3-day match, watch the light go orange as time runs out on
day 1, see every surviving NPC walk to the plaza for the vote, then wake into
day 2.

## Key facts this is built on

- `kGameSecondsPerRealSecond = 60.0`, `kSecondsPerDay = 86400`,
  `kStartHour = 9.0`. A full in-world day is **24 real minutes** today, which is
  why a 3-day match on the existing scale would run 72+ minutes.
- `advanceTime` does `fmod(seconds, kSecondsPerDay)`. **No day counter.**
- `activeScheduleIndex` is a pure function of `hour`, so schedules keep working
  when the hour advances at match pace.
- `DayNight.hpp` is deliberately pure — "core so they are unit-testable; the
  renderer maps them". `MatchClock` follows the same rule.
- **`WorldSnapshot` replicates players and NPCs but NOT the world clock.** Each
  client runs its own time today.

## Decisions taken

| Question | Answer |
|---|---|
| What drives the day? | **The match owns pacing; the clock is slaved to it.** Day length is a match setting in real minutes (default 8). Dusk coinciding with the vote is then guaranteed by construction, not by tuning a time scale. |
| NPCs during the vote? | **Everyone gathers at the plaza** via a temporary schedule override. The vote becomes a scene with a visible roll-call of who is left. |
| How does a match start? | **Same in single-player and multiplayer: opening cutscene, then interrogation** (2026-08-08). A match is a mode entered deliberately, not a state free roam drifts into. |

## Why the dusk coupling matters

`worldHour()` sweeps `dayStartHour` (9.0) → `dayEndHour` (20.0) across the
investigation phase. `DayNight`'s dusk band is 18.0–21.0, so **dusk lands
exactly when the vote opens** — the light going orange *is* the countdown, and
no timer UI is needed to convey it. A test pins that coupling so a later retune
of either value fails loudly.

During Vote and Resolution the hour **holds** at `dayEndHour`. Letting it run
would slide the town toward night mid-vote and make the plaza gather unreadable;
the light freezing is also a signal in itself.

## Match entry (answered 2026-08-08)

A match begins with the opening cutscene, then hands control over for the
investigation. Single-player and multiplayer share that flow; they differ only
in roster and where authority lives.

- There is an **`Intro` phase** before the first Investigation, its duration the
  opening cutscene's budget. `MatchPhase` gains a fifth value — prefer an
  explicit one over reusing Resolution's slot on day zero.
- **A match is a mode, not a flag.** Free roam keeps `advanceTime`; a match owns
  the clock. Nothing may have both; assert it rather than assume it.
- The cutscene does **not** gate this. Until the cutscene system exists the
  intro phase simply elapses, keeping `MatchClock` independent of presentation.

## Out of Scope

- The vote itself (separate plan), the murder, the ground truth.
- Multiplayer sync. Match state is *placed* so adding it later is cheap — `day`,
  `phase`, `phaseRemainingSeconds`, three plain numbers — but no wire changes.
- Cutscenes. The transition events are their hook.
- Save/resume mid-match.
- Changing free-roam's endless 60× loop. This is additive.

## Design (shipped in #154)

```cpp
enum class MatchPhase { Investigation, Vote, Resolution, Ended };  // + Intro (#155)

struct MatchRules {
    int dayLimit = 3;
    float investigationSeconds = 8.f * 60.f;
    float voteSeconds = 60.f;
    float resolutionSeconds = 15.f;
    float dayStartHour = 9.f;
    float dayEndHour = 20.f;   // inside DayNight's dusk band, on purpose
};

struct PhaseTransition {
    bool fired = false;
    MatchPhase from, to;
    int day = 1;
    bool dayAdvanced = false;
    bool matchEnded = false;
};
```

`advance()` reports **at most one transition per call** and carries the
overshoot. A frame longer than a whole phase must not silently skip a phase's
event, because the vote and cutscenes hang off exactly those events. Rules are
**clamped, never rejected**: `dayLimit` below 1 becomes 1, durations below a
one-second floor are raised, which is what stops `phaseProgress()` dividing by
zero. Negative dt is ignored rather than rewinding.

## The multiplayer seam (nearly free now, expensive later)

Keep all match state in three fields that serialize as plain numbers and do not
scatter derived state. Then the networked-match work adds three fields to
`WorldSnapshot` and clients stop running their own clock. **Do not add the net
code here.**

## Implementation Order

1. **`MatchClock` + tests** — pure, no wiring. **DONE, PR #167.**
2. **Drive it from `main.cpp`** (#155) plus the minimal match entry and `Intro`
   phase. Free roam must stay byte-identical.
3. **Plaza gather during Vote** (#156) — a destination override, released on the
   day rollover. Reuses schedule-driven movement, not new pathfinding.
4. **HUD** (#157) — day, phase, remaining time. Small; the dusk does the work.
5. **Match settings** (#157) — day limit and day length, defaults 3 and 8 min.

## Acceptance Criteria

- [ ] `dayLimit = 3` cycles Investigation → Vote → Resolution three times and
      the final transition reports `matchEnded`.
- [ ] `worldHour()` sweeps 9.0 → 20.0 across Investigation and **holds** at
      20.0 during Vote and Resolution.
- [ ] A single `advance(600)` reports exactly one transition; leftover carries.
- [ ] `advance(0)` and negative dt move nothing.
- [ ] `lightLevelAt(20.0)` sits in the dusk blend — pins the diegetic countdown.
- [ ] Vote phase: every surviving NPC targets the plaza; schedules resume on
      rollover.
- [ ] With no match running, free roam is byte-identical to today.
- [ ] `endMatch()` is idempotent; an Ended clock stops advancing.

## Edge Cases

| Situation | Behaviour |
|---|---|
| Frame longer than a phase | One transition per call; leftover carries |
| `dayLimit` ≤ 0 | Clamped to 1 |
| Zero-length phase | Clamped to a 1s floor |
| `dayEndHour < dayStartHour` | Rejected at construction |
| `endMatch()` twice | Idempotent |
| NPC cannot reach the plaza | Votes from where it stands — presentation must not break the mechanic |
| Free roam and a match both active | Impossible by construction; asserted |

## Open Questions

1. Does the vote phase cut off a conversation in progress? I would let the phase
   advance and close the dialogue, but it is a feel decision.
2. Where exactly is "the plaza"? May need an authored marker with room for 20.
3. Does day 1 start at 09:00 like the others, or later to compress the first day
   after the cutscene?
