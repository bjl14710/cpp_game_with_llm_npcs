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

Texture2D bakeStylized(int eyeStyle, int mouthStyle, NpcFace mood) {
    const Color dark{45, 38, 38, 255};
    const Color lip{120, 58, 54, 255};
    const Color blushTone{235, 140, 150, 220};

    // Canvas anatomy (pixels): eye line high, mouth low, so the quad the
    // renderer stretches over the face sockets lines features up with the
    // mesh head's own eye/mouth heights.
    constexpr float cx = 64.f;
    constexpr float eyeY = 46.f;
    constexpr float eyeDx = 27.f;
    constexpr float mouthY = 96.f;

    RenderTexture2D target = LoadRenderTexture(kSize, kSize);
    BeginTextureMode(target);
    ClearBackground(BLANK);  // decal: the head mesh is the background

    // Surprise widens every eye style; sleepy lids narrow it.
    const float eyeScale = mood == NpcFace::Surprised ? 1.3f : 1.f;

    for (int side = -1; side <= 1; side += 2) {
        const float ex = cx + eyeDx * static_cast<float>(side);
        const bool winkClosed = eyeStyle == 6 && side > 0 &&
                                mood != NpcFace::Surprised;
        if (winkClosed) {
            // The wink's closed lid: a cheerful horizontal dash.
            DrawRectangleRec(Rectangle{ex - 11.f, eyeY - 3.f, 22.f, 6.f}, dark);
        } else if (eyeStyle == 1) {  // wide: big solid rounds
            DrawCircleV(Vector2{ex, eyeY}, 11.f * eyeScale, dark);
        } else if (eyeStyle == 2) {  // round: ring + pupil
            DrawRing(Vector2{ex, eyeY}, 7.f * eyeScale, 10.f * eyeScale, 0.f,
                     360.f, 24, dark);
            DrawCircleV(Vector2{ex, eyeY}, 3.5f, dark);
        } else if (eyeStyle == 3) {  // happy: upward arcs (^ ^)
            DrawRing(Vector2{ex, eyeY + 3.f}, 7.f, 11.f, 180.f, 360.f, 16, dark);
        } else if (eyeStyle == 4) {  // sleepy: heavy lid over a half pupil
            DrawCircleSector(Vector2{ex, eyeY}, 8.f, 0.f, 180.f, 16, dark);
            DrawRectangleRec(Rectangle{ex - 10.f, eyeY - 6.f, 20.f, 6.f}, dark);
        } else if (eyeStyle == 5) {  // star: a four-point sparkle
            DrawRectanglePro(Rectangle{ex, eyeY, 22.f * eyeScale, 6.f},
                             Vector2{11.f * eyeScale, 3.f}, 45.f, dark);
            DrawRectanglePro(Rectangle{ex, eyeY, 22.f * eyeScale, 6.f},
                             Vector2{11.f * eyeScale, 3.f}, -45.f, dark);
        } else if (eyeStyle == 7) {  // glasses: framed lenses + pupils
            DrawRing(Vector2{ex, eyeY}, 10.f, 13.f, 0.f, 360.f, 24, dark);
            DrawCircleV(Vector2{ex, eyeY}, 4.f, dark);
        } else {  // 0 dot (and the wink's open eye)
            DrawCircleV(Vector2{ex, eyeY}, 7.f * eyeScale, dark);
        }
    }
    if (eyeStyle == 7) {  // glasses bridge
        DrawRectangleRec(Rectangle{cx - 15.f, eyeY - 2.f, 30.f, 4.f}, dark);
    }

    // Brows: same mood language as the emote set (angry inward-down, sad
    // inward-up), raised for surprise. Screen y grows downward and raylib
    // rotation is clockwise-positive, hence the sign flip per side.
    float browTilt = 0.f;
    if (mood == NpcFace::Angry) browTilt = 18.f;
    if (mood == NpcFace::Sad) browTilt = -14.f;
    const float browY = mood == NpcFace::Surprised ? eyeY - 24.f : eyeY - 18.f;
    if (mood != NpcFace::Neutral && mood != NpcFace::Happy) {
        for (int side = -1; side <= 1; side += 2) {
            const float ex = cx + eyeDx * static_cast<float>(side);
            DrawRectanglePro(Rectangle{ex, browY, 26.f, 5.f}, Vector2{13.f, 2.5f},
                             -browTilt * static_cast<float>(side), dark);
        }
    }

    if (mood == NpcFace::Embarrassed) {
        DrawEllipse(static_cast<int>(cx - 44.f), static_cast<int>(eyeY + 18.f),
                    10.f, 6.f, blushTone);
        DrawEllipse(static_cast<int>(cx + 44.f), static_cast<int>(eyeY + 18.f),
                    10.f, 6.f, blushTone);
    }

    // Mouth: the mood overrides the picked resting style — that is the
    // whole point of retiring the emote billboard for this family.
    int shape = mouthStyle;                      // 0 smile, 1 line, 2 open, 3 o
    float cornerLift = shape == 0 ? 6.f : 0.f;   // smile corners up
    if (mood == NpcFace::Happy) {
        shape = 0;
        cornerLift = 7.f;
    } else if (mood == NpcFace::Angry || mood == NpcFace::Sad) {
        shape = 0;
        cornerLift = -6.f;  // downturned
    } else if (mood == NpcFace::Surprised) {
        shape = 2;
    } else if (mood == NpcFace::Embarrassed) {
        shape = 1;
    }
    if (shape == 0) {
        DrawRectangleRec(Rectangle{cx - 15.f, mouthY - 3.f, 30.f, 6.f}, lip);
        DrawCircleV(Vector2{cx - 16.f, mouthY - cornerLift * 0.7f}, 4.f, lip);
        DrawCircleV(Vector2{cx + 16.f, mouthY - cornerLift * 0.7f}, 4.f, lip);
    } else if (shape == 1) {
        const float w = mood == NpcFace::Embarrassed ? 16.f : 26.f;
        DrawRectangleRec(Rectangle{cx - w * 0.5f, mouthY - 2.5f, w, 5.f}, lip);
    } else if (shape == 2) {
        DrawEllipse(static_cast<int>(cx), static_cast<int>(mouthY), 9.f, 12.f, lip);
    } else {
        DrawRing(Vector2{cx, mouthY}, 5.f, 9.f, 0.f, 360.f, 24, lip);
    }
    EndTextureMode();

    // Same framebuffer-independence copy as bake().
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    Texture2D out = LoadTextureFromImage(image);
    UnloadImage(image);
    UnloadRenderTexture(target);
    return out;
}

}  // namespace FaceTexture
}  // namespace llm_npc
