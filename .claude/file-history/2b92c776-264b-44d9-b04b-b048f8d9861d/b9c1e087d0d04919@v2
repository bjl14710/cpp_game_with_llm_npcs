#pragma once

#include <string>
#include <unordered_map>

namespace llm_npc {

// One shared fact on the world bus: a number, a piece of text, or both.
// Gossip rumors and journal entries will ride the same shape later; the
// world clock is the first resident.
struct WorldFact {
    double number = 0.0;
    std::string text;
};

// The world bus: a single string-keyed store every system reads shared
// facts from instead of keeping private copies. Owned by World; the clock
// convenience methods below read/write the same "world_time_seconds" fact
// any other consumer could access raw — they are sugar, not a side channel.
//
// Deliberately minimal tonight (plan: world-time-and-schedules): no
// subscriptions, no persistence, no replication. Those arrive with the
// systems that need them (gossip replication will force the protocol bump).
class WorldState {
   public:
    WorldState();

    // ---- generic facts -----------------------------------------------------
    void setNumber(const std::string& key, double value);
    // Value of `key`, or `fallback` when the fact doesn't exist.
    double number(const std::string& key, double fallback = 0.0) const;

    void setText(const std::string& key, std::string value);
    // Text of `key`; nullptr when the fact doesn't exist (callers that
    // need "missing vs empty" get it; everyone else falls back cheaply).
    const std::string* text(const std::string& key) const;

    bool has(const std::string& key) const { return facts_.count(key) != 0; }

    // ---- the clock (first fact on the bus) ---------------------------------
    // 1 real second = 1 game minute: a full day in 24 real minutes — fast
    // enough to watch the cycle in one session, slow enough to converse.
    static constexpr double kGameSecondsPerRealSecond = 60.0;
    static constexpr double kSecondsPerDay = 24.0 * 60.0 * 60.0;
    // The town wakes at 09:00 — mid-morning light for default screenshots.
    static constexpr double kStartHour = 9.0;

    // Advances the clock by a real-time frame delta, wrapping at midnight.
    void advanceTime(float realDtSeconds);

    // Time of day in hours, [0, 24).
    double timeOfDayHours() const;

    // Overrides the clock (smoke runs' --hour flag, tests).
    void setTimeOfDayHours(double hours);

   private:
    std::unordered_map<std::string, WorldFact> facts_;
};

}  // namespace llm_npc
