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

// ---- clock ownership tripwire (issue #155) ---------------------------------
//
// Exactly one owner may write the clock per frame: free roam ticking
// advanceTime, or a match driving setTimeOfDayHours. These cover the RECORDING
// mechanism rather than the assert itself -- doctest has no death test, and a
// test that deliberately trips an assert would abort the suite rather than
// report a failure. What is checked here is that each writer is attributed
// correctly and that the tripwire stays inert outside a frame, which is what
// makes the assert both reachable and free of false positives.

TEST_CASE("the clock tripwire is inert until a frame arms it") {
    // Tests and the --hour flag write the clock outside any frame. If the
    // tripwire armed itself on construction, every one of them would be a
    // candidate for a false positive.
    WorldState state;
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::None);
    state.setTimeOfDayHours(14.0);
    state.advanceTime(1.f);
    // Unarmed: nothing was recorded, and crucially nothing tripped.
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::None);
}

TEST_CASE("free roam is recorded as the ticking owner") {
    WorldState state;
    state.beginClockFrame();
    state.advanceTime(0.016f);
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::Ticked);
}

TEST_CASE("a match is recorded as the driving owner") {
    WorldState state;
    state.beginClockFrame();
    state.setTimeOfDayHours(17.5);
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::Driven);
    CHECK(state.timeOfDayHours() == doctest::Approx(17.5));
}

TEST_CASE("the same owner may write twice in one frame") {
    // Only a SECOND, DIFFERENT owner is the error. One owner writing more than
    // once is legal -- and has to be, or a future match that recomputed the
    // hour mid-frame would trip on itself rather than on a real conflict.
    WorldState state;
    state.beginClockFrame();
    state.setTimeOfDayHours(9.0);
    state.setTimeOfDayHours(9.5);
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::Driven);
}

TEST_CASE("each frame starts with no owner, so ownership can change between frames") {
    // Leaving a match and returning to free roam is a legitimate change of
    // owner. It must not look like a conflict just because the previous frame
    // had a different writer.
    WorldState state;
    state.beginClockFrame();
    state.setTimeOfDayHours(20.0);
    REQUIRE(state.clockWriterThisFrame() == WorldState::ClockWriter::Driven);

    state.beginClockFrame();
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::None);
    state.advanceTime(0.016f);
    CHECK(state.clockWriterThisFrame() == WorldState::ClockWriter::Ticked);
}
