// Tests for the world bus: generic facts and the shared clock.
#include "WorldState.hpp"
#include "doctest.h"

using llm_npc::WorldState;

TEST_CASE("number and text facts round-trip; missing keys fall back") {
    WorldState state;
    CHECK(state.number("nope", 4.5) == doctest::Approx(4.5));
    CHECK(state.text("nope") == nullptr);
    CHECK_FALSE(state.has("nope"));

    state.setNumber("gossip.rumor_count", 3.0);
    state.setText("gossip.rumor_1", "the fountain is haunted");
    CHECK(state.number("gossip.rumor_count") == doctest::Approx(3.0));
    REQUIRE(state.text("gossip.rumor_1") != nullptr);
    CHECK(*state.text("gossip.rumor_1") == "the fountain is haunted");
    CHECK(state.has("gossip.rumor_1"));

    // A fact can carry both a number and text under one key.
    state.setNumber("gossip.rumor_1", 0.8);
    CHECK(*state.text("gossip.rumor_1") == "the fountain is haunted");
    CHECK(state.number("gossip.rumor_1") == doctest::Approx(0.8));
}

TEST_CASE("the clock starts at 09:00 and advances at one game minute per real second") {
    WorldState state;
    CHECK(state.timeOfDayHours() == doctest::Approx(9.0));

    state.advanceTime(60.f);  // one real minute = one game hour
    CHECK(state.timeOfDayHours() == doctest::Approx(10.0));

    // Two readers of the bus see the same value — there is only one time.
    const double a = state.timeOfDayHours();
    const double b = state.number("world_time_seconds") / 3600.0;
    CHECK(a == doctest::Approx(b));
}

TEST_CASE("the clock wraps at midnight") {
    WorldState state;
    state.setTimeOfDayHours(23.5);
    state.advanceTime(60.f);  // +1 game hour -> 00:30
    CHECK(state.timeOfDayHours() == doctest::Approx(0.5));

    state.setTimeOfDayHours(31.0);  // out-of-range set normalizes too
    CHECK(state.timeOfDayHours() == doctest::Approx(7.0));
}
