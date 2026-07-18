#pragma once

// NOT YET IN THE BUILD: enable the raylib block in CMakeLists.txt (see the
// "raylib migration" comments there) before adding this to the app target.
// Plan: .claude/plans/raylib-visual-overhaul.md, steps 1-3.
#include "raylib.h"

#include <string>

#include "Assets.hpp"
#include "City.hpp"
#include "Math.hpp"

namespace llm_npc {

// First-person camera pose — same fields as the legacy Renderer3D's
// CameraPose so main.cpp's movement/mouse-look code carries over unchanged.
struct CameraPose {
    Vec3 position{};
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

// The renderer-facing mood face, mirroring NpcMood one-to-one (kept separate
// so core never includes rendering headers — same split Renderer3D used).
enum class NpcFace { Neutral, Happy, Angry, Sad, Embarrassed, Surprised };

// Everything needed to draw one character this frame (NPC or remote player).
struct CharacterVisual {
    Vec3 position{};
    float facingDeg = 0.f;
    int variantSeed = 0;      // stable id → model/tint pick (NPC index, 1000+playerId)
    bool walking = false;     // switches idle/walk animation clips
    float gesturePhase = 0.f; // seconds into wave/raise-hand; 0 = none
    NpcFace face = NpcFace::Neutral;
};

// raylib replacement for the legacy GL Renderer3D: draws the asset-pack city,
// animated glTF characters, sky/fog, and provides world→screen projection for
// nameplates. Owns no game state — main.cpp passes visuals per frame.
class RaylibRenderer {
   public:
    // `assets` outlives the renderer (loaded once at startup by main).
    explicit RaylibRenderer(Assets& assets);

    // Begins the 3D pass: BeginMode3D with a Camera3D derived from `pose`
    // (position + yaw/pitch → target vector), sky clear color, fog shader.
    // TODO(implement): step 3 of the plan.
    void beginFrame(const CameraPose& pose);

    // Draws ground, roads, and one asset-pack model per City building
    // (facadeKind/spotId → model via Assets), plus street props.
    // TODO(implement): step 3.
    void drawCity(const City& city);

    // Draws one animated character: model by variantSeed, idle/walk clip by
    // `walking`, gesture clip while gesturePhase > 0, face texture by `face`.
    // TODO(implement): steps 4-5.
    void drawCharacter(const CharacterVisual& visual);

    // Ends the 3D pass (EndMode3D). 2D overlay (DialogUI/Menu/nameplates)
    // draws after this, before EndDrawing.
    // TODO(implement): step 3.
    void endFrame();

    // Projects a world point for nameplates; false when behind the camera.
    // raylib's GetWorldToScreen does the math — this wraps the behind-camera
    // check the legacy worldToScreen had.
    // TODO(implement): step 3.
    bool worldToScreen(const Vec3& world, Vector2& out) const;

   private:
    Assets& assets_;
    Camera3D camera_{};  // rebuilt each beginFrame; kept for worldToScreen

    // TODO(implement): fog/tint shader handle, animation clocks per character
    // variant (advance in drawCharacter using GetFrameTime()).
};

}  // namespace llm_npc
