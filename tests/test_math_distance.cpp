// Tests for distanceSquared (issue #171): the range gate that decides where
// the inverted-hull outline stops drawing compares this against a squared
// threshold, so the exact boundary behaviour — full 3D, strict less-than,
// symmetric — is load-bearing for a visible rendering cutoff.
#include <cmath>

#include "Math.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("distanceSquared is the square of the Euclidean distance") {
    const Vec3 a{1.f, 2.f, 3.f};
    const Vec3 b{4.f, 6.f, 3.f};  // 3-4-5 triangle in the XY plane
    CHECK(distanceSquared(a, b) == doctest::Approx(25.f));
    CHECK(distanceSquared(a, b) ==
          doctest::Approx(length(a - b) * length(a - b)));
}

TEST_CASE("distanceSquared is symmetric and zero at coincidence") {
    const Vec3 a{-7.5f, 0.25f, 12.f};
    const Vec3 b{3.f, -9.f, 0.5f};
    CHECK(distanceSquared(a, b) == doctest::Approx(distanceSquared(b, a)));
    CHECK(distanceSquared(a, a) == doctest::Approx(0.f));
}

// Criterion: the vertical axis participates. distanceXZ would call an
// overhead camera "close"; the outline gate must not (a sandbox camera 40
// units straight up is 40 units away, and the rim it would buy is invisible).
TEST_CASE("distanceSquared counts the vertical axis, unlike distanceXZ") {
    const Vec3 ground{0.f, 0.f, 0.f};
    const Vec3 overhead{0.f, 40.f, 0.f};
    CHECK(distanceSquared(ground, overhead) == doctest::Approx(1600.f));
    CHECK(distanceXZ(ground, overhead) == doctest::Approx(0.f));
}

// Criterion: a range gate `distanceSquared < r*r` flips exactly at r —
// mirrors the outline cutoff at 25 units, including a point reached
// diagonally (24 back, 9.9 aside — the qa-lineup edge case, which sits just
// PAST the threshold while the lineup centre is inside it).
TEST_CASE("squared-threshold gate flips exactly at the radius") {
    constexpr float r = 25.f;
    const Vec3 eye{0.f, 1.7f, 24.f};
    const Vec3 centre{0.f, 0.f, 0.f};      // ~24.06 away: inside
    const Vec3 edge{9.9f, 0.f, 0.f};       // ~25.98 away: outside
    CHECK(distanceSquared(eye, centre) < r * r);
    CHECK_FALSE(distanceSquared(eye, edge) < r * r);
}
