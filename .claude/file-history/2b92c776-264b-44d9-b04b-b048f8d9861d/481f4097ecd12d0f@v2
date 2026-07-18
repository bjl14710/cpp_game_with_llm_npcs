# Plan: World Time, NPC Schedules, and Day/Night — the "world bus"
Date: 2026-07-06 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: M

## The Idea (one paragraph)
One authoritative world clock lives in a new generic shared-fact store
(`WorldState`, owned by `World`) that every time-aware system reads:
NPCs follow simple authored schedules (walk to a spot and do an activity
per time-of-day range), and the sky/fog/light shift through a full
day/night cycle — all from the same `world_time` value, never a private
timer. The store is the "world bus" that gossip and journal facts will
plug into later: string-keyed facts holding a number and/or text, with
world_time as its first entry.

## Goal
The player watches the town live on a clock: at 6 the baker walks to the
bakery, at dusk the sky warms then darkens and fog fades to night, and
every system agrees what time it is because there is only one time.

## Autonomy decisions (most-defensible picks, logged)
- **Store shape**: `WorldFact {double number; std::string text;}` in a
  string-keyed map with typed set/get helpers. Generic enough for gossip
  ("gossip.rumor_3" → text) and journal facts (number or text), no more.
  Time convenience accessors sit on WorldState but read the same fact
  ("world_time_seconds") any other system could read raw. NOT over-built:
  no subscriptions, no persistence, no replication tonight.
- **Clock rate**: 1 real second = 1 game minute (24-minute full day) —
  fast enough to see the cycle in a session; constant lives with the
  store. Game starts at 09:00. Time does not persist across restarts
  (nothing consumes persistence yet; log as future work with gossip).
- **Schedules are authored in .persona files** (the existing NPC content
  format): repeated `schedule = HH-HH, x, z, activity` header lines
  parsed by PersonaLoader into LoadedPersona (placement concern, like
  position — NOT part of Persona identity). Entries may wrap midnight
  (22-6). Npc walks toward the active entry's spot at kNpcWalk with the
  ReturnHome movement pattern whenever behavior_ is None and combat is
  Idle; the player standing within talk range pauses schedule walking
  (NPCs shouldn't walk away mid-approach/mid-chat).
- **Schedule activity label**: stored and exposed (`npc.activity()`),
  shown in the nameplate as "Name — activity". Injecting it into the
  LLM prompt is deferred (touches ask()'s prompt assembly; future
  ticket with gossip).
- **Day/night = pure functions in core** (`DayNight.hpp`:
  `skyColorAt(hours)`, `lightLevelAt(hours)` returning float triples/
  scalars) so the curve is unit-testable; the renderer maps them to
  raylib Color + two shader uniforms (fogColor already exists, new
  `lightLevel` multiplies the lit color). Sky, fog color, and light all
  derive from the SAME hour — fog keeps melting into the horizon at
  every time of day (lesson 0019's rule).
- **Smoke determinism**: day/night would make every screenshot depend on
  wall-clock frames, so `--frames` gains an optional `--hour H` override
  pinning the clock (same pattern as `--camera`). Existing screenshot
  workflows keep working (default hour = 09:00 noonish daylight... 9am).
- **Multiplayer**: guests run their own clock from the same 09:00 start;
  no replication tonight (needs a protocol bump — deferred to the gossip
  ticket, which must replicate facts anyway). Logged limitation.
- **Branch**: `feature/world-time` stacked on `feature/character-creator`
  (PR #77) — touches main.cpp/renderer/personas which the open chain
  modifies. One draft PR closing the tickets.

## Out of Scope (this version)
Gossip/journal content; fact persistence; fact replication; prompt
injection of activity/time; streetlamp glow, stars, moon, shadows;
schedule pathfinding beyond the existing straight-line + collision
sliding; editing schedules in-game; created-character schedules (they
just have none — same as several roster NPCs).

## Affected Areas
- NEW `src/core/WorldState.{hpp,cpp}` — fact store + clock advance/query.
- NEW `src/core/DayNight.hpp` — pure sky/light curves (header-only).
- `src/core/World.{hpp,cpp}` — owns WorldState; `state()` accessors;
  update advances the clock.
- `src/core/PersonaLoader.{hpp,cpp}` — parse/render `schedule =` lines;
  ScheduleEntry on LoadedPersona.
- `src/core/Npc.{hpp,cpp}` — setSchedule; update() takes timeOfDayHours;
  schedule walking + activity(); pause near the player.
- `src/app/RaylibRenderer.{hpp,cpp}` — setTimeOfDay(hours): sky color
  for main, fogColor + lightLevel uniforms per frame.
- `src/app/Assets.{hpp,cpp}` — lightLevel uniform in the fog shader +
  location accessor (fog color loc too, now per-frame).
- `src/app/main.cpp` — advance clock, pass hour to npc.update/renderer,
  ClearBackground from renderer.skyColor(), nameplate activity suffix,
  `--hour` smoke flag.
- `personas/baker|teacher|musician|barista.persona` — example schedules.
- Tests: NEW test_world_state.cpp, test_day_night.cpp; extend
  test_persona_roster.cpp (schedule parsing) and test_npc_behavior.cpp
  (schedule movement, no-private-clock, pause-near-player).

## Implementation Order
1. WorldState + DayNight + tests.
2. Schedule parsing (PersonaLoader) + Npc schedule behavior + tests.
3. Renderer/Assets day/night pass + --hour flag + main wiring.
4. Persona schedule content; screenshots at 09:00 / 19:00 / 00:00;
   suite; PR; report.

## Acceptance Criteria
- [ ] WorldState: number/text facts round-trip; clock wraps 24h; two
      readers see the same value. Tests green.
- [ ] Schedule: at 07:00 the baker's active entry is the bakery; passing
      a different hour to the SAME npc moves it elsewhere (proves no
      private clock); walking pauses when the player is within talk
      range. Tests green.
- [ ] Day/night: skyColorAt/lightLevelAt continuous at range edges
      (tests); screenshots at 09:00, 19:00, 00:00 show day, dusk, night
      with fog matching the sky at each.
- [ ] `--hour` pins the clock for smoke runs; default screenshots
      unchanged vs tonight's baseline (09:00).
- [ ] Full suite green; build + smoke clean.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| Schedule entry wraps midnight (22-6) | Active when hour >= start OR hour < end. |
| Overlapping entries | First matching entry wins (documented; authoring rule). |
| No active entry / no schedule | NPC idles at home exactly as today. |
| Malformed schedule line | Persona parse error names the line; file skipped like any bad persona (existing contract). |
| Combat/Follow/Arrest active | Those behaviors own movement; schedule yields (checked by state, not priority flags). |
| Unknown fact key read | number(key, fallback) returns fallback; text returns nullptr. |
| Hour exactly at a range edge | start-inclusive, end-exclusive [start, end). |

## Open Questions
None — decisions above were made under the autonomy instructions.

## Suggested GitHub Issues
1. feat(core): WorldState — shared fact store with the world clock as
   its first fact
2. feat(core): NPC schedules authored in persona files, driven by
   world_time
3. feat(render): day/night sky, fog, and light from the shared clock
4. feat(game): wire clock/schedules/visuals in the loop + --hour smoke
   flag + example schedules
