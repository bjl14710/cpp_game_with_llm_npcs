// Ground truth for the opening murder (plan: opening-murder).
//
// Generation (#178), fact seeding (#179) and body placement (#180) are live.
// The remaining skipped cases cover voteIsCorrect (#181); un-skip each in the
// commit that implements what it covers.
#include <set>
#include <string>
#include <vector>

#include "City.hpp"
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

TEST_CASE("a colliding body position is moved clear of the map") {
    const City city = City::makeDowntown();

    MysterySetup setup;
    setup.victim = "Marge Holloway";
    setup.killer = "Ray Okafor";
    setup.sceneZoneId = "bakery_block";
    // Squarely inside Marge's Bakery (-84,-64) to (-56,-40).
    setup.bodyPosition = {-70.f, 0.f, -50.f};
    REQUIRE(city.circleIntersectsAny(setup.bodyPosition.x, setup.bodyPosition.z,
                                     0.45f, 0.f));

    placeBodyClearOfColliders(setup, city);

    CHECK_FALSE(city.circleIntersectsAny(setup.bodyPosition.x,
                                         setup.bodyPosition.z, 0.45f, 0.f));
    // Still the same scene: moving the body into the street would make the
    // zone id a lie, and every clue that cites it wrong.
    CHECK(zoneAt(setup.bodyPosition.x, setup.bodyPosition.z) == "bakery_block");
}

TEST_CASE("a position that is already clear is left exactly where it is") {
    const City city = City::makeDowntown();

    MysterySetup setup;
    setup.victim = "Marge Holloway";
    setup.sceneZoneId = "plaza";
    setup.bodyPosition = {14.f, 0.f, 14.f};  // open plaza, well clear of the cart
    REQUIRE_FALSE(city.circleIntersectsAny(14.f, 14.f, 0.45f, 0.f));

    placeBodyClearOfColliders(setup, city);

    CHECK(setup.bodyPosition.x == doctest::Approx(14.f));
    CHECK(setup.bodyPosition.z == doctest::Approx(14.f));
}

TEST_CASE("every generated scene can hold a body") {
    // The real assurance: run generation across many seeds, clear each body,
    // and require the result to be walkable and still in its own zone. This is
    // what proves the lattice is fine enough for the actual map rather than
    // just for one hand-picked case.
    const City city = City::makeDowntown();
    const std::vector<Persona> roster = testRoster();

    for (unsigned seed = 0; seed < 100; ++seed) {
        MysterySetup setup = generateMystery(roster, seed);
        placeBodyClearOfColliders(setup, city);

        const bool clear = !city.circleIntersectsAny(
            setup.bodyPosition.x, setup.bodyPosition.z, 0.45f, 0.f);
        CHECK(clear);
        CHECK(zoneAt(setup.bodyPosition.x, setup.bodyPosition.z) ==
              setup.sceneZoneId);
    }
}

TEST_CASE("an unknown scene zone leaves the body where it was") {
    // Better than moving it somewhere arbitrary — and it cannot happen from
    // generateMystery, which only picks from zonesForDowntown().
    const City city = City::makeDowntown();

    MysterySetup setup;
    setup.sceneZoneId = "not_a_zone";
    setup.bodyPosition = {5.f, 0.f, 6.f};

    placeBodyClearOfColliders(setup, city);

    CHECK(setup.bodyPosition.x == doctest::Approx(5.f));
    CHECK(setup.bodyPosition.z == doctest::Approx(6.f));
}

TEST_CASE("a zone with no clear spot still returns a position in that zone") {
    // The fallback. One building swallowing the whole plaza block means every
    // lattice cell collides, and the result must be the zone centre — not the
    // world origin and not an infinite loop.
    const City city = City::fromBuildings(
        {{"slab", "", -24.f, -24.f, 24.f, 24.f, 20.f, 0}}, 110.f);

    MysterySetup setup;
    setup.sceneZoneId = "plaza";
    setup.bodyPosition = {10.f, 0.f, 10.f};

    placeBodyClearOfColliders(setup, city);

    CHECK(setup.bodyPosition.x == doctest::Approx(0.f));
    CHECK(setup.bodyPosition.z == doctest::Approx(0.f));
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
        {"Ray Okafor", "coffee_block", "someone hurrying away", 21.5, ""});

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
        {"Yuki Tanaka", "plaza_block", "the lights still on", 22.0, ""});

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

TEST_CASE("voteIsCorrect is true only for the killer") {
    const std::vector<Persona> roster = testRoster();
    const MysterySetup setup = generateMystery(roster, 77u);

    CHECK(voteIsCorrect(setup, setup.killer));

    for (const Persona& person : roster) {
        if (person.name == setup.killer) continue;
        CHECK_FALSE(voteIsCorrect(setup, person.name));
    }
    CHECK_FALSE(voteIsCorrect(setup, "Someone Not In This Town"));
    CHECK_FALSE(voteIsCorrect(setup, ""));
}

TEST_CASE("voteIsCorrect does not match loosely") {
    // Exact compare, same key WorldState and the roster use. A lenient match
    // would mean "marge holloway" convicts Marge while "Marge  Holloway" does
    // not — a rule players cannot see.
    MysterySetup setup;
    setup.victim = "Ray Okafor";
    setup.killer = "Marge Holloway";

    CHECK(voteIsCorrect(setup, "Marge Holloway"));
    CHECK_FALSE(voteIsCorrect(setup, "marge holloway"));
    CHECK_FALSE(voteIsCorrect(setup, "Marge"));
    CHECK_FALSE(voteIsCorrect(setup, " Marge Holloway "));
}

TEST_CASE("a setup with no killer never reports a correct vote") {
    // A default MysterySetup has an empty killer. Accusing "" must be false
    // rather than a match on empty == empty.
    const MysterySetup empty;
    CHECK_FALSE(voteIsCorrect(empty, ""));
    CHECK_FALSE(voteIsCorrect(empty, "Marge Holloway"));

    // And the same for a setup that generation refused to build.
    const MysterySetup failed = generateMystery({}, 5u);
    CHECK_FALSE(voteIsCorrect(failed, ""));
}

TEST_CASE("the debug reveal is compiled out by default") {
    // The gate itself. This case exists so that defining
    // LLM_NPC_REVEAL_KILLER in a shipped build breaks the suite rather than
    // quietly handing every host the answer.
#ifdef LLM_NPC_REVEAL_KILLER
    FAIL("LLM_NPC_REVEAL_KILLER is defined — never ship a build that can host "
         "a networked match with the killer reveal compiled in");
#else
    CHECK(true);
#endif
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

TEST_CASE("startVictimDead kills exactly the victim") {
    const std::vector<Persona> roster = testRoster();
    const MysterySetup setup = generateMystery(roster, 31u);

    std::vector<Npc> npcs;
    for (const Persona& p : roster) npcs.emplace_back(p, idleClient());

    REQUIRE(startVictimDead(npcs, setup));

    int dead = 0;
    for (const Npc& npc : npcs) {
        if (npc.combatState() != NpcState::Dead) continue;
        ++dead;
        CHECK(npc.persona().name == setup.victim);
    }
    CHECK(dead == 1);
}

TEST_CASE("startVictimDead reports a victim who is not in the world") {
    MysterySetup setup;
    setup.victim = "Someone Who Left Town";

    std::vector<Npc> npcs;
    Persona p;
    p.name = "Marge Holloway";
    npcs.emplace_back(p, idleClient());

    // False rather than a silent no-op: a mystery whose victim is walking
    // around is not a mystery, so the caller has to be able to see it.
    CHECK_FALSE(startVictimDead(npcs, setup));
    CHECK(npcs[0].combatState() == NpcState::Idle);
}

TEST_CASE("startVictimDead on an empty setup does nothing") {
    std::vector<Npc> npcs;
    Persona p;
    p.name = "Marge Holloway";
    npcs.emplace_back(p, idleClient());

    CHECK_FALSE(startVictimDead(npcs, MysterySetup{}));
    CHECK(npcs[0].combatState() == NpcState::Idle);
}

TEST_CASE("a full match-start sequence leaves one body and an informed town") {
    // The integration case: generate, clear the body, kill the victim, seed
    // the facts. Everything this milestone builds, in the order match start
    // will call it.
    const City city = City::makeDowntown();
    const std::vector<Persona> roster = testRoster();

    MysterySetup setup = generateMystery(roster, 2024u);
    placeBodyClearOfColliders(setup, city);

    std::vector<Npc> npcs;
    for (const Persona& p : roster) npcs.emplace_back(p, idleClient());
    REQUIRE(startVictimDead(npcs, setup));

    WorldState state;
    seedMysteryFacts(state, setup, roster);

    int dead = 0;
    for (const Npc& npc : npcs) {
        if (npc.combatState() == NpcState::Dead) ++dead;
    }
    CHECK(dead == 1);
    REQUIRE(state.facts().size() == 1);
    CHECK(state.knows("player", state.facts()[0].factId));
    CHECK_FALSE(city.circleIntersectsAny(setup.bodyPosition.x,
                                         setup.bodyPosition.z, 0.45f, 0.f));
}

TEST_CASE("the victim is moved to the body position, not left where they spawned") {
    // placeBodyClearOfColliders computed a position that nothing read, so the
    // baker died in the bakery every match whatever sceneZoneId said. "Where
    // was the body found" is the first question of any mystery and the world
    // has to agree with the answer.
    const std::vector<Persona> roster = testRoster();
    MysterySetup setup = generateMystery(roster, 11u);

    std::vector<Npc> npcs;
    for (const Persona& person : roster) {
        Npc npc(person, idleClient());
        // Somewhere that is definitely not the scene.
        npc.setPlacement(Vec3{999.f, 0.f, 999.f}, 0.f, "nowhere");
        npcs.push_back(std::move(npc));
    }

    REQUIRE(startVictimDead(npcs, setup));

    const Npc* victim = nullptr;
    for (const Npc& npc : npcs) {
        if (npc.persona().name == setup.victim) victim = &npc;
    }
    REQUIRE(victim != nullptr);
    CHECK(victim->combatState() == NpcState::Dead);
    CHECK(victim->position().x == doctest::Approx(setup.bodyPosition.x));
    CHECK(victim->position().z == doctest::Approx(setup.bodyPosition.z));

    // Everyone else stays exactly where they were.
    for (const Npc& npc : npcs) {
        if (npc.persona().name == setup.victim) continue;
        CHECK(npc.position().x == doctest::Approx(999.f));
    }
}
