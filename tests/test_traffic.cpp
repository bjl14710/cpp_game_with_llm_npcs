// Tests for Traffic: cars hold their lanes, stay in bounds, brake for the
// player, and never overlap them.
#include <cmath>

#include "Math.hpp"
#include "Traffic.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {
const Vec3 kFarAway{1.0e4f, 0.f, 1.0e4f};  // player nowhere near the roads
}

TEST_CASE("cars hold their avenue lanes and stay within wrap bounds") {
    Traffic traffic(170.f, 7u, 16);
    for (int i = 0; i < 2000; ++i) traffic.update(0.05f, kFarAway);
    for (const Vehicle& v : traffic.vehicles()) {
        // The perpendicular coordinate is pinned to the avenue line +- offset.
        const float perp = v.axis == 0 ? v.x : v.z;
        CHECK(std::fabs(perp - v.line) == doctest::Approx(3.0f).epsilon(0.001));
        // The travel coordinate stays within the wrap bounds.
        const float along = v.axis == 0 ? v.z : v.x;
        CHECK(std::fabs(along) <= 175.f);
    }
}

TEST_CASE("a lone car brakes rather than driving over the player in its lane") {
    Traffic t(170.f, 5u, 1);
    t.update(0.01f, kFarAway);  // settle the initial pose
    const Vehicle car = t.vehicles().front();

    // Put the player 5 units directly ahead of the car, in its lane.
    const float h = car.headingDeg * 3.14159265f / 180.f;
    const Vec3 player{car.x + std::sin(h) * 5.f, 0.f, car.z + std::cos(h) * 5.f};

    bool everHit = false;
    for (int i = 0; i < 300; ++i) {
        t.update(0.05f, player);
        if (t.circleHitsVehicle(player.x, player.z, 0.45f)) everHit = true;
    }
    CHECK_FALSE(everHit);                          // never ran the player over
    CHECK(t.vehicles().front().speed == doctest::Approx(0.f));  // and it stopped
}

TEST_CASE("a car brakes for a pedestrian ahead in its lane") {
    Traffic t(170.f, 5u, 1);
    t.update(0.01f, kFarAway);  // settle the initial pose
    const Vehicle car = t.vehicles().front();

    // A pedestrian standing 5 units directly ahead of the car, in its lane.
    const float h = car.headingDeg * 3.14159265f / 180.f;
    const std::vector<Vec3> people = {{car.x + std::sin(h) * 5.f, 0.f, car.z + std::cos(h) * 5.f}};

    bool everHit = false;
    for (int i = 0; i < 300; ++i) {
        t.update(0.05f, kFarAway, people);  // player far away; only the pedestrian matters
        if (t.circleHitsVehicle(people[0].x, people[0].z, 0.45f)) everHit = true;
    }
    CHECK_FALSE(everHit);                                       // never ran the pedestrian over
    CHECK(t.vehicles().front().speed == doctest::Approx(0.f));  // and it stopped for them
}

TEST_CASE("traffic is deterministic for a given seed") {
    Traffic a(170.f, 42u, 12);
    Traffic b(170.f, 42u, 12);
    for (int i = 0; i < 100; ++i) {
        a.update(0.05f, Vec3{0.f, 0.f, 0.f});
        b.update(0.05f, Vec3{0.f, 0.f, 0.f});
    }
    REQUIRE(a.vehicles().size() == b.vehicles().size());
    for (std::size_t i = 0; i < a.vehicles().size(); ++i) {
        CHECK(a.vehicles()[i].x == doctest::Approx(b.vehicles()[i].x));
        CHECK(a.vehicles()[i].z == doctest::Approx(b.vehicles()[i].z));
    }
}

TEST_CASE("circleHitsVehicle detects the body and clear space") {
    Traffic t(170.f, 9u, 1);
    const Vehicle v = t.vehicles().front();
    CHECK(t.circleHitsVehicle(v.x, v.z, 0.4f));               // dead center
    CHECK_FALSE(t.circleHitsVehicle(v.x + 30.f, v.z, 0.4f));  // far to the side
}
