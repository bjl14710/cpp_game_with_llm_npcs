#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <cstdint>
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

// A transient arm gesture the renderer can pose an NPC into. Kept separate
// from the core NpcAction enum so the renderer stays self-contained; the main
// loop maps one to the other.
enum class NpcPose { None, RaiseHand, Wave };

// The facial expression to draw, mirroring the core NpcMood the same way
// NpcPose mirrors actions.
enum class NpcFace { Neutral, Happy, Angry, Sad, Embarrassed, Surprised };

// A single NPC's procedurally-varied appearance: clothing/skin/hair colors, a
// hair-style id, and overall height/build scales. Built deterministically from
// a per-NPC seed (see appearanceFromSeed) so each citizen looks distinct but
// stable across frames.
struct NpcAppearance {
    sf::Color skin{235, 200, 175};
    sf::Color hair{60, 45, 35};
    sf::Color shirt{120, 120, 130};
    sf::Color pants{60, 60, 70};
    sf::Color shoes{40, 35, 32};
    int hairStyle = 0;        // 0 short cap, 1 voluminous, 2 buzz
    float heightScale = 1.f;  // whole-body vertical scale (~0.92..1.08)
    float buildScale = 1.f;   // whole-body girth scale (~0.92..1.08)
};

// Everything the renderer needs to draw one NPC: where they stand, which way
// they face, their appearance, the current arm gesture (with elapsed seconds
// so a wave can animate), the facial expression matching their mood, and a
// locomotion phase + moving flag that drive the walk cycle.
struct NpcVisual {
    Vec3 position{};
    float facingDeg = 0.f;
    NpcAppearance appearance{};
    NpcPose pose = NpcPose::None;
    float gesturePhase = 0.f;
    NpcFace face = NpcFace::Neutral;
    float locomotionPhase = 0.f;  // accumulated stride phase (radians)
    bool moving = false;          // true while the NPC is walking
};

// Derives a stable, varied appearance from a seed. Police get a recognizable
// blue uniform regardless of seed; everyone else gets seeded colors and build.
NpcAppearance appearanceFromSeed(std::uint32_t seed, bool police);

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
    GLuint texWood_ = 0;    // interior floors, counters
    GLuint texLeaf_ = 0;    // tree foliage
    GLuint texShelf_ = 0;   // bookshelves / store shelving
    GLuint texWindow_ = 0;  // windowed facade for tall filler buildings

    // Camera basis captured by beginFrame for worldToScreen.
    Vec3 eye_{};
    Vec3 right_{};
    Vec3 up_{};
    Vec3 forward_{};
    float aspect_ = 1.f;

    // Draws a flat, textured rectangle on the ground at height y.
    void drawGroundPatch(float minX, float minZ, float maxX, float maxZ, float y,
                         GLuint texture, const sf::Color& tint, float tile) const;

    // Draws one building: a solid box, or (when enterable) a hollow shell with
    // walls, doorway, roof, interior floor, and shop fixtures.
    void drawBuilding(const Building& building) const;

    // Draws an enterable shop: wall segments with the doorway gap, a lintel
    // over the door, a roof, an interior floor, a sign band, and a swinging
    // door panel whose angle follows the camera's distance to the doorway.
    void drawHollowBuilding(const Building& building) const;

    // Draws the back-wall fixtures themed to a shop's facadeKind (bookshelves,
    // store shelving, counters, wall guitars, ...).
    void drawShopFixtures(const Building& building) const;

    // Draws steel bars across an AABB span (the holding-cell front).
    void drawCellBars(const Building& building) const;

    // Draws an axis-aligned box from the footprint `box`, spanning y0..y1.
    void drawBox(const AABB& box, float y0, float y1, GLuint texture,
                 const sf::Color& tint, float tile) const;

    // Street dressing drawn across the city: lamp posts and trees.
    void drawStreetProps() const;

    // Paints the background gradient sky (an ortho full-screen quad) before the
    // 3D pass; the horizon color matches the fog so distance dissolves to haze.
    void drawSky() const;

    // Draws one jointed NPC arm (upper arm, forearm, hand) in the figure's
    // local frame, pivoting at the shoulder. `pitchXDeg` rotates the whole arm
    // about local +X (the walk swing, or ~195 to raise it overhead); `rollZDeg`
    // adds a side-to-side rotation about local +Z for waving. `segments` sets
    // the cylinder tessellation for level of detail.
    void drawArm(float xOffset, const sf::Color& sleeve, const sf::Color& skin,
                 float pitchXDeg, float rollZDeg, int segments) const;

    // Draws one jointed NPC leg (thigh, shin, shoe) pivoting at the hip.
    // `phase` is the stride phase in radians and `walking` toggles the swing +
    // knee bend; the left/right legs pass phases a half-cycle apart.
    void drawLeg(float xOffset, float phase, bool walking,
                 const NpcAppearance& appearance, int segments) const;

    // Draws the hair cap on the crown, shaped by appearance.hairStyle and set
    // back so it never covers the face.
    void drawHair(const NpcAppearance& appearance, int slices, int stacks) const;

    // Draws eyes, brows, and mouth on the front of the head in the figure's
    // local frame, shaped by the expression.
    void drawFace(NpcFace face) const;
};

}  // namespace llm_npc
