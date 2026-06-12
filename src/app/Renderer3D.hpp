#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <vector>

#include "City.hpp"
#include "Math.hpp"

namespace llm_npc {

// First-person camera state: eye position (feet; eye height is added by the
// renderer) plus look angles in degrees.
struct CameraPose {
    Vec3 position{};
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

// Everything the renderer needs to draw one NPC: where they stand, which way
// they face, and a palette slot that picks their clothing colors.
struct NpcVisual {
    Vec3 position{};
    float facingDeg = 0.f;
    int palette = 0;
};

// Legacy-GL (2.1) renderer for the city and its inhabitants. Draws with
// immediate mode and procedural textures so the game needs no asset files.
// Lives in src/app because it requires SFML + OpenGL; never unit-tested in
// the headless container.
class Renderer3D {
   public:
    // Eye height above the ground the camera position stands on, in world
    // units (~ meters).
    static constexpr float kEyeHeight = 1.7f;
    // Vertical field of view used for both rendering and worldToScreen.
    static constexpr float kFovDeg = 70.f;
    // Near/far clip planes.
    static constexpr float kNearPlane = 0.1f;
    static constexpr float kFarPlane = 500.f;

    // One-time GL setup: depth test, lighting, blending, and the procedural
    // texture set. Call once after the window's GL context is active.
    void init();

    // Starts a frame: sets the viewport, clears to sky, and loads the
    // projection and camera matrices. Remembers the camera so worldToScreen
    // can project points for this frame's overlay pass.
    void beginFrame(const sf::RenderWindow& window, const CameraPose& camera);

    // Draws the ground, streets, plaza, park, and every building of the city.
    void drawCity(const City& city);

    // Draws one NPC as a simple capsule-ish figure facing `facingDeg`.
    void drawNpc(const NpcVisual& npc);

    // Projects a world point through the camera set by beginFrame. Returns
    // true and fills `out` with pixel coordinates when the point is in front
    // of the camera; false when it is behind (the caller should skip drawing).
    bool worldToScreen(const Vec3& world, const sf::RenderWindow& window, sf::Vector2f& out) const;

   private:
    // Procedural textures, generated once in init().
    GLuint texAsphalt_ = 0;
    GLuint texPavement_ = 0;
    GLuint texGrass_ = 0;
    GLuint texBrick_ = 0;
    GLuint texGlass_ = 0;
    GLuint texStripe_ = 0;
    GLuint texCloth_ = 0;

    // Camera basis captured by beginFrame for worldToScreen.
    Vec3 eye_{};
    Vec3 right_{};
    Vec3 up_{};
    Vec3 forward_{};
    float aspect_ = 1.f;

    // Draws a flat, textured rectangle on the ground at height y.
    void drawGroundPatch(float minX, float minZ, float maxX, float maxZ, float y,
                         GLuint texture, const sf::Color& tint, float tile) const;

    // Draws one building box, choosing texture and tint from facadeKind.
    void drawBuilding(const Building& building) const;
};

}  // namespace llm_npc
