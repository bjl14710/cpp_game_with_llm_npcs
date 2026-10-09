// Tests for SignLayout (issue #286, plan .claude/plans/store-signs.md): the
// placement math behind the painted-hardwood storefront signs, and the
// predicate that decides whether the #166 HUD label still draws.
//
// SCAFFOLD -- every case below is skipped until SignLayout.cpp is
// implemented. These stubs ARE the contract: each one maps to an acceptance
// criterion in the plan. Remove the doctest::skip() decorator as each is
// filled in; do not weaken a CHECK to make one pass.
#include "City.hpp"
#include "SignLayout.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// The six named storefronts, straight out of City.cpp:27-34, so the numbers
// under test are the numbers the game actually ships.
Building bakery() {
    return Building{"bakery", "Marge's Bakery", -84.f, -64.f, -56.f, -40.f, 12.f, 0};
}
Building library() {
    return Building{"library", "City Library", -80.f, -16.f, -40.f, 16.f, 15.f, 3};
}
// Six by four units of footprint and three tall -- the reason
// boardTopOverride exists at all.
Building cart() {
    return Building{"cart", "Gus's Hot Dogs", -3.f, -2.f, 3.f, 2.f, 3.f, 5};
}

}  // namespace

TEST_CASE("face normals point out of the building" * doctest::skip()) {
    CHECK(signFaceNormal(SignFace::PlusZ).z == doctest::Approx(1.f));
    CHECK(signFaceNormal(SignFace::MinusZ).z == doctest::Approx(-1.f));
    CHECK(signFaceNormal(SignFace::PlusX).x == doctest::Approx(1.f));
    CHECK(signFaceNormal(SignFace::MinusX).x == doctest::Approx(-1.f));
    // No face tilts. A sign leaning out of the wall is a bug, not a style.
    CHECK(signFaceNormal(SignFace::PlusZ).y == doctest::Approx(0.f));
}

TEST_CASE("a wide shopfront takes the full board, capped" * doctest::skip()) {
    // The bakery's +Z face is 28 units across; 72% of that is 20.16, which
    // must clamp to the 6.4 maximum rather than growing a billboard.
    const SignPlacement p = signPlacementFor(bakery(), SignSpec{});
    REQUIRE(p.valid);
    CHECK(p.boardWidth == doctest::Approx(kBoardMaxWidth));
    CHECK(p.boardHeight == doctest::Approx(kBoardMaxWidth * kBoardAspect));
}

TEST_CASE("the board ratio holds when the board scales down" * doctest::skip()) {
    // Whatever width a front yields, height tracks it. A squashed wordmark
    // is worse than a small one.
    const SignPlacement p = signPlacementFor(cart(), SignSpec{});
    REQUIRE(p.valid);
    CHECK(p.boardHeight / p.boardWidth == doctest::Approx(kBoardAspect));
}

TEST_CASE("a tall building takes the default 4.6u board line" * doctest::skip()) {
    // Every downtown shop is 10-16u, so all of them land on the default.
    const SignPlacement p = signPlacementFor(bakery(), SignSpec{});
    REQUIRE(p.valid);
    const float top = p.boardCenter.y + p.boardHeight * 0.5f;
    CHECK(top == doctest::Approx(kBoardDefaultTop));
}

TEST_CASE("the cart override puts its board at umbrella height" * doctest::skip()) {
    // Acceptance criterion: the cart's board neither floats above it nor
    // clips through it.
    SignSpec spec;
    spec.boardTopOverride = 1.6f;
    const SignPlacement p = signPlacementFor(cart(), spec);
    REQUIRE(p.valid);
    const float top = p.boardCenter.y + p.boardHeight * 0.5f;
    CHECK(top == doctest::Approx(1.6f));
    CHECK(p.boardCenter.y - p.boardHeight * 0.5f > 0.f);  // above ground
}

TEST_CASE("a narrow front clamps rather than vanishing" * doctest::skip()) {
    // Documented tradeoff: below 1.6u the board overhangs the face. An
    // overhanging sign is readable; a sub-metre one is not.
    Building narrow{"kiosk", "Kiosk", 0.f, 0.f, 1.f, 1.f, 8.f, 0};
    const SignPlacement p = signPlacementFor(narrow, SignSpec{});
    REQUIRE(p.valid);
    CHECK(p.boardWidth == doctest::Approx(kBoardMinWidth));
}

TEST_CASE("the board stands proud of the face it mounts on" * doctest::skip()) {
    // Coplanar with the wall means z-fighting, which reads as a flickering
    // sign at exactly the distance you stop to read it.
    const SignPlacement p = signPlacementFor(bakery(), SignSpec{});
    REQUIRE(p.valid);
    CHECK(p.boardCenter.z > bakery().maxZ);
    CHECK(p.bladeCenter.z > p.boardCenter.z);  // blade projects further out
}

TEST_CASE("the face override moves the whole sign" * doctest::skip()) {
    // The library's +Z face looks north while the plaza is due east, so its
    // row is expected to override. Board, fascia and blade must all follow.
    SignSpec spec;
    spec.face = SignFace::PlusX;
    const SignPlacement p = signPlacementFor(library(), spec);
    REQUIRE(p.valid);
    CHECK(p.outwardNormal.x == doctest::Approx(1.f));
    CHECK(p.boardCenter.x > library().maxX);
    CHECK(p.fasciaCenter.x > library().maxX);
    // A +X face is sized off the Z extent, not the X extent.
    CHECK(p.fasciaWidth == doctest::Approx(library().maxZ - library().minZ));
}

TEST_CASE("unnamed and degenerate buildings carry no sign" * doctest::skip()) {
    // Filler and obstacles have an empty name -- same rule the #166 pass
    // already applies.
    Building filler{"filler", "", 0.f, 0.f, 10.f, 10.f, 8.f, 0};
    CHECK_FALSE(signPlacementFor(filler, SignSpec{}).valid);
    Building flat{"flat", "Flat", 5.f, 5.f, 5.f, 5.f, 8.f, 0};
    CHECK_FALSE(signPlacementFor(flat, SignSpec{}).valid);
}

TEST_CASE("a building with no world sign keeps its HUD label" * doctest::skip()) {
    // Sandbox and uncurated ids must behave exactly as they do today, at
    // every range out to kSignageRange. This is the no-regression case.
    CHECK(hudLabelWanted(2.f, false, 1.f));
    CHECK(hudLabelWanted(kSignLegibleRange - 1.f, false, 1.f));
    CHECK(hudLabelWanted(85.f, false, -1.f));
}

TEST_CASE("the HUD label survives beyond sign-legible range" * doctest::skip()) {
    // You cannot read a 6.4u board across the plaza; the label is the
    // navigation aid and it stays.
    CHECK(hudLabelWanted(kSignLegibleRange + 1.f, true, 1.f));
    CHECK(hudLabelWanted(85.f, true, 1.f));
}

TEST_CASE("a sign facing you retires its HUD label" * doctest::skip()) {
    // The acceptance criterion the play report in #286 is about: standing
    // outside Marge's, you get a board and no amber sticker.
    CHECK_FALSE(hudLabelWanted(6.f, true, 1.f));
    CHECK_FALSE(hudLabelWanted(kSignLegibleRange - 0.1f, true, 0.2f));
}

TEST_CASE("a sign facing away keeps its HUD label" * doctest::skip()) {
    // The walk-around-the-block hole: near enough to matter, but the board
    // is on a face you cannot see.
    CHECK(hudLabelWanted(6.f, true, -1.f));
    // Exactly edge-on counts as facing away -- a board seen at zero degrees
    // is a line.
    CHECK(hudLabelWanted(6.f, true, 0.f));
}

TEST_CASE("the range boundary is not a gap" * doctest::skip()) {
    // Whichever way the boundary falls, a label must be drawn on exactly one
    // side of it and never on neither.
    CHECK(hudLabelWanted(kSignLegibleRange, true, 1.f) !=
          hudLabelWanted(kSignLegibleRange + 0.01f, true, 1.f));
}

TEST_CASE("sandbox instance ids resolve to their base sign" * doctest::skip()) {
    // Same dispatch the fountain already relies on at
    // RaylibRenderer.cpp:347. Without this, a placed bakery gets no sign.
    CHECK(signBaseId("bakery#4") == "bakery");
    CHECK(signBaseId("bakery") == "bakery");
    CHECK(signBaseId("") == "");
    CHECK(signBaseId("#7") == "");
}
