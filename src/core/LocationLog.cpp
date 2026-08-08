#include "LocationLog.hpp"

#include "Zones.hpp"

namespace llm_npc {

// Scaffolded stubs (plan: clue-like-zones, step 2). Nothing is recorded and
// both queries return empty, so a caller ticking this every frame changes no
// behaviour. Fill these in and un-skip tests/test_location_log.cpp in the same
// commit.

void LocationLog::observe(const std::string&, float, float, double) {
    // TODO(zones step 2): resolve zoneAt(x, z); if it differs from this
    // agent's current ongoing visit, close that visit at worldHour and append
    // a new ongoing one. If it matches, do nothing — that is what makes this
    // transition-based rather than a sampler.
}

std::vector<ZoneVisit> LocationLog::whoWasIn(const std::string&, double,
                                             double) const {
    // TODO(zones step 2): overlap, not containment. An inverted window
    // (from > to) returns empty rather than silently swapping the bounds.
    return {};
}

std::vector<ZoneVisit> LocationLog::trailOf(const std::string&, double,
                                            double) const {
    // TODO(zones step 2): this agent's overlapping visits, chronological.
    return {};
}

void LocationLog::closeAgent(const std::string&, double) {
    // TODO(zones step 2): close any ongoing visit for this agent.
}

void LocationLog::clear() {
    visits_.clear();
}

}  // namespace llm_npc
