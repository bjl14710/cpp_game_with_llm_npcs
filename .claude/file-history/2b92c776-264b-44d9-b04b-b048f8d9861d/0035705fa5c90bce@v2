#pragma once

// NOT YET IN THE BUILD — see the raylib migration block in CMakeLists.txt.
// Plan: .claude/plans/raylib-visual-overhaul.md, step 5.
#include "raylib.h"

namespace llm_npc {

enum class NpcFace;  // RaylibRenderer.hpp

// Bakes the game's signature procedural faces (brows/eyes/mouth per mood —
// currently drawn in GL immediate mode inside Renderer3D::drawFace) into
// 128x128 textures that map onto the character models' head material.
// Bake once per mood at startup; swapping a head texture per frame is cheap.
namespace FaceTexture {

// Renders one mood's face into a texture the caller owns (UnloadTexture).
// TODO(implement): LoadRenderTexture(128,128); BeginTextureMode; port the
// brow-tilt/eye/mouth shapes from the legacy drawFace() — KEEP the brow tilt
// direction fixed by commit 6298566 (angry brows slant inward-down, sad
// inward-up); EndTextureMode. Skin tone stays in the model's palette; this
// texture carries features on a transparent background.
Texture2D bake(NpcFace face);

}  // namespace FaceTexture

}  // namespace llm_npc
