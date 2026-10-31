#pragma once

// NOT YET IN THE BUILD: enable the raylib block in CMakeLists.txt (see the
// "raylib migration" comments there) before adding this to the app target.
// Plan: .claude/plans/raylib-visual-overhaul.md, steps 1-3.
#include "raylib.h"

#include <unordered_map>

#include "Assets.hpp"
#include "CharacterParts.hpp"
#include "City.hpp"
#include "FaceTexture.hpp"
#include "Math.hpp"

namespace llm_npc {

// First-person camera pose — same fields as the legacy Renderer3D's
// CameraPose so main.cpp's movement/mouse-look code carries over unchanged.
struct CameraPose {
    Vec3 position{};
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

// Everything needed to draw one character this frame (NPC or remote player).
struct CharacterVisual {
    Vec3 position{};
    float facingDeg = 0.f;
    int variantSeed = 0;      // stable id → model/tint pick (NPC index, 1000+playerId)
    bool police = false;      // uniforms: police always get the Knight model
    bool walking = false;     // switches idle/walk animation clips
    bool dead = false;        // collapsed: death pose overrides all clips
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

    // Feeds the day/night pass from the SHARED world clock (call once per
    // frame, before beginFrame): sky/fog color and scene light level all
    // derive from this one hour value — no renderer-private timers.
    void setTimeOfDay(float hours);

    // The sky clear color for the current time of day (main passes it to
    // ClearBackground so sky and fog always agree).
    Color skyColor() const { return skyColor_; }

    // Draws ground, roads, and one asset-pack model per City building
    // (facadeKind/spotId → model via Assets), plus street props.
    // TODO(implement): step 3.
    void drawCity(const City& city);

    // Draws one animated character: model by variantSeed, idle/walk clip by
    // `walking`, gesture clip while gesturePhase > 0, face texture by `face`.
    // TODO(implement): steps 4-5.
    void drawCharacter(const CharacterVisual& visual);

    // Draws one composite character — the ONE shared parts library both the
    // creator and every roaming NPC render from (plan:
    // shared-character-library): parts snapped by the core's socket
    // assembly, scaled ONCE so the whole figure meets the same 1.8-unit
    // height contract as pack characters, colored by the look's palette.
    // `phase` drives the procedural walk bob (no skeleton in v1). Non-neutral
    // `face` billboards the same emote pack characters use; `dead` lays the
    // figure flat instead (the dead don't emote). Defaults keep the creator
    // preview call sites unchanged.
    void drawCompositeCharacter(const CharacterLook& look, const Vec3& position,
                                float facingDeg, bool walking, float phase,
                                NpcFace face = NpcFace::Neutral, bool dead = false);

    // The player avatar's palette (issue #106): the punch arm/fist and the
    // pistol hand tint from it, so the avatar choice is visible in first
    // person. Unknown/empty ids keep the previous colors.
    void setAvatarPalette(const std::string& paletteId);

    // First-person weapon prop anchored lower-right of the camera, driven
    // solely by the authoritative equipped state: the WeaponKind index and
    // the core's attackAnimFraction (1 at swing start, decaying to 0).
    // Call inside the 3D pass, after the world is drawn.
    void drawViewmodel(int weaponKind, float attackFraction);

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
    // Avatar-driven viewmodel colors (defaults match the pre-avatar look).
    Color avatarSkin_{224, 172, 138, 255};
    Color avatarOutfit_{60, 64, 74, 255};
    Camera3D camera_{};  // rebuilt each beginFrame; kept for worldToScreen
    Color skyColor_{135, 190, 235, 255};  // daylight until the clock speaks
    float lightLevel_ = 1.f;              // scene brightness multiplier

    // Per-entity animation playback (keyed by variantSeed, which is unique
    // per entity): which clip is playing and the running frame clock. The
    // clock resets when the clip changes so transitions start at frame 0.
    struct AnimState {
        int clip = -1;
        float time = 0.f;
    };
    std::unordered_map<int, AnimState> anim_;
};

}  // namespace llm_npc
