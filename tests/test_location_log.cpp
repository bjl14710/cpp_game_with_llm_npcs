// Agent position history as zone intervals (issue #164).
//
// Positions here are chosen from the downtown block grid: block centres are at
// -64/0/64 on each axis and blocks span +-24, so +-32 is mid-street.
#include <string>

#include "LocationLog.hpp"
#include "Zones.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Readable coordinates for the zones these cases move between.
constexpr float kBakeryX = -64.f, kBakeryZ = -64.f;
constexpr float kPlazaX = 0.f, kPlazaZ = 0.f;
constexpr float kStreetX = -32.f, kStreetZ = 0.f;

}  // namespace

TEST_CASE("standing still records one visit, not one per observation") {
    // The whole point of recording transitions instead of samples. Ticking
    // every frame for 21 NPCs must not produce 30,000 rows a day.
    LocationLog log;
    for (int i = 0; i < 50; ++i) {
        log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0 + i * 0.01);
    }
    CHECK(log.visits().size() == 1);
    CHECK(log.visits()[0].zoneId == "bakery_block");
    CHECK(log.visits()[0].ongoing);
}

TEST_CASE("moving between zones closes one visit and opens the next") {
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    log.observe("Marge Holloway", kStreetX, kStreetZ, 10.0);
    log.observe("Marge Holloway", kPlazaX, kPlazaZ, 11.0);

    REQUIRE(log.visits().size() == 3);
    CHECK(log.visits()[0].zoneId == "bakery_block");
    CHECK(log.visits()[0].endHour == doctest::Approx(10.0));
    CHECK_FALSE(log.visits()[0].ongoing);
    CHECK(log.visits()[1].zoneId == kStreetsZoneId);
    CHECK(log.visits()[1].endHour == doctest::Approx(11.0));
    CHECK(log.visits()[2].zoneId == "plaza");
    CHECK(log.visits()[2].ongoing);
}

TEST_CASE("whoWasIn returns stays that OVERLAP the window, not only contained ones") {
    // The case that matters: someone who arrived at 13:50 and left at 14:10
    // WAS in the bakery during 14:00-15:00, and an alibi that misses them is
    // worse than useless.
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 13.50);
    log.observe("Marge Holloway", kPlazaX, kPlazaZ, 14.17);   // left at 14:10ish

    log.observe("Theo", kBakeryX, kBakeryZ, 13.00);
    log.observe("Theo", kPlazaX, kPlazaZ, 13.98);             // left before 14:00

    const auto found = log.whoWasIn("bakery_block", 14.0, 15.0);
    REQUIRE(found.size() == 1);
    CHECK(found[0].agent == "Marge Holloway");
}

TEST_CASE("whoWasIn includes an ongoing stay that began before the window") {
    // An agent who walked in at 13:00 and never left is still there at 14:30.
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 13.0);
    const auto found = log.whoWasIn("bakery_block", 14.0, 15.0);
    REQUIRE(found.size() == 1);
    CHECK(found[0].ongoing);
}

TEST_CASE("trailOf returns one agent's stays in chronological order") {
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    log.observe("Marge Holloway", kStreetX, kStreetZ, 10.0);
    log.observe("Theo", kPlazaX, kPlazaZ, 9.5);

    const auto trail = log.trailOf("Marge Holloway", 8.0, 12.0);
    REQUIRE(trail.size() == 2);
    CHECK(trail[0].zoneId == "bakery_block");
    CHECK(trail[1].zoneId == kStreetsZoneId);
    CHECK(trail[0].startHour < trail[1].startHour);
}

TEST_CASE("the player is logged like anyone else") {
    // The retaliation rule means the player has to be as observable as any
    // resident — an alibi system that cannot place the player is half a system.
    LocationLog log;
    log.observe("player", kPlazaX, kPlazaZ, 9.0);
    const auto trail = log.trailOf("player", 8.0, 10.0);
    REQUIRE(trail.size() == 1);
    CHECK(trail[0].agent == "player");
}

TEST_CASE("an inverted window returns empty rather than swapping bounds") {
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    CHECK(log.whoWasIn("bakery_block", 15.0, 14.0).empty());
    CHECK(log.trailOf("Marge Holloway", 15.0, 14.0).empty());
}

TEST_CASE("an unknown zone returns empty, not an error") {
    // Asking about a zone that does not exist gets "nobody", which is true.
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    CHECK(log.whoWasIn("no_such_zone", 0.0, 24.0).empty());
}

TEST_CASE("closeAgent ends an ongoing stay so a corpse stops accumulating") {
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    log.closeAgent("Marge Holloway", 12.0);

    REQUIRE(log.visits().size() == 1);
    CHECK_FALSE(log.visits()[0].ongoing);
    CHECK(log.visits()[0].endHour == doctest::Approx(12.0));

    // Further observation must not resurrect them into a new stay.
    log.observe("Marge Holloway", kPlazaX, kPlazaZ, 13.0);
    CHECK(log.visits().size() == 1);
}

TEST_CASE("clear empties both queries") {
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    log.clear();
    CHECK(log.visits().empty());
    CHECK(log.whoWasIn("bakery_block", 0.0, 24.0).empty());
    CHECK(log.trailOf("Marge Holloway", 0.0, 24.0).empty());
}

TEST_CASE("two agents in the same zone are both recorded") {
    // Co-presence is what witnessing will later be derived from, so the log
    // must keep both stays rather than collapsing them.
    LocationLog log;
    log.observe("Marge Holloway", kBakeryX, kBakeryZ, 9.0);
    log.observe("Theo", kBakeryX, kBakeryZ, 9.5);

    const auto found = log.whoWasIn("bakery_block", 9.0, 10.0);
    CHECK(found.size() == 2);
}

// ---- the game-loop wiring (issue #165) -------------------------------------

TEST_CASE("retired reports whether an agent has been closed") {
    LocationLog log;
    log.observe("Marge Holloway", 0.f, 0.f, 9.0);
    CHECK_FALSE(log.retired("Marge Holloway"));
    CHECK_FALSE(log.retired("nobody at all"));

    log.closeAgent("Marge Holloway", 10.0);
    CHECK(log.retired("Marge Holloway"));
}

TEST_CASE("the corpse pattern: close once, then keep ticking harmlessly") {
    // What main.cpp does every frame. The body is still observed by the loop
    // after death, so the guard has to survive being run repeatedly.
    LocationLog log;
    log.observe("Hal Jensen", 0.f, 0.f, 9.0);
    log.observe("Hal Jensen", 0.f, 0.f, 9.5);

    log.closeAgent("Hal Jensen", 10.0);
    const std::size_t afterDeath = log.visits().size();

    // 200 frames of the loop running over a corpse.
    for (int frame = 0; frame < 200; ++frame) {
        if (!log.retired("Hal Jensen")) {
            log.closeAgent("Hal Jensen", 10.0 + frame * 0.01);
        }
        log.observe("Hal Jensen", 0.f, 0.f, 10.0 + frame * 0.01);
    }

    // No new stays, and the closing time is the moment of death — not the
    // last frame the body happened to be looked at.
    CHECK(log.visits().size() == afterDeath);
    const auto trail = log.trailOf("Hal Jensen", 0.0, 24.0);
    REQUIRE_FALSE(trail.empty());
    CHECK_FALSE(trail.back().ongoing);
    CHECK(trail.back().endHour == doctest::Approx(10.0));
}

TEST_CASE("the player is logged like anyone else") {
    // The retaliation rule means a player has to be as observable as any
    // resident. An alibi system that cannot place the player is half a system.
    LocationLog log;
    log.observe("player", 0.f, 0.f, 9.0);
    log.observe("player", -64.f, -64.f, 9.5);
    log.observe("player", 0.f, 0.f, 10.0);

    const auto trail = log.trailOf("player", 0.0, 24.0);
    REQUIRE(trail.size() == 3);
    CHECK(trail[0].agent == "player");
    CHECK(trail[0].zoneId != trail[1].zoneId);
    CHECK(trail[2].zoneId == trail[0].zoneId);  // came back
}
