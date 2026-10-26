// Tests for the pure day/night curves: band values and edge continuity.
#include <cmath>
#include <initializer_list>

#include "DayNight.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Colors within one 8-bit step of each other (the visible threshold).
bool nearlyEqual(const SkyColor& a, const SkyColor& b) {
    const float eps = 1.5f / 255.f;
    return std::abs(a.r - b.r) < eps && std::abs(a.g - b.g) < eps &&
           std::abs(a.b - b.b) < eps;
}

}  // namespace

TEST_CASE("bands: day is bright and blue, night is dark") {
    CHECK(lightLevelAt(12.f) == doctest::Approx(1.f));
    CHECK(lightLevelAt(0.f) == doctest::Approx(0.35f));
    CHECK(lightLevelAt(23.f) == doctest::Approx(0.35f));

    const SkyColor noon = skyColorAt(12.f);
    const SkyColor midnight = skyColorAt(0.f);
    CHECK(noon.b > 0.8f);
    CHECK(midnight.r < 0.1f);
    // Mid-dusk is the warm peak: more red than either neighbor band.
    const SkyColor dusk = skyColorAt(19.5f);
    CHECK(dusk.r > noon.r);
    CHECK(dusk.r > midnight.r);
}

TEST_CASE("curves are continuous at every band edge") {
    // A visible pop at a band boundary would read as a lighting glitch.
    for (const float edge : {5.f, 7.f, 18.f, 21.f}) {
        CAPTURE(edge);
        const float before = lightLevelAt(edge - 0.001f);
        const float after = lightLevelAt(edge + 0.001f);
        CHECK(std::abs(before - after) < 0.01f);
        CHECK(nearlyEqual(skyColorAt(edge - 0.001f), skyColorAt(edge + 0.001f)));
    }
    // Midnight wrap: 23:59:59 and 00:00:01 must match (both deep night).
    CHECK(nearlyEqual(skyColorAt(23.999f), skyColorAt(0.001f)));
}

TEST_CASE("night factor: 0 all day, 1 all night, linear across dawn and dusk") {
    // The whole day band is exactly 0 so every noir effect vanishes there.
    for (const float h : {7.f, 9.5f, 12.f, 15.f, 17.999f}) {
        CAPTURE(h);
        CHECK(nightFactorAt(h) == 0.f);
    }
    for (const float h : {21.f, 22.5f, 23.f, 0.f, 2.f, 4.999f}) {
        CAPTURE(h);
        CHECK(nightFactorAt(h) == 1.f);
    }
    // Dusk ramps up, dawn ramps down, both linearly through their midpoint.
    CHECK(nightFactorAt(18.f) == doctest::Approx(0.f));
    CHECK(nightFactorAt(19.5f) == doctest::Approx(0.5f));
    CHECK(nightFactorAt(20.25f) == doctest::Approx(0.75f));
    CHECK(nightFactorAt(5.f) == doctest::Approx(1.f));
    CHECK(nightFactorAt(6.f) == doctest::Approx(0.5f));
    CHECK(nightFactorAt(6.5f) == doctest::Approx(0.25f));
}

TEST_CASE("night factor is continuous at every band edge and the midnight wrap") {
    // A jump here would pop the whole grade on or off in one frame.
    for (const float edge : {5.f, 7.f, 18.f, 21.f}) {
        CAPTURE(edge);
        CHECK(std::abs(nightFactorAt(edge - 0.001f) - nightFactorAt(edge + 0.001f)) <
              0.01f);
    }
    CHECK(nightFactorAt(23.999f) == doctest::Approx(nightFactorAt(0.001f)));
}
