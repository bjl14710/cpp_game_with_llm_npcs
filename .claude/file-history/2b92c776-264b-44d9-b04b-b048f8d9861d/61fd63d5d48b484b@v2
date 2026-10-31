#include "WorldState.hpp"

#include <cmath>

namespace llm_npc {

namespace {
constexpr const char* kTimeKey = "world_time_seconds";
}

WorldState::WorldState() {
    setNumber(kTimeKey, WorldState::kStartHour * 3600.0);
}

void WorldState::setNumber(const std::string& key, double value) {
    facts_[key].number = value;
}

double WorldState::number(const std::string& key, double fallback) const {
    const auto it = facts_.find(key);
    return it != facts_.end() ? it->second.number : fallback;
}

void WorldState::setText(const std::string& key, std::string value) {
    facts_[key].text = std::move(value);
}

const std::string* WorldState::text(const std::string& key) const {
    const auto it = facts_.find(key);
    return it != facts_.end() ? &it->second.text : nullptr;
}

void WorldState::advanceTime(float realDtSeconds) {
    double seconds = number(kTimeKey) +
                     static_cast<double>(realDtSeconds) * kGameSecondsPerRealSecond;
    seconds = std::fmod(seconds, kSecondsPerDay);
    if (seconds < 0.0) seconds += kSecondsPerDay;
    setNumber(kTimeKey, seconds);
}

double WorldState::timeOfDayHours() const { return number(kTimeKey) / 3600.0; }

void WorldState::setTimeOfDayHours(double hours) {
    double seconds = std::fmod(hours * 3600.0, kSecondsPerDay);
    if (seconds < 0.0) seconds += kSecondsPerDay;
    setNumber(kTimeKey, seconds);
}

}  // namespace llm_npc
