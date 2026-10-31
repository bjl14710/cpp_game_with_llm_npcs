// Tests for the pure day/night curves: band values and edge continuity.
#include <cmath>

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
