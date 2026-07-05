#include "FaceTexture.hpp"

namespace llm_npc {
namespace FaceTexture {

namespace {

// The legacy face lived in the NPC's local frame: x in [-0.16, 0.16],
// y around [1.55, 1.85]. Map that window onto the 128px canvas so every
// feature keeps its proportions (screen y grows downward).
constexpr int kSize = 128;
constexpr float kScale = 400.f;  // pixels per world unit
constexpr float kTop = 1.85f;    // world y that maps to canvas y = 0

float px(float worldX) { return kSize / 2.f + worldX * kScale; }
float py(float worldY) { return (kTop - worldY) * kScale; }

// Axis-aligned feature box given the legacy center + half extents.
void box(float cx, float cy, float halfW, float halfH, Color color) {
    DrawRectangleRec(Rectangle{px(cx) - halfW * kScale, py(cy) - halfH * kScale,
                               halfW * 2.f * kScale, halfH * 2.f * kScale},
                     color);
}

// Rotated feature box (brows). `worldDeg` is the legacy rotation about +Z
// (counter-clockwise, +X end lifts); canvas y points down and raylib rotates
// clockwise-positive, so the canvas angle is the negation.
void rotBox(float cx, float cy, float halfW, float halfH, float worldDeg, Color color) {
    DrawRectanglePro(Rectangle{px(cx), py(cy), halfW * 2.f * kScale, halfH * 2.f * kScale},
                     Vector2{halfW * kScale, halfH * kScale}, -worldDeg, color);
}

}  // namespace

Texture2D bake(NpcFace face) {
    const Color dark{45, 38, 38, 255};
    const Color lip{150, 75, 70, 255};
    const Color blush{235, 140, 150, 255};
    const Color bubble{250, 242, 228, 235};

    RenderTexture2D target = LoadRenderTexture(kSize, kSize);
    BeginTextureMode(target);
    ClearBackground(BLANK);
    DrawCircle(kSize / 2, kSize / 2, kSize / 2.f - 2.f, bubble);
    DrawCircleLines(kSize / 2, kSize / 2, kSize / 2.f - 2.f, Color{120, 110, 100, 200});

    // Eyes: taller when surprised (legacy half-heights preserved).
    const float eyeHalfY = face == NpcFace::Surprised ? 0.040f : 0.022f;
    box(-0.07f, 1.71f, 0.024f, eyeHalfY, dark);
    box(0.07f, 1.71f, 0.024f, eyeHalfY, dark);

    // Brows: +tilt lowers the inner (nose-side) ends via the mirrored
    // per-side rotation — angry slants inward-down, sad inward-up.
    float browTilt = 0.f;
    if (face == NpcFace::Angry) browTilt = 18.f;
    if (face == NpcFace::Sad) browTilt = -14.f;
    const float browY = face == NpcFace::Surprised ? 1.80f : 1.77f;
    for (int side = -1; side <= 1; side += 2) {
        rotBox(0.07f * static_cast<float>(side), browY, 0.038f, 0.008f,
               browTilt * static_cast<float>(side), dark);
    }

    // Mouth per mood (legacy shapes).
    switch (face) {
        case NpcFace::Surprised:
            box(0.f, 1.615f, 0.022f, 0.030f, dark);  // small open "o"
            break;
        case NpcFace::Happy:
            box(0.f, 1.615f, 0.045f, 0.008f, lip);
            box(-0.048f, 1.628f, 0.010f, 0.010f, lip);  // upturned corners
            box(0.048f, 1.628f, 0.010f, 0.010f, lip);
            break;
        case NpcFace::Sad:
        case NpcFace::Angry:
            box(0.f, 1.612f, 0.040f, 0.008f, lip);
            box(-0.044f, 1.600f, 0.010f, 0.010f, lip);  // downturned corners
            box(0.044f, 1.600f, 0.010f, 0.010f, lip);
            break;
        case NpcFace::Embarrassed:
            box(0.f, 1.612f, 0.022f, 0.008f, lip);      // tight little mouth
            box(-0.115f, 1.66f, 0.030f, 0.016f, blush);  // cheek blush
            box(0.115f, 1.66f, 0.030f, 0.016f, blush);
            break;
        case NpcFace::Neutral:
            box(0.f, 1.612f, 0.038f, 0.008f, lip);
            break;
    }
    EndTextureMode();

    // Keep the pixels, drop the render target: copy through an Image so the
    // returned texture is independent of the RenderTexture's framebuffer.
    // (Render textures are stored y-flipped; LoadImageFromTexture preserves
    // that, so flip once here.)
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Texture2D out = LoadTextureFromImage(image);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return out;
}

}  // namespace FaceTexture
}  // namespace llm_npc
