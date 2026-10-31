#include "WorldState.hpp"

#include <cassert>
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

void WorldState::beginClockFrame() {
    clockFrameArmed_ = true;
    clockWriter_ = ClockWriter::None;
}

void WorldState::noteClockWrite(ClockWriter who) {
    if (!clockFrameArmed_) return;  // outside the frame loop: tests, --hour
    assert((clockWriter_ == ClockWriter::None || clockWriter_ == who) &&
           "two owners wrote the world clock in one frame - free roam's "
           "advanceTime and a match's setTimeOfDayHours must never both run. "
           "See advanceWorldClock in src/app/main.cpp.");
    clockWriter_ = who;
}

void WorldState::advanceTime(float realDtSeconds) {
    noteClockWrite(ClockWriter::Ticked);
    double seconds = number(kTimeKey) +
                     static_cast<double>(realDtSeconds) * kGameSecondsPerRealSecond;
    seconds = std::fmod(seconds, kSecondsPerDay);
    if (seconds < 0.0) seconds += kSecondsPerDay;
    setNumber(kTimeKey, seconds);
}

double WorldState::timeOfDayHours() const { return number(kTimeKey) / 3600.0; }

bool WorldState::addFact(const KnownFact& fact) {
    if (findFact(fact.factId)) return false;  // first teller wins
    knownFacts_.push_back(fact);
    return true;
}

void WorldState::grantKnowledge(const std::string& agent,
                                const std::string& factId) {
    if (!findFact(factId)) return;  // no phantom knowledge
    knowledge_[agent].insert(factId);
}

bool WorldState::knows(const std::string& agent, const std::string& factId) const {
    const auto it = knowledge_.find(agent);
    return it != knowledge_.end() && it->second.count(factId) != 0;
}

std::vector<const KnownFact*> WorldState::factsKnownBy(
    const std::string& agent) const {
    std::vector<const KnownFact*> out;
    const auto it = knowledge_.find(agent);
    if (it == knowledge_.end()) return out;
    for (const KnownFact& fact : knownFacts_) {
        if (it->second.count(fact.factId)) out.push_back(&fact);
    }
    return out;
}

const KnownFact* WorldState::findFact(const std::string& factId) const {
    for (const KnownFact& fact : knownFacts_) {
        if (fact.factId == factId) return &fact;
    }
    return nullptr;
}

void WorldState::setTimeOfDayHours(double hours) {
    noteClockWrite(ClockWriter::Driven);
    double seconds = std::fmod(hours * 3600.0, kSecondsPerDay);
    if (seconds < 0.0) seconds += kSecondsPerDay;
    setNumber(kTimeKey, seconds);
}

}  // namespace llm_npc
