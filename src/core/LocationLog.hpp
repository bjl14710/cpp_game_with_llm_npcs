#pragma once

#include <string>
#include <vector>

namespace llm_npc {

// Where every agent has been, as an interval record (plan: clue-like-zones).
//
// This is the alibi substrate. No position history of any kind exists today —
// Journal and FactStore are fact-keyed, not positional — so this is genuinely
// new state.
//
// TRANSITIONS, NOT SAMPLES. Twenty-one NPCs sampled at 1 Hz over a
// 24-real-minute day is ~30,000 rows a day and grows without bound. Recording
// only a CHANGE of zone is perhaps 50 rows per agent per day, makes the query
// a scan over stays rather than a histogram, and stores exactly the sentence
// an alibi is made of: "entered the bakery at 14:03, left at 14:21".
//
// Ground truth only. This records what actually happened; turning it into
// per-NPC partial testimony (who SAW whom) is a separate concern, and
// co-presence is trivially derivable from these intervals.

// One agent's stay in one zone. Closed when they leave; `ongoing` while they
// are still there.
struct ZoneVisit {
    std::string agent;   // persona name, or "player"
    std::string zoneId;
    double startHour = 0.0;
    // Only meaningful when `ongoing` is false. An ongoing visit has no end
    // yet, and a query that overlaps it should treat it as running to the
    // window's end rather than to endHour.
    double endHour = 0.0;
    bool ongoing = false;
};

class LocationLog {
   public:
    // Records where one agent is at this instant. Only a change of zone writes
    // anything, so calling this every frame for every agent is the intended
    // usage and costs a zone lookup plus a comparison.
    //
    // `worldHour` is passed in rather than read from a clock, for the same
    // reason DayNight.hpp is a pure function of hours: it keeps this testable
    // with no WorldState and no MatchClock. It must be the match's MONOTONIC
    // hour, not the wrapped 0-24 clock hour, or a stay across midnight becomes
    // two out-of-order intervals.
    //
    // TODO(zones step 2): implement.
    void observe(const std::string& agent, float x, float z, double worldHour);

    // Everyone whose stay in `zoneId` OVERLAPS [fromHour, toHour]. Overlap, not
    // containment: someone who arrived at 13:50 and left at 14:10 was in the
    // bakery during 14:00-15:00 and must be returned.
    //
    // TODO(zones step 2): implement.
    std::vector<ZoneVisit> whoWasIn(const std::string& zoneId,
                                    double fromHour, double toHour) const;

    // Where `agent` was across the window, in chronological order — their
    // alibi, in the order they would tell it.
    //
    // TODO(zones step 2): implement.
    std::vector<ZoneVisit> trailOf(const std::string& agent,
                                   double fromHour, double toHour) const;

    // Closes any ongoing visit for one agent — call when they die, so a corpse
    // does not accumulate an eternal stay.
    //
    // TODO(zones step 2): implement.
    void closeAgent(const std::string& agent, double worldHour);

    // Drops everything. New match.
    void clear();

    // All recorded visits, for tests and tooling.
    const std::vector<ZoneVisit>& visits() const { return visits_; }

   private:
    // Append-only, in arrival order. Ordering is by construction rather than
    // by sorting, because observe() only ever appends or closes the last visit
    // for an agent.
    std::vector<ZoneVisit> visits_;
};

}  // namespace llm_npc
