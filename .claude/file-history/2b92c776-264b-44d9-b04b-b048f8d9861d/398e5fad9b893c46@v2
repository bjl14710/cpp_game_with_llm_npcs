#pragma once

#include <string>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// One block of an NPC's day: be at `position` doing `activity` while the
// world clock reads [startHour, endHour). Ranges may wrap midnight
// (22-6 == evening through early morning).
struct ScheduleEntry {
    float startHour = 0.f;
    float endHour = 0.f;
    Vec3 position{};
    std::string activity;
};

// Index of the entry active at `hour` (first match wins — the authoring
// rule for overlaps), or -1 when nothing is scheduled. Start-inclusive,
// end-exclusive; a wrapped range matches hour >= start OR hour < end.
inline int activeScheduleIndex(const std::vector<ScheduleEntry>& schedule,
                               float hour) {
    for (std::size_t i = 0; i < schedule.size(); ++i) {
        const ScheduleEntry& entry = schedule[i];
        const bool wraps = entry.endHour < entry.startHour;
        const bool active = wraps ? (hour >= entry.startHour || hour < entry.endHour)
                                  : (hour >= entry.startHour && hour < entry.endHour);
        if (active) return static_cast<int>(i);
    }
    return -1;
}

}  // namespace llm_npc
