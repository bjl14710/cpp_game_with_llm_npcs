#pragma once

#include <string>

#include "SignLayout.hpp"
#include "raylib.h"

namespace llm_npc {

// Bakes a storefront sign into textures at load time, the same way
// FaceTexture::bake() bakes the procedural mood faces -- render once into an
// image, upload, keep the Texture2D, never draw text in the 3D pass.
//
// WHY BAKE rather than draw world-space text: raylib's built-in font is
// ASCII 32-126 and computes glyph advance by integer division, so sizes
// 14-18 are pixel-identical (src/core/AsciiText.hpp documents the whole
// mechanism, and it is why "Marge's" renders as "Marge?s" if the name is not
// folded first). Blown up onto a 6.4-unit board it reads as a debug overlay.
// Baking through a real font atlas gives the design's slab letterforms and
// costs nothing per frame.
//
// DEPENDS ON #236 (font atlas). Until it lands there is no TTF to bake from
// and bake() falls back to the built-in font: blocky, still legible, never a
// blank board.
namespace SignTexture {

// Two textures per sign, and the split is deliberate.
//
// The board dims with the world -- it is painted wood and it goes dark at
// night like every other surface, so the renderer draws it inside the
// frame's fog shader with no special case. The letters must NOT dim, so they
// are a separate transparent layer drawn kSignProud + a hair further out,
// OUTSIDE the fog shader, tinted toward the night colour as lightLevel
// falls. Emote billboards already break out of the frame shader exactly like
// this (RaylibRenderer.cpp:1095) -- this is that pattern, not a new one, and
// it means the fog shader keeps the single uniform set it has today.
struct Baked {
    Texture2D board{};    // hardwood, grain, border
    Texture2D letters{};  // wordmark + emblem on transparent
};

// Board texture pixel size. 4.57:1 matches kBoardAspect, so the wordmark
// keeps its proportions whatever width the board scales to.
constexpr int kBoardTexWidth = 1024;
constexpr int kBoardTexHeight = 224;

// Bakes one sign. `name` is Building::name and MUST be folded through
// toRenderableAscii() before it reaches the font. `emblem` is the SignSpec
// tile id ("loaves", "cup", "hammer", "books", "shield", "hotdog"); empty
// bakes the wordmark alone.
//
// The caller owns both textures and must pass the result to unload().
Baked bake(const std::string& name, const std::string& emblem);

// Releases both textures. Safe on a default-constructed Baked.
void unload(Baked& baked);

}  // namespace SignTexture

}  // namespace llm_npc
