#include "SignTexture.hpp"

#include "AsciiText.hpp"

namespace llm_npc {
namespace SignTexture {

// SCAFFOLD -- stub bodies only. bake() returns empty textures so the app
// links and runs unchanged; the renderer pass that consumes them is not
// wired up yet either.
//
// Palette and layout: .claude/plans/store-signs.md, design artboard 1a.
//   board   #7a4a2c    edge/shadow #5d3720
//   letters #f4e8d0    accent      #e0b884
//   night letters      #ffe9b8
//   emblem tile 0.9u square, left of the wordmark

Baked bake(const std::string& name, const std::string& emblem) {
    // TODO(#286-3): board pass --
    //   GenImageColor(kBoardTexWidth, kBoardTexHeight, board colour), a
    //   #5d3720 inner border, then upload with LoadTextureFromImage.
    //
    // TODO(#286-3): letter pass --
    //   GenImageColor(..., BLANK); reserve a square emblem cell on the left
    //   at the image height, draw the emblem into it with ImageDrawRectangle
    //   / ImageDrawCircle; then ImageDrawTextEx the folded name into the
    //   remaining width with the #236 atlas at ~96px, scaling the size down
    //   to fit with a floor of 48px and truncating on a word boundary below
    //   that. Never overflow the board.
    //
    // TODO(#286-3): fall back to GetFontDefault() when the #236 atlas is
    // absent, and log that once -- not once per sign.
    (void)toRenderableAscii(name);
    (void)emblem;
    return Baked{};
}

void unload(Baked& baked) {
    // TODO(#286-3): UnloadTexture both, guarding on id != 0 so a
    // default-constructed Baked is safe, then reset the struct.
    (void)baked;
}

}  // namespace SignTexture
}  // namespace llm_npc
