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

// Criterion (issue #170): at fogOpaqueDistance the fog mix leaves less than
// one 8-bit step of the surface's own color, and just inside it leaves more
// — the property the NPC cull radius is derived from. Checked against the
// shader's exact formula, 1 - exp(-(d * density)^2), at the shipped density.
TEST_CASE("fogOpaqueDistance is where exp2 fog crosses one 8-bit step") {
    constexpr float density = 0.006f;  // Assets::kFogDensity (app header not
                                       // includable in the no-graphics suite)
    const float d = fogOpaqueDistance(density);
    CHECK(d == doctest::Approx(392.4f).epsilon(0.01));
    const auto survives = [&](float dist) {
        const float x = dist * density;
        return std::exp(-x * x);  // the figure's surviving contribution
    };
    CHECK(survives(d) <= 1.f / 255.f + 1e-6f);
    CHECK(survives(d * 0.9f) > 1.f / 255.f);
    // Deeper density ⇒ nearer opacity, monotonically.
    CHECK(fogOpaqueDistance(0.012f) == doctest::Approx(d * 0.5f));
}

// Criterion (issue #272): sphereFullyBehind is the behind-camera reject —
// it may fire only when the WHOLE bounding sphere is strictly behind the
// camera plane, because a fired reject skips the draw entirely and a wrong
// fire is a figure missing from the screen edge.
TEST_CASE("sphereFullyBehind rejects only spheres wholly behind the plane") {
    const Vec3 eye{0.f, 1.7f, 0.f};
    const Vec3 forward{0.f, 0.f, -1.f};  // looking down -Z, unit length
    constexpr float r = 3.5f;            // kNpcBehindMargin in the renderer
    // Straight behind, beyond the margin: rejected.
    CHECK(sphereFullyBehind(eye, forward, Vec3{0.f, 0.f, 10.f}, r));
    // Behind but straddling the plane (closer than the margin): drawn.
    CHECK_FALSE(sphereFullyBehind(eye, forward, Vec3{0.f, 0.f, 3.f}, r));
    // Exactly at the margin: the strict < keeps the boundary sphere drawn.
    CHECK_FALSE(sphereFullyBehind(eye, forward, Vec3{0.f, 1.7f, 3.5f}, r));
    // In front: drawn, however far to the side (half-space, not a cone).
    CHECK_FALSE(sphereFullyBehind(eye, forward, Vec3{0.f, 0.f, -5.f}, r));
    CHECK_FALSE(sphereFullyBehind(eye, forward, Vec3{80.f, 0.f, -0.1f}, r));
    // Exactly beside the eye (on the plane): drawn.
    CHECK_FALSE(sphereFullyBehind(eye, forward, Vec3{50.f, 1.7f, 0.f}, r));
}

// Criterion (issue #272): the verdict must not depend on the forward
// vector's magnitude (the renderer passes target - position and must not
// care whether that happens to be unit length), and a degenerate zero
// forward must fail SAFE — nothing rejected, everything drawn.
TEST_CASE("sphereFullyBehind is scale-invariant in forward and fails safe") {
    const Vec3 eye{2.f, 1.7f, -8.f};
    const Vec3 forward{0.6f, 0.f, 0.8f};  // unit 3-4-5 direction
    const Vec3 behind = eye - forward * 10.f;
    const Vec3 inFront = eye + forward * 10.f;
    for (const float scale : {0.001f, 1.f, 250.f}) {
        CHECK(sphereFullyBehind(eye, forward * scale, behind, 3.5f));
        CHECK_FALSE(sphereFullyBehind(eye, forward * scale, inFront, 3.5f));
    }
    CHECK_FALSE(sphereFullyBehind(eye, Vec3{0.f, 0.f, 0.f}, behind, 3.5f));
}

// Criterion (issue #272 constraints): the renderer's 3.5 margin must bound
// everything a GATED figure can put on screen, measured from its feet
// anchor. The literals duplicate the app sources (emote billboard at
// y 2.45 size 0.55, composite hull kHullScale 1.035 on a <= ~1.9
// assembled body, pack outlines a fixed 0.022 normal push) because the
// no-graphics suite can't include app files — same accepted tradeoff as
// the fog-density literal above. The viewmodel's 1.14 rim is camera-local
// and never gated, so it is deliberately absent here.
TEST_CASE("the behind-camera margin bounds every drawn extent") {
    constexpr float margin = 3.5f;
    CHECK(2.45f + 0.55f * 0.5f < margin);    // emote billboard top
    CHECK(1.9f * 1.035f + 0.022f < margin);  // hulled composite body
}

// Criterion (issue #170 constraints): the cull radius (opaque distance plus
// its 2% margin) sits far beyond both the talk radius — the NPC you are
// talking to is mathematically un-cullable — and the town's worst-case
// eye-to-figure distance (city halfSize 110 ⇒ corner-to-corner ~311), so in
// the shipped city no visible resident is ever culled.
TEST_CASE("the derived cull radius clears dialogue and the whole town") {
    const float cull = fogOpaqueDistance(0.006f) * 1.02f;
    CHECK(cull > 392.4f);                  // exceeds full haze: no popping
    CHECK(cull / 3.5f > 100.f);            // talk radius margin, two orders
    const float townDiagonal = std::sqrt(2.f) * 2.f * 110.f;
    CHECK(cull > townDiagonal);            // nothing in town ever culled
}
