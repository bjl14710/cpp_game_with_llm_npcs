// Tests for the journal read path: visibility filter, grouping, conflicts.
#include "Journal.hpp"
#include "WorldState.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

KnownFact fact(const char* id, const char* subject, const char* content,
               const char* source, double at) {
    KnownFact f;
    f.factId = id;
    f.subject = subject;
    f.content = content;
    f.source = source;
    f.learnedAtSeconds = at;
    return f;
}

}  // namespace

TEST_CASE("journal shows only facts the player was personally told") {
    WorldState state;
    // Told to the player by Marge -> visible.
    state.addFact(fact("f1", "bakery_fire", "The oven caught fire.", "Marge", 100));
    state.grantKnowledge("player", "f1");
    // The player told Hal this -> hidden (source is the player).
    state.addFact(fact("f2", "cab_secret", "Ray hides fares.", "player", 200));
    state.grantKnowledge("player", "f2");
    state.grantKnowledge("Hal", "f2");
    // Gossip between NPCs the player never heard -> hidden (not a knower).
    state.addFact(fact("f3", "park_ghost", "The park is haunted.", "Benny", 300));
    state.grantKnowledge("Theo", "f3");

    const auto entries = journalEntries(state);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].fact->factId == std::string("f1"));
    CHECK_FALSE(entries[0].conflicting);
}

TEST_CASE("same subject with different content flags BOTH; same content doesn't") {
    WorldState state;
    state.addFact(fact("f1", "bakery_fire", "It happened at dawn.", "Marge", 100));
    state.addFact(fact("f2", "bakery_fire", "It happened at midnight.", "Hal", 200));
    state.addFact(fact("f3", "plaza_cart", "Gus sells the best dogs.", "Ray", 300));
    state.addFact(fact("f4", "plaza_cart", "Gus sells the best dogs.", "Yuki", 400));
    for (const char* id : {"f1", "f2", "f3", "f4"}) state.grantKnowledge("player", id);

    const auto entries = journalEntries(state);
    REQUIRE(entries.size() == 4);
    // Grouped by subject (bakery_fire first), time-ordered within a group.
    CHECK(entries[0].fact->factId == std::string("f1"));
    CHECK(entries[1].fact->factId == std::string("f2"));
    CHECK(entries[0].conflicting);
    CHECK(entries[1].conflicting);
    // Two tellers agreeing is corroboration, not conflict.
    CHECK_FALSE(entries[2].conflicting);
    CHECK_FALSE(entries[3].conflicting);
}

TEST_CASE("clockLabel renders HH:MM") {
    CHECK(clockLabel(0.0) == "00:00");
    CHECK(clockLabel(9.5 * 3600.0) == "09:30");
    CHECK(clockLabel(23.0 * 3600.0 + 59.0 * 60.0) == "23:59");
}
