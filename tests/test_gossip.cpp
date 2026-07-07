// Tests for gossip: subject normalization, propose->validate->commit, and
// conservative propagation on the world bus.
#include <random>

#include "Gossip.hpp"
#include "WorldState.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("subjects normalize to the comparable key space") {
    CHECK(normalizeSubject("The Bakery Fire!") == "the_bakery_fire");
    CHECK(normalizeSubject("  ray's   cab  ") == "ray_s_cab");
    CHECK(normalizeSubject("___") == "");
    CHECK(normalizeSubject("plaza") == "plaza");
}

TEST_CASE("fact ids are stable and content-sensitive") {
    const std::string a = factIdFor("bakery_fire", "The oven caught fire.");
    CHECK(a == factIdFor("bakery_fire", "The oven caught fire."));
    CHECK(a != factIdFor("bakery_fire", "It never happened."));
}

TEST_CASE("validation accepts well-formed proposals and rejects everything else") {
    const auto good = validateProposedFacts(
        R"([{"subject":"Bakery Fire","content":"The oven caught fire at dawn.",)"
        R"("direction":"npc_learned"},)"
        R"({"subject":"rays_cab","content":"Ray parks illegally.",)"
        R"("direction":"player_learned"}])");
    REQUIRE(good.size() == 2);
    CHECK(good[0].subject == "bakery_fire");  // normalized on the way in
    CHECK_FALSE(good[0].playerLearned);
    CHECK(good[1].playerLearned);

    CHECK(validateProposedFacts("[]").empty());
    // Each of these must yield NOTHING (no partial commits from bad input).
    for (const char* bad : {
             "the oven caught fire",                                   // prose
             R"({"subject":"x","content":"y","direction":"npc_learned"})",  // not an array
             R"([{"subject":"x","content":"y"}])",                     // missing field
             R"([{"subject":"x","content":"y","direction":"telepathy"}])",  // bad direction
             R"([{"subject":"!!!","content":"y","direction":"npc_learned"}])",  // empty after normalize
             R"([{"subject":"a","content":"b","direction":"npc_learned"},
                 {"subject":"c","content":"d","direction":"npc_learned"},
                 {"subject":"e","content":"f","direction":"npc_learned"}])",  // > 2
         }) {
        CAPTURE(bad);
        CHECK(validateProposedFacts(bad).empty());
    }
    // Oversized content is rejected too.
    CHECK(validateProposedFacts(
              std::string(R"([{"subject":"x","content":")") + std::string(200, 'a') +
              R"(","direction":"npc_learned"}])")
              .empty());
}

TEST_CASE("commit stamps world time, grants the right knowers, and is idempotent") {
    WorldState state;
    state.setTimeOfDayHours(10.0);

    ProposedFact told;
    told.subject = "bakery_fire";
    told.content = "The oven caught fire.";
    told.playerLearned = false;  // player told Marge
    const KnownFact record = commitFact(state, told, "Marge");
    CHECK(record.source == "player");
    CHECK(record.learnedAtSeconds == doctest::Approx(10.0 * 3600.0));
    CHECK(state.knows("Marge", record.factId));
    CHECK_FALSE(state.knows("player", record.factId));

    // Re-commit later: the original stamp and source survive.
    state.setTimeOfDayHours(20.0);
    const KnownFact again = commitFact(state, told, "Hal");
    CHECK(again.factId == record.factId);
    CHECK(again.learnedAtSeconds == doctest::Approx(10.0 * 3600.0));
    CHECK(state.facts().size() == 1);
    CHECK(state.knows("Hal", record.factId));

    ProposedFact heard;
    heard.subject = "cab_secret";
    heard.content = "Ray hides fares.";
    heard.playerLearned = true;  // Ray told the player
    const KnownFact record2 = commitFact(state, heard, "Ray");
    CHECK(record2.source == "Ray");
    CHECK(state.knows("player", record2.factId));
}

TEST_CASE("propagation respects radius, age, chance, and one-per-pair") {
    WorldState state;
    state.setTimeOfDayHours(9.0);
    ProposedFact fact;
    fact.subject = "s1";
    fact.content = "c1";
    const KnownFact record = commitFact(state, fact, "Marge");
    ProposedFact fact2;
    fact2.subject = "s2";
    fact2.content = "c2";
    const KnownFact record2 = commitFact(state, fact2, "Marge");

    std::mt19937 alwaysPass(1);  // with chance 0.35 some rolls fail; use many ticks

    // Too fresh: no spread regardless of proximity or rolls.
    std::vector<AgentAt> close = {{"Marge", {0.f, 0.f, 0.f}}, {"Hal", {1.f, 0.f, 0.f}}};
    for (int i = 0; i < 20; ++i) propagateGossip(state, close, alwaysPass);
    CHECK_FALSE(state.knows("Hal", record.factId));

    // Old enough but out of range: still no spread.
    state.setTimeOfDayHours(10.0);  // 60 game minutes later
    std::vector<AgentAt> far = {{"Marge", {0.f, 0.f, 0.f}}, {"Hal", {50.f, 0.f, 0.f}}};
    for (int i = 0; i < 20; ++i) propagateGossip(state, far, alwaysPass);
    CHECK_FALSE(state.knows("Hal", record.factId));

    // Close and old enough: ONE fact moves per successful tick, not both.
    const int first = propagateGossip(state, close, alwaysPass);
    if (first == 1) {
        CHECK(state.knows("Hal", record.factId) !=
              state.knows("Hal", record2.factId));
    }
    // Enough ticks: both eventually arrive (chance can fail, so iterate).
    for (int i = 0; i < 50; ++i) propagateGossip(state, close, alwaysPass);
    CHECK(state.knows("Hal", record.factId));
    CHECK(state.knows("Hal", record2.factId));

    // Facts only ever flipped knowledge bits — the store itself is intact.
    CHECK(state.facts().size() == 2);
}
