#include "LocationLog.hpp"

#include <algorithm>

#include "Zones.hpp"

namespace llm_npc {
namespace {

// True when [aStart, aEnd] and [bStart, bEnd] share any instant.
//
// Overlap, not containment. Someone who arrived at 13:50 and left at 14:10 WAS
// in the bakery during 14:00-15:00, and an alibi query that misses them is
// worse than useless.
bool overlaps(double aStart, double aEnd, double bStart, double bEnd) {
    return aStart <= bEnd && bStart <= aEnd;
}

}  // namespace

void LocationLog::observe(const std::string& agent, float x, float z,
                          double worldHour) {
    // A retired agent is dead. The body still gets ticked every frame by the
    // caller, so without this it would open a fresh stay and keep logging.
    if (closed_.count(agent) != 0) return;

    const std::string& zone = zoneAt(x, z);

    // The agent's current stay is the last ongoing visit they own. Scanning
    // backwards finds it immediately in the common case, because an agent that
    // is moving appended its visit recently.
    ZoneVisit* current = nullptr;
    for (auto it = visits_.rbegin(); it != visits_.rend(); ++it) {
        if (it->agent == agent) {
            if (it->ongoing) current = &(*it);
            break;  // the newest entry for this agent decides; older ones are closed
        }
    }

    if (current != nullptr) {
        // Standing still writes NOTHING. This is what makes the log
        // transition-based rather than a sampler, and it is why ticking every
        // frame for 21 NPCs is the intended usage.
        if (current->zoneId == zone) return;
        current->endHour = worldHour;
        current->ongoing = false;
    }

    ZoneVisit next;
    next.agent = agent;
    next.zoneId = zone;
    next.startHour = worldHour;
    next.ongoing = true;
    visits_.push_back(std::move(next));
}

std::vector<ZoneVisit> LocationLog::whoWasIn(const std::string& zoneId,
                                             double fromHour,
                                             double toHour) const {
    std::vector<ZoneVisit> found;
    // An inverted window returns empty rather than silently swapping the
    // bounds — a caller that got its arguments backwards should see nothing,
    // not a confidently wrong answer.
    if (fromHour > toHour) return found;

    for (const ZoneVisit& visit : visits_) {
        if (visit.zoneId != zoneId) continue;
        // An ongoing visit has no end yet, so it runs to the end of the window
        // being asked about: an agent who walked in at 13:00 and never left is
        // still there at 14:30.
        const double end = visit.ongoing ? toHour : visit.endHour;
        if (overlaps(visit.startHour, end, fromHour, toHour)) found.push_back(visit);
    }
    return found;
}

std::vector<ZoneVisit> LocationLog::trailOf(const std::string& agent,
                                            double fromHour,
                                            double toHour) const {
    std::vector<ZoneVisit> trail;
    if (fromHour > toHour) return trail;

    for (const ZoneVisit& visit : visits_) {
        if (visit.agent != agent) continue;
        const double end = visit.ongoing ? toHour : visit.endHour;
        if (overlaps(visit.startHour, end, fromHour, toHour)) trail.push_back(visit);
    }
    // Already chronological: visits_ is append-only and observe() only ever
    // appends, so no sort is needed. Returned in the order the agent would
    // tell it.
    return trail;
}

void LocationLog::closeAgent(const std::string& agent, double worldHour) {
    closed_.insert(agent);
    for (auto it = visits_.rbegin(); it != visits_.rend(); ++it) {
        if (it->agent != agent) continue;
        if (it->ongoing) {
            it->endHour = worldHour;
            it->ongoing = false;
        }
        return;  // only the newest entry for an agent can be ongoing
    }
}

void LocationLog::clear() {
    visits_.clear();
    closed_.clear();
}

}  // namespace llm_npc
