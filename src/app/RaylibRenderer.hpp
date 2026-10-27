#pragma once

// NOT YET IN THE BUILD: enable the raylib block in CMakeLists.txt (see the
// "raylib migration" comments there) before adding this to the app target.
// Plan: .claude/plans/raylib-visual-overhaul.md, steps 1-3.
#include "raylib.h"

#include <array>
#include <random>
#include <unordered_map>

#include "Assets.hpp"
#include "CharacterParts.hpp"
#include "City.hpp"
#include "FaceTexture.hpp"
#include "Math.hpp"
#include "NoirLook.hpp"
#include "WalkCycle.hpp"

namespace llm_npc {

// CameraPose moved to core/Math.hpp so cutscene playback can drive the camera
// without a core header reaching into the app layer. Math.hpp is included
// above; nothing else about the renderer's interface changed.

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
    ~RaylibRenderer();

    RaylibRenderer(const RaylibRenderer&) = delete;
    RaylibRenderer& operator=(const RaylibRenderer&) = delete;

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

    // Noir night switches (config noir_night / rain). Off restores today's
    // night exactly; takes effect at the next setTimeOfDay.
    void setNoirOptions(bool noirNight, bool rain);

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
    // `pose` is one sample of the procedural walk (WalkCycle): two-bone
    // legs, bob, lean, sway, head nod and hair follow-through; the default
    // stands still. Non-neutral `face` billboards the same emote pack
    // characters use; `dead` lays the figure flat instead (the dead don't
    // emote). `highlighted` marks the NPC the talk prompt targets: at night
    // its outline turns into a thin unlit red rim (one of the few colors the
    // noir grade lets through); by day it changes nothing.
    void drawCompositeCharacter(const CharacterLook& look, const Vec3& position,
                                float facingDeg, const WalkPose& pose = standingPose(),
                                NpcFace face = NpcFace::Neutral, bool dead = false,
                                bool highlighted = false);

    // The player avatar's palette (issue #106): the punch arm/fist and the
    // pistol hand tint from it, so the avatar choice is visible in first
    // person. Unknown/empty ids keep the previous colors.
    void setAvatarPalette(const std::string& paletteId);

    // First-person weapon prop anchored lower-right of the camera, driven
    // solely by the authoritative equipped state: the WeaponKind index and
    // the core's attackAnimFraction (1 at swing start, decaying to 0).
    // Call inside the 3D pass, after the world is drawn.
    void drawViewmodel(int weaponKind, float attackFraction);

    // Ends the 3D pass (EndMode3D). At night the pass was rendered into an
    // offscreen target; this draws it to the screen through the noir grade.
    // The 2D overlay (DialogUI/Menu/nameplates) draws after this, before
    // EndDrawing, so it is never graded.
    void endFrame();

    // Editor support (issue #112): unprojects a screen point onto the
    // ground plane (y = 0) with the CURRENT camera; false when the ray
    // points at the sky. Call between beginFrame and endFrame.
    bool screenToGround(Vector2 screen, Vec3& out) const;

    // Editor ghost: the piece's would-be Building drawn translucent —
    // green when the placement is valid, red when refused. Reuses the
    // normal building draw paths so the preview IS the eventual look.
    void drawPlacementGhost(const Building& building, bool valid);

    // Projects a world point for nameplates; false when behind the camera.
    // raylib's GetWorldToScreen does the math — this wraps the behind-camera
    // check the legacy worldToScreen had.
    // TODO(implement): step 3.
    bool worldToScreen(const Vec3& world, Vector2& out) const;

   private:
    // Routes the primitive batch through the character cel shader for the
    // duration of a character draw (composite parts, viewmodel), then back
    // to the frame's fog shader — BeginShaderMode is not a stack, so the
    // restore is explicit (issue #138 routing rule: no character surface
    // on the default lit material).
    void beginCharacterShader();
    void endCharacterShader();

    Assets& assets_;
    // Avatar-driven viewmodel colors (defaults match the pre-avatar look).
    Color avatarSkin_{224, 172, 138, 255};
    Color avatarOutfit_{60, 64, 74, 255};
    Camera3D camera_{};  // rebuilt each beginFrame; kept for worldToScreen
    Color skyColor_{135, 190, 235, 255};  // daylight until the clock speaks
    float lightLevel_ = 1.f;              // scene brightness multiplier
    Texture2D glowTexture_{};             // radial falloff for lamp halos

    // Noir post-process: the 3D pass renders into sceneTarget_ whenever the
    // night factor is above 0, then endFrame draws it through gradeShader_.
    RenderTexture2D sceneTarget_{};
    Shader gradeShader_{};
    bool gradeLoaded_ = false;
    bool grading_ = false;  // this frame's 3D pass targets sceneTarget_
    struct GradeLocs {
        int desaturation = -1, contrast = -1, vignette = -1, grain = -1;
        int time = -1, resolution = -1;
    };
    GradeLocs gradeLocs_;

    // (Re)creates sceneTarget_ at the current framebuffer size; false when
    // it can't be created (the frame then draws ungraded).
    bool ensureSceneTarget();

    // Draws sceneTarget_ fullscreen through the grade shader.
    void drawGradedScene();

    // Rain: a fixed pool of drops in a camera-relative box, advanced and
    // recycled in place each frame (never reallocated). The first
    // rain-intensity share of the pool is drawn as short slanted streaks
    // through the fog shader, so drops fade into the haze with distance and
    // catch the lamp pools; drawn inside the 3D pass, before the grade.
    static constexpr std::size_t kRainDrops = 900;
    std::array<Vector3, kRainDrops> rainDrops_{};
    bool rainSeeded_ = false;
    std::minstd_rand rainRng_{1234567u};
    void drawRain();
    NoirLook noir_{};                     // night look for the current hour
    bool noirEnabled_ = true;             // config: noir_night
    bool rainEnabled_ = true;             // config: rain

    // Feeds the nearest lamp heads (cap noir::kMaxLamps) to the fog shader,
    // or zero lamps in daylight so the shader takes its day path.
    void uploadLamps(const City& city);

    // Night-only red accents that survive the grade: a flashing light bar
    // on the police car and a neon strip over two storefronts (bakery,
    // coffee), placed from City's building boxes and drawn unlit.
    void drawRedAccents(const City& city);

    // Night-only glowing bulbs at every lamp head, drawn unlit (outside the
    // fog shader) so they read as light sources rather than lit surfaces.
    void drawLampGlows(const City& city);

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
