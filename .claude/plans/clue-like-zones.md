# Plan: Clue-like Named Zones + Location Log
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: PARTLY SHIPPED — #163, #164 done (PRs #168, #169); #165, #166 open
Estimated complexity: M

## The Idea

A murder mystery has to answer "who was in the bakery between 14:00 and 15:00".
Today the game cannot: there are no named places the code can reason about, and
no record of where anyone has been. Downtown is already a 3×3 block grid —
blocks centred at −64/0/64 on each axis, spanning ±24, with 16-unit streets
between — so it partitions into **nine named zones, the same count as Clue's
nine rooms**, plus the street network. A `LocationLog` records an enter/exit
transition whenever any agent, including the player, crosses a boundary.

Separately, `Building::name` already holds "Marge's Bakery" and has **never once
reached the screen**; this renders it, so "meet me at the bakery" means
something.

## Goal

A player can walk downtown, see every shop's name above it, and the game can
answer — as ground truth — which agents were in which zone over any window.

## Decisions taken

| Question | Answer |
|---|---|
| Zones, interiors, or a new map? | **Named zones over the existing downtown.** Interiors do not exist and are not required by the actual ask. |
| Does this build witnessing too? | **No — ground-truth log only.** Per-NPC testimony is the alibi plan's job; co-presence is trivially derivable from these intervals. |

## The interiors situation (read before proposing rooms)

**Buildings are solid axis-aligned boxes.** `Building` is
`{id, name, minX, minZ, maxX, maxZ, height, facadeKind}` and
`City::circleIntersectsAny` treats every one as impassable. There is no door, no
interior, no threshold anywhere in `src/`.

Enterable buildings exist only in unported SFML branches (`9edfbd3`, `1b4cc30`,
`fab91d6`), and `legacy-feature-gaps.md` ranks the port as item 2 with the note
*"Big collision-model change (rooms, not solid AABBs)"* — an L/XL on its own.

**This plan does not need it.** A zone is a named region with a boundary;
whether that boundary is a wall with a door or the kerb of a city block changes
the feel, not the query. And `LocationLog`'s API is identical either way — when
interiors land, a room becomes another zone with a smaller footprint and one
entrance. **This is a prerequisite for that port, not a detour around it.**

## The nine zones

Derived from the same block constants `City::makeDowntown` uses, so the
partition cannot drift from the map:

| Zone id | Name | Centre | Contains |
|---|---|---|---|
| `bakery_block` | Bakery Corner | (−64, −64) | Marge's Bakery, apartments |
| `police_block` | Station Square | (0, −64) | City Police Station |
| `coffee_block` | Coffee Row | (64, −64) | Bean There Coffee |
| `library_block` | Library Steps | (−64, 0) | City Library |
| `plaza` | The Plaza | (0, 0) | Gus's Hot Dogs, open ground |
| `hardware_block` | Hardware Yard | (64, 0) | Jensen Hardware |
| `west_block` | West Terrace | (−64, 64) | filler apartments |
| `north_block` | North Walk | (0, 64) | filler apartments |
| `park` | The Park | (64, 64) | fountain, benches, bushes |

Plus **`streets`** — everything between blocks — as an explicit tenth zone
rather than a null. "Nobody was anywhere" is not a useful alibi answer, and an
optional would push that branch onto every caller.

## Two rules that matter more than they look

**Half-open bounds** (`min <= p < max`), matching `activeScheduleIndex`'s
convention for hour ranges. Adjacent blocks share an edge without overlapping,
so a point on a kerb belongs to exactly one zone. Without it an agent standing
on a boundary flickers between two zones every frame and fills the log with
phantom transitions.

**Transitions, not samples.** 21 NPCs sampled at 1 Hz over a 24-real-minute day
is ~30,000 rows a day and unbounded. Transitions are ~50 per agent per day, make
the query a scan over stays, and store exactly the sentence an alibi is made of:
"entered the bakery at 14:03, left at 14:21".

## Out of Scope

- Enterable buildings, interiors, doors.
- Witnessing and testimony. No per-NPC knowledge, no `FactStore` writes.
- The murder, victim, or ground-truth mystery.
- A new map. Sandbox maps get no zones — detective mode requires the authored
  downtown, stated rather than discovered.
- Pathfinding to zones. The log observes.
- Persisting the log past a match.
- Custom fonts for signage.

## Design (shipped)

```cpp
struct ZoneDef { std::string id, name; float minX, minZ, maxX, maxZ; };
const std::vector<ZoneDef>& zonesForDowntown();
const std::string& zoneAt(float x, float z);   // never empty; falls back to streets
const std::string& zoneName(const std::string& zoneId);

struct ZoneVisit {
    std::string agent;     // persona name, or "player"
    std::string zoneId;
    double startHour = 0.0, endHour = 0.0;
    bool ongoing = false;
};

class LocationLog {
    void observe(const std::string& agent, float x, float z, double worldHour);
    std::vector<ZoneVisit> whoWasIn(const std::string& zoneId, double from, double to) const;
    std::vector<ZoneVisit> trailOf(const std::string& agent, double from, double to) const;
    void closeAgent(const std::string& agent, double worldHour);
    void clear();
};
```

`whoWasIn` is **overlap, not containment** — a stay from 13:50 to 14:10 *was* in
the bakery during 14:00–15:00. An ongoing visit runs to the end of the window
asked about.

`worldHour` is passed in rather than read, same rule as `DayNight.hpp`, so this
is testable with no `WorldState`. It must be the **monotonic** match hour; the
wrapped 0–24 clock would split a stay across midnight into two out-of-order
intervals.

**`closeAgent` retires an agent permanently, not just closing the stay.** Writing
the test surfaced why: the caller ticks every agent every frame, so a corpse
would immediately open a fresh visit and log forever.

## Signage

`Building::name` renders as a screen-space label via the existing
`worldToScreen`, above the building at roof height — the same technique as the
mood emote. Only the six buildings with a non-empty `name` get one; props are
already authored with `name == ""`.

Constraints: cull behind the camera and beyond a distance cap, or every sign in
a 3×3 city draws every frame. And the built-in bitmap font computes glyph
spacing with **integer division**, so sizes 14–18 space identically — a
smaller-at-distance effect must step across sizes that visibly differ.

## Implementation Order

1. `Zones` + tests (#163, PR #168) — **done**
2. `LocationLog` + tests (#164, PR #169) — **done**
3. Tick the log for every agent including the player (#165) — open
4. Signage (#166) — open, independent
5. A debug key dumping `trailOf` for the nearest NPC — how #165 gets verified

## Acceptance Criteria

- [x] Each of the nine block centres resolves to its own zone.
- [x] A street point and an out-of-bounds point both resolve to `streets`.
- [x] **Every named building's centre lies inside the zone the table claims** —
      the check that stops a map edit silently mis-filing an alibi.
- [x] Repeated observation at one position records ONE visit.
- [x] `whoWasIn` returns overlapping stays, including one that began before the
      window.
- [x] An inverted window returns empty; an unknown zone returns empty.
- [ ] The player appears in the log under `"player"` — the retaliation rule
      needs the player as observable as anyone.
- [ ] A building with an empty `name` renders no sign.

## Open Questions

1. Are block zones the right granularity? A block holding both the bakery and
   two apartment buildings may be too coarse for "was he in the bakery".
   Per-building sub-zones are a strict refinement and change no API.
2. Should the street network subdivide into named streets? "He was on the
   street" is a weak alibi.
3. Sign placement on tall buildings — `library` is 15 units, `apt_a` is 22, so a
   roof-height label may float absurdly high. Facade placement needs a facing
   direction buildings do not store.
