#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace llm_npc {

// One shared fact on the world bus: a number, a piece of text, or both.
// The world clock is the first resident.
struct WorldFact {
    double number = 0.0;
    std::string text;
};

// One piece of structured town knowledge (gossip). Normalized fields —
// not free paragraphs — because the journal's contradiction check
// compares subjects exactly. `source` is who first said it ("player" or
// a persona name); who has HEARD it lives in the knowledge sets, not in
// the fact.
struct KnownFact {
    std::string factId;        // stable hash of subject+content
    std::string subject;       // normalized: lowercase, [a-z0-9_]
    std::string content;       // one short statement, <= 140 chars
    std::string source;
    double learnedAtSeconds = 0.0;  // world time when first recorded
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

    // ---- structured town knowledge (gossip) --------------------------------
    // Commit a fact to the bus. Idempotent: an existing factId keeps its
    // original source and timestamp (first teller wins). Returns whether
    // the fact was new.
    bool addFact(const KnownFact& fact);

    // Marks `agent` ("player" or a persona name) as having heard `factId`.
    // Unknown fact ids are ignored (no phantom knowledge).
    void grantKnowledge(const std::string& agent, const std::string& factId);

    bool knows(const std::string& agent, const std::string& factId) const;

    // Facts `agent` has heard, in commit order.
    std::vector<const KnownFact*> factsKnownBy(const std::string& agent) const;

    // Every fact on the bus, in commit order (persistence, journal).
    const std::vector<KnownFact>& facts() const { return knownFacts_; }

    const KnownFact* findFact(const std::string& factId) const;

   private:
    std::unordered_map<std::string, WorldFact> facts_;
    std::vector<KnownFact> knownFacts_;  // commit order preserved
    std::unordered_map<std::string, std::unordered_set<std::string>>
        knowledge_;  // agent -> fact ids heard
};

}  // namespace llm_npc
