// The clues that mattered (issue #233) — the contract the win cutscene needs.
//
// The load-bearing property is ORDER. win-cutscene.md calls it the requirement
// that is cheap now and expensive to retrofit: a montage over an unordered bag
// of clues cannot read as reasoning. If a sort ever creeps into either
// function, these cases go red.
#include <string>
#include <vector>

#include "Gossip.hpp"
#include "Montage.hpp"
#include "Mystery.hpp"
#include "WorldState.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

std::vector<Persona> roster() {
    std::vector<Persona> out;
    for (const char* name : {"Marge Holloway", "Ray Okafor", "Yuki Tanaka",
                             "Officer Dana Brooks", "Theo Vance", "Gus Pike"}) {
        Persona p;
        p.name = name;
        p.role = "resident";
        out.push_back(p);
    }
    return out;
}

// A setup with three clues and testimony both about and not about the killer.
MysterySetup setupWithChain() {
    MysterySetup setup = generateMystery(roster(), 4242u);
    setup.evidence = {
        Evidence{"unlocked_door", "bakery_block",
                 "The bakery's back door was unlocked all evening.", true},
        Evidence{"two_cups", "coffee_block",
                 "Two cups on the counter, one untouched.", false},
        Evidence{"torn_ledger", "bakery_block",
                 "A delivery ledger with the last entry torn out.", true},
    };
    // About the killer: belongs in the chain.
    setup.witnesses.push_back({"Ray Okafor", "bakery_block",
                               "someone hurrying out through the alley door",
                               21.5, setup.killer});
    // About somebody else: real content, real noise, not the argument.
    setup.witnesses.push_back({"Yuki Tanaka", "plaza_block",
                               "the lights still on", 22.0, "Theo Vance"});
    return setup;
}

std::vector<std::string> zonesOf(const std::vector<ClueStep>& steps) {
    std::vector<std::string> out;
    for (const ClueStep& s : steps) out.push_back(s.zoneId);
    return out;
}

}  // namespace

// ---- solutionChain ---------------------------------------------------------

TEST_CASE("the chain keeps authored order and never sorts") {
    const MysterySetup setup = setupWithChain();
    const std::vector<ClueStep> chain = solutionChain(setup);

    // Three clues plus the one testimony about the killer.
    REQUIRE(chain.size() == 4);
    CHECK(chain[0].caption ==
          "The bakery's back door was unlocked all evening.");
    CHECK(chain[1].caption == "Two cups on the counter, one untouched.");
    CHECK(chain[2].caption == "A delivery ledger with the last entry torn out.");

    // Contiguous, 1-based, in the order authored — not grouped by zone, which
    // is what a sort would produce given clues 0 and 2 share bakery_block.
    for (std::size_t i = 0; i < chain.size(); ++i) {
        CHECK(chain[i].order == static_cast<int>(i) + 1);
    }
    CHECK(zonesOf(chain) == std::vector<std::string>{"bakery_block", "coffee_block",
                                                     "bakery_block", "bakery_block"});
}

TEST_CASE("only testimony about the killer joins the argument") {
    const MysterySetup setup = setupWithChain();
    const std::vector<ClueStep> chain = solutionChain(setup);

    for (const ClueStep& step : chain) {
        CHECK(step.caption.find("the lights still on") == std::string::npos);
    }
    CHECK(chain.back().caption.find("hurrying out through the alley door") !=
          std::string::npos);
}

TEST_CASE("chain fact ids match what seeding commits, not a reconstruction") {
    // The guard against #214 happening again in a new place: two layers
    // building the same subject independently is exactly how that bug worked.
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithChain();

    WorldState state;
    seedMysteryFacts(state, setup, cast);

    const std::vector<ClueStep> chain = solutionChain(setup);
    const ClueStep& testimony = chain.back();
    REQUIRE_FALSE(testimony.factId.empty());
    CHECK(state.findFact(testimony.factId) != nullptr);
}

TEST_CASE("a setup with no evidence and no testimony yields an empty chain") {
    MysterySetup bare = generateMystery(roster(), 9u);
    CHECK(solutionChain(bare).empty());
}

TEST_CASE("the chain does not name the killer as the killer") {
    // It is built from setup.killer, so it is the answer sheet in narrative
    // form — but the captions are ordinary fact text and must stay that way.
    const MysterySetup setup = setupWithChain();
    for (const ClueStep& step : solutionChain(setup)) {
        CAPTURE(step.caption);
        for (const char* tell : {"killer", "murderer", "did it", "is guilty"}) {
            CHECK(step.caption.find(tell) == std::string::npos);
        }
    }
}

// ---- buildMontage ----------------------------------------------------------

TEST_CASE("what the agent knows lands in found, the rest in missed") {
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithChain();

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    const std::vector<ClueStep> chain = solutionChain(setup);

    // Grant only the testimony; the evidence facts are not seeded by
    // seedMysteryFacts today (that is issue #218), so they stay missed.
    state.grantKnowledge("player", chain.back().factId);

    const MontagePlan plan = buildMontage(chain, state, "player");
    REQUIRE(plan.found.size() == 1);
    CHECK(plan.found[0].caption == chain.back().caption);
    CHECK(plan.missed.size() == 3);
}

TEST_CASE("both lists keep chain order independently") {
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithChain();

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    std::vector<ClueStep> chain = solutionChain(setup);

    // Commit the first and third clue as facts and grant them, so found and
    // missed interleave in the chain rather than splitting at a boundary.
    for (const int i : {0, 2}) {
        KnownFact fact;
        fact.subject = "clue_" + std::to_string(i);
        fact.content = chain[static_cast<std::size_t>(i)].caption;
        fact.factId = chain[static_cast<std::size_t>(i)].factId;
        fact.source = "town";
        state.addFact(fact);
        state.grantKnowledge("player", fact.factId);
    }

    const MontagePlan plan = buildMontage(chain, state, "player");
    REQUIRE(plan.found.size() == 2);
    CHECK(plan.found[0].order < plan.found[1].order);
    CHECK(plan.found[0].order == 1);
    CHECK(plan.found[1].order == 3);

    REQUIRE(plan.missed.size() == 2);
    CHECK(plan.missed[0].order == 2);
    CHECK(plan.missed[1].order == 4);
}

TEST_CASE("a step with no factId is always missed, never found") {
    // An undiscovered clue belongs in the beat that exists to show it. There
    // is nothing on the bus to have learned, so "found" would be a lie.
    std::vector<ClueStep> chain = {
        ClueStep{"plaza_block", "A clock that nobody committed.", "", 1}};

    WorldState state;
    const MontagePlan plan = buildMontage(chain, state, "player");
    CHECK(plan.found.empty());
    REQUIRE(plan.missed.size() == 1);
    CHECK(plan.missed[0].caption == "A clock that nobody committed.");
}

TEST_CASE("found everything yields an empty missed, and the reverse") {
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithChain();

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    const std::vector<ClueStep> chain = solutionChain(setup);

    // Nothing known: everything missed, nothing crashes.
    const MontagePlan none = buildMontage(chain, state, "player");
    CHECK(none.found.empty());
    CHECK(none.missed.size() == chain.size());

    // Everything known: nothing missed.
    for (const ClueStep& step : chain) {
        KnownFact fact;
        fact.subject = "seeded_" + step.factId;
        fact.content = step.caption;
        fact.factId = step.factId;
        fact.source = "town";
        state.addFact(fact);
        state.grantKnowledge("player", step.factId);
    }
    const MontagePlan all = buildMontage(chain, state, "player");
    CHECK(all.missed.empty());
    CHECK(all.found.size() == chain.size());
}

TEST_CASE("an empty chain produces an empty plan rather than a dangling beat") {
    WorldState state;
    const MontagePlan plan = buildMontage({}, state, "player");
    CHECK(plan.found.empty());
    CHECK(plan.missed.empty());
}

TEST_CASE("knowledge is per agent: one player's find is another's miss") {
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithChain();

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    const std::vector<ClueStep> chain = solutionChain(setup);
    state.grantKnowledge("player_0", chain.back().factId);

    CHECK(buildMontage(chain, state, "player_0").found.size() == 1);
    CHECK(buildMontage(chain, state, "player_1").found.empty());
}
