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
// [[maybe_unused]] because every case that calls it is currently skipped, and
// -Wunused-function is an error waiting to happen otherwise. Drop the attribute
// once the generation cases are live. (First use of [[maybe_unused]] in this
// codebase — the alternative, calling it from a live case, would mean asserting
// against a stub and passing for the wrong reason.)
[[maybe_unused]] std::vector<Persona> testRoster() {
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

TEST_CASE("the same roster and seed produce an identical setup" *
          doctest::skip()) {
    // TODO(mystery step 1): generate twice with seed 12345 and compare victim,
    // killer, sceneZoneId, murderHour and bodyPosition. This is the property
    // every other assertion in this file leans on — without it, "given seed X
    // the victim is Y" cannot be written at all.
}

TEST_CASE("seed 0 produces a valid setup" * doctest::skip()) {
    // TODO(mystery step 1): the xorshift gotcha. A generator seeded 0 stays 0
    // forever, so the implementation must construct Rng{seed ? seed : 1u}.
    // Seed 0 is legitimate caller input, so assert victim and killer are both
    // non-empty here rather than documenting the constraint.
}

TEST_CASE("victim and killer are different roster members" * doctest::skip()) {
    // TODO(mystery step 1): both names appear in the roster, and they differ.
    // Run it over a spread of seeds — a single seed passing proves nothing
    // about the pick, and victim == killer is the failure that would make the
    // whole mystery incoherent rather than merely wrong.
}

TEST_CASE("a hundred seeds produce more than one victim" * doctest::skip()) {
    // TODO(mystery step 1): guards against a generator that is deterministic
    // AND constant, which would pass every case above. Collect victims across
    // 100 seeds into a std::set and require size() > 1.
}

TEST_CASE("a roster smaller than two fails rather than inventing a killer" *
          doctest::skip()) {
    // TODO(mystery step 1): a one-person roster cannot satisfy
    // victim != killer. The setup comes back empty so match start can fail
    // loudly; it must never make one person both the victim and the killer.
}

// ---- generation: the scene ----------------------------------------------

TEST_CASE("the body sits inside the scene zone" * doctest::skip()) {
    // TODO(mystery step 1): zoneAt(bodyPosition.x, bodyPosition.z) equals
    // setup.sceneZoneId. The scene zone and the body position are generated
    // separately, so this is the case that catches them drifting apart.
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

TEST_CASE("every survivor and the player know the death" * doctest::skip()) {
    // TODO(mystery step 2): after seedMysteryFacts, state.knows(name, deathId)
    // holds for all three survivors and for "player". The victim is not
    // required to know anything.
}

TEST_CASE("a non-witness does not know what a witness saw" * doctest::skip()) {
    // TODO(mystery step 2): the observation is granted to the witness ALONE.
    // If everyone starts knowing every observation there is nothing to
    // investigate — this case is the difference between a mystery and a
    // briefing.
}

TEST_CASE("no committed fact names the killer as the killer" *
          doctest::skip()) {
    // TODO(mystery step 2): THE LEAK TEST. Walk state.facts() and assert no
    // content ties setup.killer to the murder. seedMysteryFacts is the one
    // place the host's private answer sits next to the shared bus, so this is
    // the assertion that keeps defence 1 honest.
    //
    // Note the victim's name legitimately appears in the death fact, and the
    // killer's name may legitimately appear in an unrelated witness sighting —
    // "saw Ray on Coffee Row" is a clue, not a leak. Assert on the pairing,
    // not on the presence of the name.
}

TEST_CASE("seeding twice changes nothing" * doctest::skip()) {
    // TODO(mystery step 2): idempotence, which comes free from
    // WorldState::addFact being first-teller-wins. Call seedMysteryFacts
    // twice and check facts().size() is unchanged and no timestamp moved.
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
