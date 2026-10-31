#include "SignLayout.hpp"

namespace llm_npc {

// SCAFFOLD — stub bodies only. Every function below compiles and links so
// both builds stay green, and every one of them returns a wrong answer on
// purpose. tests/test_sign_layout.cpp holds the real contract, skipped until
// these are filled in.
//
// Implementation order and the exact rules: .claude/plans/store-signs.md,
// sections "Sign geometry - 1a" and "Range split".

Vec3 signFaceNormal(SignFace face) {
    // TODO(#286-2): PlusZ -> {0,0,1}, MinusZ -> {0,0,-1},
    // PlusX -> {1,0,0}, MinusX -> {-1,0,0}.
    (void)face;
    return Vec3{0.f, 0.f, 1.f};
}

SignPlacement signPlacementFor(const Building& building, const SignSpec& spec) {
    // TODO(#286-2): the scaling rule from the plan, verbatim --
    //
    //   frontWidth = (face is PlusZ/MinusZ) ? (maxX - minX) : (maxZ - minZ)
    //   boardW     = clampf(frontWidth * kBoardFrontFraction,
    //                       kBoardMinWidth, kBoardMaxWidth)
    //   boardH     = boardW * kBoardAspect
    //   boardTop   = spec.boardTopOverride > 0.f
    //                  ? spec.boardTopOverride
    //                  : std::fmin(kBoardDefaultTop,
    //                              building.height - kBoardTopClearance)
    //
    // Then center the board on the chosen face, push it kSignProud out along
    // the outward normal, drop the fascia band to kFasciaTop, and hang the
    // blade kBladeOut along the normal at board height.
    //
    // Invalid when: building.name is empty, the footprint has zero extent on
    // either axis, or boardTop - boardH lands below ground.
    (void)building;
    (void)spec;
    return SignPlacement{};
}

bool hudLabelWanted(float range, bool hasWorldSign, float facingDot) {
    // TODO(#286-2): the four cases documented in the header. Returning true
    // unconditionally is today's #166 behaviour, so the scaffold is a no-op
    // on the HUD rather than a regression.
    (void)range;
    (void)hasWorldSign;
    (void)facingDot;
    return true;
}

std::string signBaseId(const std::string& id) {
    // TODO(#286-2): truncate at the first '#'. Must be total -- an id with no
    // '#' comes back unchanged, and an id that starts with '#' yields "".
    return id;
}

}  // namespace llm_npc
