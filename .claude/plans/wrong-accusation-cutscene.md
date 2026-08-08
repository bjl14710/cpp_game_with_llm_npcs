# Plan: Wrong-Accusation Cutscene
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: S

## The Idea

When the players agree on the wrong resident, that resident dies — and in
multiplayer the nominator dies in retaliation. This is the cutscene:
**family-friendly, no blood, no on-screen violence** — the deaths read as
departures rather than injuries. Everyone watches the accused walk away and the
town close behind them; then the nominator alone gets a short personal coda as
their own view drains to grey.

It fires at the end of every wrong day, so the hard requirement is that it stays
short and stops being mandatory after the first viewing.

## Goal

A group can feel the weight of getting someone killed, once per wrong day, in a
few seconds, without anyone wanting to alt-tab by day three.

## Decisions taken

| Question | Answer |
|---|---|
| One cutscene or two? | **Shared beat, then a personal coda for the nominator.** Everyone watches the resident leave; only the player who paid sees their screen go grey. |

## Two things already resolved elsewhere

**The disconnect case needs no design.** The vote plan specifies that if the
nominator has quit before resolution, retaliation falls on the lowest living
player id — deterministic, never random. So the player who dies is always someone
present. If *every* player has disconnected, the match ends and nothing plays.

**In single-player nobody dies but the accused** (amended 2026-08-08). The coda
therefore does not play in single-player at all — there is no retaliation to
depict. The shared beat still does.

**Walking away is not animation work.** The cutscene system excludes character
animation, but NPCs already walk under schedule-driven locomotion, and the
day-phases plan already introduces a destination override for the plaza gather.
The accused departing is that same override with the camera watching. **The
cutscene animates nothing — it points at a world that is already moving.**

## What is free, and what is not

| Effect | Cost |
|---|---|
| Accused walks away | Free — movement override, already planned |
| Full-screen fade to grey | Free — the fade rect, tinted |
| Letterbox, captions, hold beats | Free |
| Town clock striking, as a caption | Free — `clockLabel` |
| Fog swallowing the departing figure | Free — distance fog already exists |
| **Per-NPC colour draining** | **Not free** — a new `CharacterVisual` field and a tint path in the character draw |

The full-screen grey fade gets most of that feeling for nothing, and the fog
already swallows a departing figure. **v1 uses only the free column.**

## Pacing, which is the real design risk

- **Total duration must fit the Resolution phase budget** — around four seconds
  shared plus two for the coda.
- **Mandatory on first viewing, skippable afterwards.** The first time carries the
  beat; the second is a toll. This drives a cutscene-system change: `skippable`
  becomes three-valued (`never` / `always` / `after_first`) with a per-session
  seen-count. That belongs in the system, but this plan is its first consumer.
- **No new information after the first viewing**, so skipping never costs a player
  anything they needed.

## Out of Scope

- Any depiction of violence. No blood, weapons, impact or falling. The accused
  walks; the screen fades. That is the whole vocabulary.
- Per-NPC colour draining (see the enhancement note).
- Sound.
- A death screen or scoreboard. The player enters spectator mode; this covers the
  transition into it.
- Revealing whether the accused was innocent — an open question below.

## Design

Two authored files: `cutscenes/wrong_accusation.cutscene` (shared, poses relative
to the accused) and `cutscenes/retaliation.cutscene` (the nominator only, a grey
fade with one caption).

`fade_colour` is a small cutscene-system addition — the fade rect already exists,
this only chooses its tint. Default black.

**Dispatch:** on a `Wrong` outcome the server sends the shared id to all clients
and the coda id to the nominator only. Cutscenes play locally, so this is one
targeted message using `sendToPlayer`, which `NetServer` already has.

## Implementation Order

1. **Extend the cutscene system**: three-valued `skippable` with per-session seen
   tracking, plus `fade_colour`. Tests. No content yet.
2. `PlayCutscene` message + dispatch, broadcast and targeted, with a loopback test
   that the coda reaches **only** the nominator.
3. **The accused departure override** — send them walking *before* marking them
   dead, so the camera has something to follow.
4. Author both cutscenes; capture beats for `visual-qa`.
5. **Tone review.** An explicit pass confirming nothing reads as violent. This is
   a stated requirement, so it gets a step rather than a hope.

## Acceptance Criteria

- [ ] A `Wrong` outcome plays the shared cutscene on every client and the coda on
      **only** the nominator.
- [ ] Single-player `Wrong` plays the shared beat and **no coda** — nobody died.
- [ ] Nominator disconnected → the coda goes to the reassigned player and nobody
      receives two.
- [ ] First viewing in a session: skip does nothing. Any later viewing: skip ends
      it immediately.
- [ ] Shared plus coda fits inside the Resolution phase with margin; neither
      overruns into the next day.
- [ ] `fade_colour = grey` renders grey; absent renders black as before.
- [ ] **The accused walks before being marked `NpcState::Dead`** — a corpse cannot
      walk away.
- [ ] No frame shows blood, a weapon, or an impact. Judged by `visual-qa` against
      the stated tone requirement.
- [ ] A skipped cutscene still leaves the player entering spectator mode
      correctly — skipping must never desync state.
- [ ] A correct accusation plays neither of these.

## Edge Cases

| Situation | Behaviour |
|---|---|
| The accused is already dead | Cannot happen — the vote rejects nominating the dead |
| The accused cannot walk anywhere | They stay put and the camera holds. The departure is a nicety, not a dependency |
| A client misses `PlayCutscene` | They see the world carry on for the Resolution phase. The phase clock is authoritative, so a missed cutscene is cosmetic |
| The dying player skips the coda | They still enter spectator mode; the state change is server-driven, never gated on cutscene completion |
| Two wrong accusations in one match | Second is skippable per `after_first`. Seen-count is per session |
| `fade_colour` names an unknown colour | Falls back to black with a warning at load |

## Optional enhancement, deliberately not scoped

**Per-NPC colour draining.** A `float desaturate` on `CharacterVisual` plus a tint
path in the character draw, so the accused visibly loses colour as they walk. It
is the strongest single image in the original description and genuinely small —
but it touches the character renderer, which the cutscene system was carefully
designed not to require.

**Now wanted by two cutscenes** (this one and the win's "mask drops"), so worth
doing once as its own change rather than twice.

## Open Questions

1. Should the cutscene state the accused was innocent? The retaliation implies
   it, but an explicit line lands harder and removes ambiguity about whether the
   players simply lost a turn.
2. Does the town clock striking work as a caption, or need a visible clock? There
   is no clock prop today.
3. Is four seconds plus two enough to land the beat? Only judgeable from captured
   frames.
