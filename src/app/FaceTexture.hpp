#pragma once

#include "raylib.h"

namespace llm_npc {

// The renderer-facing mood face, mirroring NpcMood one-to-one (kept separate
// so core never includes rendering headers — same split the legacy renderer
// used).
enum class NpcFace { Neutral, Happy, Angry, Sad, Embarrassed, Surprised };

// Bakes the game's signature procedural faces (brows/eyes/mouth per mood —
// previously drawn in GL immediate mode by Renderer3D::drawFace) into
// textures shown as emote billboards above characters' heads. The KayKit
// models share one atlas material, so painting the head per mood isn't
// practical; a floating emote reads better at street distance anyway.
namespace FaceTexture {

// Renders one mood's face into a 128x128 texture the caller owns
// (UnloadTexture). Features sit on a pale round bubble; geometry and the
// brow-tilt direction are ported 1:1 from the legacy renderer (angry brows
// slant inward-down, sad inward-up — regression fixed in commit 6298566).
Texture2D bake(NpcFace face);

// Stylized face set (issue #140) for the quaternius mesh family: one flat
// TRANSPARENT decal per (eye style, mouth style, mood), drawn by the
// renderer as a head-oriented quad at the face sockets — NOT a camera
// billboard, and no bubble: the mesh head is the face's background. The
// style axes mirror the family's creator picks (eyes_q_* / mouth_q_* rows);
// the mood axis replaces the floating emote for this family. Angry/sad
// brow tilts keep the legacy directions.
constexpr int kEyeStyleCount = 8;    // dot wide round happy sleepy star wink glasses
constexpr int kMouthStyleCount = 4;  // smile line open o
Texture2D bakeStylized(int eyeStyle, int mouthStyle, NpcFace mood);

}  // namespace FaceTexture

}  // namespace llm_npc
