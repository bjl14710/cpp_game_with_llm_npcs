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

// ---- provenance pacing (issue #219) ----------------------------------------
//
// THE MEASUREMENT IS THE TEST. The plan is explicit that these constants come
// from data rather than from taste, so the acceptance criterion is simulated
// here rather than argued for in a comment.
//
// One match day: the investigation phase is 8 real minutes and the gossip tick
// runs every 15 real seconds, so 32 ticks. The world clock compresses ~11
// in-world hours into those 8 minutes, so each tick advances the clock by
// roughly 20 game minutes.

namespace {

constexpr int kTicksPerMatchDay = 32;
constexpr double kGameSecondsPerTick = 11.0 * 3600.0 / kTicksPerMatchDay;

// Twenty residents packed into a compact downtown: a 5x4 lattice at 4 units,
// which is inside kGossipRadius (6), so most neighbours can always hear each
// other. This is the WORST case for homogenisation, which is the case the
// constants have to survive.
std::vector<AgentAt> clusteredTown() {
    std::vector<AgentAt> agents;
    for (int i = 0; i < 20; ++i) {
        agents.push_back({"resident_" + std::to_string(i),
                          Vec3{static_cast<float>(i % 5) * 4.f, 0.f,
                               static_cast<float>(i / 5) * 4.f}});
    }
    return agents;
}

// Seeds one private account per witness, sourced with a persona name so it
// lands in the testimony lane, and returns their fact ids.
std::vector<std::string> seedTestimony(WorldState& state,
                                       const std::vector<AgentAt>& agents,
                                       int witnesses) {
    std::vector<std::string> ids;
    for (int i = 0; i < witnesses; ++i) {
        const std::string& who = agents[static_cast<std::size_t>(i)].name;
        KnownFact fact;
        fact.subject = "someone_whereabouts";
        fact.content = who + " saw something at the bakery.";
        fact.factId = factIdFor(fact.subject, fact.content);
        fact.source = who;  // a persona name: testimony, not player-introduced
        fact.learnedAtSeconds = 0.0;
        state.addFact(fact);
        state.grantKnowledge(who, fact.factId);
        ids.push_back(fact.factId);
    }
    return ids;
}

// Runs a full match day of propagation, advancing the clock each tick.
void runMatchDay(WorldState& state, const std::vector<AgentAt>& agents,
                 std::mt19937& rng) {
    for (int tick = 0; tick < kTicksPerMatchDay; ++tick) {
        state.setNumber("world_time_seconds",
                        static_cast<double>(tick + 1) * kGameSecondsPerTick);
        propagateGossip(state, agents, rng);
    }
}

// How many (agent, fact) pairs are still unknown after the day.
int gapsRemaining(const WorldState& state, const std::vector<AgentAt>& agents,
                  const std::vector<std::string>& factIds) {
    int gaps = 0;
    for (const AgentAt& agent : agents) {
        for (const std::string& id : factIds) {
            if (!state.knows(agent.name, id)) ++gaps;
        }
    }
    return gaps;
}

}  // namespace

TEST_CASE("a full match day does not homogenise seeded testimony") {
    // THE ACCEPTANCE CRITERION. If this goes red the mode is broken in a way
    // no other test would catch: every resident would hold every account, so
    // interrogating a second person would tell you nothing new and the whole
    // investigation collapses into one conversation.
    //
    // Several seeds, because passing on one lucky RNG stream is not a result.
    for (const unsigned seed : {1u, 7u, 4242u, 99991u}) {
        CAPTURE(seed);
        WorldState state;
        const std::vector<AgentAt> town = clusteredTown();
        const std::vector<std::string> ids = seedTestimony(state, town, 3);

        std::mt19937 rng(seed);
        runMatchDay(state, town, rng);

        CHECK(gapsRemaining(state, town, ids) > 0);
    }
}

TEST_CASE("at the player rate, the same day WOULD homogenise") {
    // The counterfactual, so the constants are justified by a measured
    // difference rather than by assertion. Same town, same ticks, same seeds —
    // the only change is that the facts are sourced "player", which puts them
    // in the fast lane.
    int homogenised = 0;
    for (const unsigned seed : {1u, 7u, 4242u, 99991u}) {
        WorldState state;
        const std::vector<AgentAt> town = clusteredTown();

        std::vector<std::string> ids;
        for (int i = 0; i < 3; ++i) {
            KnownFact fact;
            fact.subject = "someone_whereabouts";
            fact.content = town[static_cast<std::size_t>(i)].name + " saw something.";
            fact.factId = factIdFor(fact.subject, fact.content);
            fact.source = "player";  // the fast lane
            fact.learnedAtSeconds = 0.0;
            state.addFact(fact);
            state.grantKnowledge(town[static_cast<std::size_t>(i)].name, fact.factId);
            ids.push_back(fact.factId);
        }

        std::mt19937 rng(seed);
        runMatchDay(state, town, rng);
        if (gapsRemaining(state, town, ids) == 0) ++homogenised;
    }
    // Not "always", because the rolls are random — but the difference from the
    // testimony lane has to be stark, or the split is not doing anything.
    CHECK(homogenised >= 3);
}

TEST_CASE("testimony still travels, given enough of a match") {
    // Slower is the point; silent is not. Second-hand testimony has to exist,
    // because "Ray told me he saw..." is a real and weaker kind of evidence,
    // and a red-herring generator the mode wants.
    WorldState state;
    const std::vector<AgentAt> town = clusteredTown();
    const std::vector<std::string> ids = seedTestimony(state, town, 3);

    // Measured against the state at seeding, not against the theoretical
    // maximum: three facts each known by one agent already leaves 57 of the 60
    // pairs unknown, so "fewer than 60" would pass with zero transfers.
    const int before = gapsRemaining(state, town, ids);
    REQUIRE(before == 57);

    std::mt19937 rng(7u);
    for (int day = 0; day < 3; ++day) {
        for (int tick = 0; tick < kTicksPerMatchDay; ++tick) {
            state.setNumber("world_time_seconds",
                            static_cast<double>(day * kTicksPerMatchDay + tick + 1) *
                                kGameSecondsPerTick);
            propagateGossip(state, town, rng);
        }
    }
    CHECK(gapsRemaining(state, town, ids) < before);
}

TEST_CASE("the player lane is untouched: one draw per pair direction") {
    // The change compares ONE draw against a per-fact threshold rather than
    // drawing per fact. Drawing per fact would give a pair with several
    // candidates several chances and quietly speed up the player lane, which
    // this change is required to leave alone.
    //
    // Pinned by draw count: two runs from the same seed, one with a single
    // player fact and one with four, must consume the same rng stream — so a
    // third fact committed afterwards gets the same id-independent outcome.
    const auto drawsConsumed = [](int factCount) {
        WorldState state;
        state.setNumber("world_time_seconds", 4.0 * 3600.0);
        std::vector<AgentAt> pair = {{"Marge", {0.f, 0.f, 0.f}},
                                     {"Hal", {1.f, 0.f, 0.f}}};
        for (int i = 0; i < factCount; ++i) {
            KnownFact fact;
            fact.subject = "s" + std::to_string(i);
            fact.content = "c" + std::to_string(i);
            fact.factId = factIdFor(fact.subject, fact.content);
            fact.source = "player";
            fact.learnedAtSeconds = 0.0;
            state.addFact(fact);
            state.grantKnowledge("Marge", fact.factId);
        }
        std::mt19937 rng(12345u);
        propagateGossip(state, pair, rng);
        return rng();  // the next value in the stream
    };
    CHECK(drawsConsumed(1) == drawsConsumed(4));
}
