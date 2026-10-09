#pragma once

#include <string>

#include "City.hpp"
#include "Math.hpp"

namespace llm_npc {

// Placement math for the painted-hardwood storefront signs (plan:
// .claude/plans/store-signs.md, issue #286; design "Store Signs.dc.html"
// artboard 1a).
//
// This lives in core, not in the renderer, for one reason: it is the only
// part of the sign feature that can be tested. tests/ builds without
// graphics, so anything that touches raylib is unverifiable there. Every
// decision with a right and a wrong answer — how wide the board is, how high
// it sits, which way it faces, and whether the #166 HUD label should still
// draw — is a pure function over Building and belongs here.
//
// BUILD TRAP: SignLayout.cpp is globbed automatically by tests/Makefile but
// must be listed EXPLICITLY in CMakeLists.txt. A green test suite does not
// prove the app links.

// Which face of a building's axis-aligned footprint carries the sign.
//
// PlusZ is the default because the KayKit city pieces face +Z at identity
// (RaylibRenderer.cpp:47) — that is the face with the shopfront art on it,
// so a sign anywhere else is pasted onto a blank wall. The override exists
// because the downtown blocks have streets on all four sides: the library's
// +Z face looks north while the plaza it serves is due east.
enum class SignFace { PlusZ, MinusZ, PlusX, MinusX };

// The curated mount recipe for one named storefront. Keyed in Assets by the
// building's BASE id, so sandbox instances ("bakery#4") hit the same row.
struct SignSpec {
    SignFace face = SignFace::PlusZ;
    // World y of the board's top edge. <= 0 means "use the default rule".
    // Gus's cart sets 1.6 (the design's umbrella height) because a 3-unit
    // cart cannot carry a board at the 4.6 shopfront line.
    float boardTopOverride = 0.f;
    // Emblem tile baked left of the wordmark: "loaves", "cup", "hammer",
    // "books", "shield", "hotdog". Empty draws the wordmark alone.
    std::string emblem;
};

// A resolved sign, in world space, ready for the renderer to draw. All
// centers already include the outward stand-off from the building face.
struct SignPlacement {
    bool valid = false;  // false when the building cannot carry a sign
    Vec3 outwardNormal;  // unit, points away from the building face
    Vec3 boardCenter;
    float boardWidth = 0.f;
    float boardHeight = 0.f;
    Vec3 fasciaCenter;
    float fasciaWidth = 0.f;
    Vec3 bladeCenter;  // perpendicular bracket blade, out over the pavement
};

// Board geometry, world units, from design artboard 1a. The ratio is held
// when the board scales down, so a narrow shopfront gets a smaller sign
// rather than a squashed one.
constexpr float kBoardMaxWidth = 6.4f;
constexpr float kBoardMinWidth = 1.6f;
constexpr float kBoardAspect = 1.4f / 6.4f;   // height / width
constexpr float kBoardThickness = 0.18f;
constexpr float kBoardFrontFraction = 0.72f;  // of the front face width
constexpr float kBoardDefaultTop = 4.6f;
// A board must clear the roofline by this much, or a short structure wears
// its sign like a hat.
constexpr float kBoardTopClearance = 0.6f;

constexpr float kFasciaTop = 3.4f;
constexpr float kFasciaHeight = 0.24f;
constexpr float kFasciaThickness = 0.10f;

constexpr float kBladeWidth = 2.4f;
constexpr float kBladeHeight = 1.0f;
constexpr float kBladeThickness = 0.10f;
constexpr float kBladeOut = 1.6f;   // projection from the face
constexpr float kBladeArm = 0.12f;  // arm the blade hangs off

// How far the sign geometry stands proud of the building face, so the board
// never z-fights the wall it is mounted on.
constexpr float kSignProud = 0.09f;

// Inside this range a 6.4-unit board is readable and the #166 HUD label
// stands down. Outside it the label is the navigation aid — you cannot read
// a storefront from across the plaza, and that is what the label is for.
// Signage itself still ranges to kSignageRange (90) in main.cpp.
constexpr float kSignLegibleRange = 42.f;

// Unit outward normal for a face.
Vec3 signFaceNormal(SignFace face);

// Resolves the full sign placement for a building, or an invalid placement
// if it cannot carry one (no name, zero footprint, or too short for even the
// minimum board under its override).
SignPlacement signPlacementFor(const Building& building, const SignSpec& spec);

// Should the screen-space building label from #166 still draw?
//
// The four cases, in the order they matter:
//   - no world sign at all (sandbox, uncurated) -> always, unchanged from #166
//   - world sign but beyond kSignLegibleRange   -> yes, long-range navigation
//   - world sign, near, facing you              -> NO, the board is doing it
//   - world sign, near, facing away             -> yes, the walk-around hole
//
// facingDot is dot(sign outward normal, direction from sign to player);
// positive means the sign faces the player.
bool hudLabelWanted(float range, bool hasWorldSign, float facingDot);

// The base id of a building, with any sandbox instance suffix removed:
// "bakery#4" -> "bakery". Sign lookups must go through this, the same way
// RaylibRenderer.cpp:347 already does for "fountain#4".
std::string signBaseId(const std::string& id);

}  // namespace llm_npc
