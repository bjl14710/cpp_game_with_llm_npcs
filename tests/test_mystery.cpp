// Ground truth for the opening murder (plan: opening-murder).
//
// Most cases are SKIPPED: generateMystery, seedMysteryFacts, voteIsCorrect and
// placeBodyClearOfColliders are stubs. The Npc::markDeadAtStart cases are LIVE —
// that entry point is two lines and shipped with the scaffold, because a no-op
// version of "make this NPC the victim" is a footgun rather than a placeholder.
//
// Un-skip each case in the commit that implements what it covers.
#include <set>
#include <string>
#include <vector>

#include "Journal.hpp"
#include "LlmClient.hpp"
#include "Mystery.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "WorldState.hpp"
#include "Zones.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A four-person roster: enough that "victim != killer" is not trivially
// satisfied and that a survivor set is non-empty.
//
std::vector<Persona> testRoster() {
    std::vector<Persona> roster;
    for (const char* name : {"Marge Holloway", "Ray Okafor", "Yuki Tanaka",
                             "Officer Dana Brooks"}) {
        Persona p;
        p.name = name;
        p.role = "resident";
        roster.push_back(p);
    }
    return roster;
}

// A client pointed at a port nothing listens on — these tests never make LLM
// requests, matching tests/test_combat_npc.cpp.
LlmClient& idleClient() {
    static LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    return client;
}

}  // namespace

// ---- generation: determinism --------------------------------------------

TEST_CASE("the same roster and seed produce an identical setup") {
    // The property every other assertion here leans on. Without it, "given
    // seed X the victim is Y" cannot be written at all.
    const MysterySetup a = generateMystery(testRoster(), 12345u);
    const MysterySetup b = generateMystery(testRoster(), 12345u);

    CHECK(a.victim == b.victim);
    CHECK(a.killer == b.killer);
    CHECK(a.sceneZoneId == b.sceneZoneId);
    CHECK(a.murderHour == doctest::Approx(b.murderHour));
    CHECK(a.bodyPosition.x == doctest::Approx(b.bodyPosition.x));
    CHECK(a.bodyPosition.z == doctest::Approx(b.bodyPosition.z));
}

TEST_CASE("a different seed produces a different setup") {
    // Determinism without variation is a constant, which would satisfy the
    // case above and be useless. This is the other half of the contract.
    const MysterySetup a = generateMystery(testRoster(), 1u);
    const MysterySetup b = generateMystery(testRoster(), 2u);

    const bool differs = a.victim != b.victim || a.killer != b.killer ||
                         a.sceneZoneId != b.sceneZoneId ||
                         a.murderHour != b.murderHour;
    CHECK(differs);
}

TEST_CASE("seed 0 produces a valid setup") {
    // The xorshift gotcha: a generator seeded 0 stays 0 forever, so the
    // implementation constructs Rng{seed ? seed : 1u}. Seed 0 is legitimate
    // caller input, so this is asserted rather than documented.
    const MysterySetup setup = generateMystery(testRoster(), 0u);

    CHECK_FALSE(setup.victim.empty());
    CHECK_FALSE(setup.killer.empty());
    CHECK_FALSE(setup.sceneZoneId.empty());
    CHECK(setup.victim != setup.killer);
}

TEST_CASE("victim and killer are different roster members") {
    const std::vector<Persona> roster = testRoster();
    std::set<std::string> names;
    for (const Persona& p : roster) names.insert(p.name);

    // A spread of seeds: one seed passing proves nothing about the pick, and
    // victim == killer is the failure that makes the mystery incoherent rather
    // than merely wrong.
    for (unsigned seed = 0; seed < 200; ++seed) {
        const MysterySetup setup = generateMystery(roster, seed);
        CHECK(setup.victim != setup.killer);
        CHECK(names.count(setup.victim) == 1);
        CHECK(names.count(setup.killer) == 1);
    }
}

TEST_CASE("a hundred seeds produce more than one victim") {
    // Guards against a generator that is deterministic AND constant.
    std::set<std::string> victims;
    for (unsigned seed = 0; seed < 100; ++seed) {
        victims.insert(generateMystery(testRoster(), seed).victim);
    }
    CHECK(victims.size() > 1);
}

TEST_CASE("a roster smaller than two fails rather than inventing a killer") {
    const MysterySetup none = generateMystery({}, 7u);
    CHECK(none.victim.empty());
    CHECK(none.killer.empty());

    std::vector<Persona> alone;
    Persona solo;
    solo.name = "Marge Holloway";
    alone.push_back(solo);

    const MysterySetup one = generateMystery(alone, 7u);
    // Empty, so match start can fail loudly. Never one person as both.
    CHECK(one.victim.empty());
    CHECK(one.killer.empty());
}

// ---- generation: the scene ----------------------------------------------

TEST_CASE("the body sits inside the scene zone") {
    // The scene zone and the body position are generated separately, so this
    // is the case that catches them drifting apart.
    for (unsigned seed = 0; seed < 100; ++seed) {
        const MysterySetup setup = generateMystery(testRoster(), seed);
        CHECK(zoneAt(setup.bodyPosition.x, setup.bodyPosition.z) ==
              setup.sceneZoneId);
    }
}

TEST_CASE("the murder happened the night before the match") {
    // Day one starts at 09:00 and the town already knows, so the murder hour
    // sits in the previous evening and never wraps past midnight.
    for (unsigned seed = 0; seed < 100; ++seed) {
        const MysterySetup setup = generateMystery(testRoster(), seed);
        CHECK(setup.murderHour >= 20.0);
        CHECK(setup.murderHour < 24.0);
    }
}

TEST_CASE("a colliding body position is moved clear of the map" *
          doctest::skip()) {
    // TODO(mystery step 3): build a City, force a setup whose bodyPosition is
    // inside a building, and check placeBodyClearOfColliders moves it to
    // somewhere circleIntersectsAny rejects no longer — while keeping it in
    // the same zone.
}

TEST_CASE("a zone with no clear spot still returns a position" *
          doctest::skip()) {
    // TODO(mystery step 3): the bounded-retry fallback. Every sample colliding
    // must yield the zone centre, not an infinite loop and not the world
    // origin. A body at (0,0,0) when the scene is the bakery is a bug that
    // looks like a placement.
}

// ---- seeding the world bus ----------------------------------------------

TEST_CASE("every survivor and the player know the death") {
    const std::vector<Persona> roster = testRoster();
    const MysterySetup setup = generateMystery(roster, 12345u);
    WorldState state;
    seedMysteryFacts(state, setup, roster);

    REQUIRE(state.facts().size() == 1);
    const std::string deathId = state.facts()[0].factId;

    int survivors = 0;
    for (const Persona& person : roster) {
        if (person.name == setup.victim) continue;
        CHECK(state.knows(person.name, deathId));
        ++survivors;
    }
    CHECK(survivors == 3);
    CHECK(state.knows("player", deathId));
}

TEST_CASE("the death fact names the place but not the hour") {
    // The hour is what an alibi is checked against, so handing it to the whole
    // town for free would remove the reason to cross-check testimony at all.
    const std::vector<Persona> roster = testRoster();
    const MysterySetup setup = generateMystery(roster, 99u);
    WorldState state;
    seedMysteryFacts(state, setup, roster);

    REQUIRE(state.facts().size() == 1);
    const std::string& content = state.facts()[0].content;

    CHECK(content.find(setup.victim) != std::string::npos);
    CHECK(content.find(zoneName(setup.sceneZoneId)) != std::string::npos);
    CHECK(content.find(clockLabel(setup.murderHour * 3600.0)) ==
          std::string::npos);
    CHECK(content.size() <= 140);
}

TEST_CASE("a non-witness does not know what a witness saw") {
    // The difference between a mystery and a briefing.
    const std::vector<Persona> roster = testRoster();
    MysterySetup setup = generateMystery(roster, 4u);
    setup.witnesses.push_back(
        {"Ray Okafor", "coffee_block", "someone hurrying away", 21.5});

    WorldState state;
    seedMysteryFacts(state, setup, roster);

    REQUIRE(state.facts().size() == 2);
    const KnownFact* testimony = nullptr;
    for (const KnownFact& fact : state.facts()) {
        if (fact.source == "Ray Okafor") testimony = &fact;
    }
    REQUIRE(testimony != nullptr);

    CHECK(state.knows("Ray Okafor", testimony->factId));
    CHECK_FALSE(state.knows("Yuki Tanaka", testimony->factId));
    CHECK_FALSE(state.knows("player", testimony->factId));
}

TEST_CASE("no committed fact names the killer as the killer") {
    // THE LEAK TEST. seedMysteryFacts is the one place the host's private
    // answer sits next to the shared bus.
    //
    // Asserts on the PAIRING, not on the presence of a name: the killer's name
    // may legitimately appear in an unrelated sighting, and "saw Ray on Coffee
    // Row" is a clue rather than a leak.
    const std::vector<Persona> roster = testRoster();
    for (unsigned seed = 0; seed < 50; ++seed) {
        const MysterySetup setup = generateMystery(roster, seed);
        WorldState state;
        seedMysteryFacts(state, setup, roster);

        for (const KnownFact& fact : state.facts()) {
            const bool namesKiller =
                fact.content.find(setup.killer) != std::string::npos;
            const bool aboutTheDeath =
                fact.content.find("dead") != std::string::npos ||
                fact.content.find("killed") != std::string::npos ||
                fact.content.find("murder") != std::string::npos;
            const bool leaks = namesKiller && aboutTheDeath;
            CHECK_FALSE(leaks);
        }
    }
}

TEST_CASE("seeding twice changes nothing") {
    // Idempotence comes free from WorldState::addFact being first-teller-wins.
    const std::vector<Persona> roster = testRoster();
    MysterySetup setup = generateMystery(roster, 8u);
    setup.witnesses.push_back(
        {"Yuki Tanaka", "plaza_block", "the lights still on", 22.0});

    WorldState state;
    state.setNumber("world_time_seconds", 1000.0);
    seedMysteryFacts(state, setup, roster);

    REQUIRE(state.facts().size() == 2);
    const double firstStamp = state.facts()[0].learnedAtSeconds;

    state.setNumber("world_time_seconds", 9999.0);
    seedMysteryFacts(state, setup, roster);

    CHECK(state.facts().size() == 2);
    CHECK(state.facts()[0].learnedAtSeconds == doctest::Approx(firstStamp));
}

TEST_CASE("a setup with no victim seeds nothing") {
    // A default MysterySetup means generation failed (roster too small). It
    // must not commit a fact about nobody dying nowhere.
    WorldState state;
    seedMysteryFacts(state, MysterySetup{}, testRoster());
    CHECK(state.facts().empty());
}

// ---- the answer ----------------------------------------------------------

TEST_CASE("voteIsCorrect is true only for the killer" * doctest::skip()) {
    // TODO(mystery step 5): true for setup.killer, false for every other
    // roster name, false for a name that is not in the roster at all, and
    // false for an empty string.
}

TEST_CASE("a setup with no killer never reports a correct vote" *
          doctest::skip()) {
    // TODO(mystery step 5): a default-constructed MysterySetup has an empty
    // killer. Accusing "" must be false, not a match on empty == empty — a
    // match with no ground truth reporting a win is the worst failure this
    // function has.
}

// ---- the victim starts dead (LIVE — markDeadAtStart is implemented) ------

TEST_CASE("the victim starts dead without taking damage") {
    Persona p;
    p.name = "Marge Holloway";
    p.role = "baker";
    Npc victim(p, idleClient());
    victim.setPlacement({-64.f, 0.f, -64.f}, 0.f, "bakery");

    REQUIRE(victim.combatState() == NpcState::Idle);
    victim.markDeadAtStart();

    CHECK(victim.combatState() == NpcState::Dead);
    CHECK(victim.hp() == 0);
}

TEST_CASE("marking an already-dead victim again is a no-op") {
    Persona p;
    p.name = "Marge Holloway";
    Npc victim(p, idleClient());
    victim.markDeadAtStart();
    victim.markDeadAtStart();

    CHECK(victim.combatState() == NpcState::Dead);
    CHECK(victim.hp() == 0);
}

TEST_CASE("the dead victim stays dead when the town calms down") {
    // calmDown() returns Fleeing and Hostile NPCs to Idle after the player
    // respawns. The murder victim must not be revived by it.
    Persona p;
    p.name = "Marge Holloway";
    Npc victim(p, idleClient());
    victim.markDeadAtStart();

    victim.calmDown();

    CHECK(victim.combatState() == NpcState::Dead);
}

TEST_CASE("wiring the victim into a match start" * doctest::skip()) {
    // TODO(mystery step 4): the integration case. Generate a setup, mark the
    // named victim dead, seed the facts, and check the roster holds exactly
    // one dead NPC and that it is setup.victim.
}
