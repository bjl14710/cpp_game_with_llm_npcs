// Tests for labelBoxesOverlap (issue #274): the predicate behind the one
// shared placed-label pass in main.cpp. Nameplates claim screen boxes
// first, signage takes what is left, and a wrong verdict here is a garbled
// or missing label on the HUD — so the boundary behaviour (strict <, both
// axes required, symmetric) is load-bearing.
#include "Math.hpp"
#include "doctest.h"

using namespace llm_npc;

// The clearance the HUD uses (kLabelClearX/kLabelClearY in main.cpp — app
// literals duplicated because the no-graphics suite can't include app
// files, same accepted tradeoff as the fog-density literal in
// test_math_distance.cpp).
namespace {
constexpr float kClearX = 140.f;
constexpr float kClearY = 20.f;
}  // namespace

TEST_CASE("labels collide only when both axes are inside the clear box") {
    // Dead overlap: the #274 repro — a nameplate on top of a sign.
    CHECK(labelBoxesOverlap(640.f, 300.f, 640.f, 300.f, kClearX, kClearY));
    CHECK(labelBoxesOverlap(640.f, 300.f, 700.f, 310.f, kClearX, kClearY));
    // Near on x but clear on y (stacked labels): no collision — vertical
    // separation is how two labels on the same bearing coexist.
    CHECK_FALSE(labelBoxesOverlap(640.f, 300.f, 640.f, 340.f, kClearX, kClearY));
    // Near on y but clear on x (side by side): no collision.
    CHECK_FALSE(labelBoxesOverlap(640.f, 300.f, 800.f, 300.f, kClearX, kClearY));
}

TEST_CASE("the clear-box boundary is strict and the test symmetric") {
    // Exactly one clearance apart on either axis: NOT a collision, so a
    // label may sit flush at the reserved edge (strict <, matching every
    // other gate in Math.hpp).
    CHECK_FALSE(labelBoxesOverlap(0.f, 0.f, kClearX, 0.f, kClearX, kClearY));
    CHECK_FALSE(labelBoxesOverlap(0.f, 0.f, 0.f, kClearY, kClearX, kClearY));
    // A hair inside on both axes: collision.
    CHECK(labelBoxesOverlap(0.f, 0.f, kClearX - 0.5f, kClearY - 0.5f, kClearX,
                            kClearY));
    // A hair inside on ONE axis while the other is well clear: no collision
    // — both axes are required, right up against the boundary.
    CHECK_FALSE(labelBoxesOverlap(0.f, 0.f, kClearX - 0.5f, 300.f, kClearX,
                                  kClearY));
    CHECK_FALSE(labelBoxesOverlap(0.f, 0.f, 500.f, kClearY - 0.5f, kClearX,
                                  kClearY));
    // Order of the two anchors cannot matter.
    CHECK(labelBoxesOverlap(10.f, 5.f, 60.f, 12.f, kClearX, kClearY) ==
          labelBoxesOverlap(60.f, 12.f, 10.f, 5.f, kClearX, kClearY));
    // Negative coordinates (a label just off the left screen edge still
    // reserves its box).
    CHECK(labelBoxesOverlap(-30.f, 10.f, 40.f, 10.f, kClearX, kClearY));
}
