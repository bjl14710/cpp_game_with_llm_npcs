#include "RaylibRenderer.hpp"

#include <cmath>

#include "Math.hpp"

namespace llm_npc {

namespace {

// Street geometry from City::makeDowntown's documented layout: 3x3 blocks on
// a 64-unit pitch, 16-unit streets centered between block edges at ±32.
constexpr float kStreetCenter = 32.f;
constexpr float kStreetWidth = 16.f;

// Ground/asphalt/sidewalk palette — warm, low-poly-friendly flats.
constexpr Color kGrass{116, 153, 86, 255};
constexpr Color kAsphalt{68, 70, 78, 255};
constexpr Color kPlaza{188, 178, 158, 255};
constexpr Color kSidewalk{158, 152, 140, 255};

// Draws `model` scaled and translated so its bounding box exactly fills the
// axis-aligned footprint [minX,maxX]x[minZ,maxZ] with the given height,
// sitting on the ground plane. Rotation-free: KayKit city pieces face +Z at
// identity, which matches the street-facing fronts of the downtown layout.
void drawModelFittedToAABB(const Model& model, float minX, float minZ, float maxX,
                           float maxZ, float height, Color tint) {
    const BoundingBox bb = GetModelBoundingBox(model);
    const Vector3 size{bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z};
    if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f) return;
    const Vector3 scale{(maxX - minX) / size.x, height / size.y, (maxZ - minZ) / size.z};
    const Vector3 position{minX - bb.min.x * scale.x, -bb.min.y * scale.y,
                           minZ - bb.min.z * scale.z};
    DrawModelEx(model, position, Vector3{0.f, 1.f, 0.f}, 0.f, scale, tint);
}

}  // namespace

RaylibRenderer::RaylibRenderer(Assets& assets) : assets_(assets) {}

void RaylibRenderer::beginFrame(const CameraPose& pose) {
    camera_.position = {pose.position.x, pose.position.y + 1.7f, pose.position.z};
    const float pitchRad = degToRad(pose.pitchDeg);
    const float yawRad = degToRad(pose.yawDeg);
    const Vector3 dir{std::sin(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                      std::cos(yawRad) * std::cos(pitchRad)};
    camera_.target = {camera_.position.x + dir.x, camera_.position.y + dir.y,
                      camera_.position.z + dir.z};
    camera_.up = {0.f, 1.f, 0.f};
    camera_.fovy = 70.f;
    camera_.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(camera_);
}

void RaylibRenderer::drawCity(const City& city) {
    const float half = city.halfSize();

    // Ground: grass base with asphalt street strips and block slabs on top.
    DrawPlane({0.f, 0.f, 0.f}, {half * 2.f, half * 2.f}, kGrass);
    for (const float c : {-kStreetCenter, kStreetCenter}) {
        DrawCube({c, 0.02f, 0.f}, kStreetWidth, 0.04f, half * 2.f, kAsphalt);
        DrawCube({0.f, 0.02f, c}, half * 2.f, 0.04f, kStreetWidth, kAsphalt);
    }
    // Block slabs: plaza (center) is pale stone, park (north-east) stays
    // grass, every other block gets a sidewalk-toned slab.
    for (const float bx : {-64.f, 0.f, 64.f}) {
        for (const float bz : {-64.f, 0.f, 64.f}) {
            const bool plaza = bx == 0.f && bz == 0.f;
            const bool park = bx == 64.f && bz == 64.f;
            if (park) continue;
            DrawCube({bx, 0.04f, bz}, 48.f, 0.08f, 48.f, plaza ? kPlaza : kSidewalk);
        }
    }

    // Street dressing at the four crossings, when the pack is present.
    if (const Model* light = assets_.prop("trafficlight_A")) {
        for (const float x : {-kStreetCenter, kStreetCenter}) {
            for (const float z : {-kStreetCenter, kStreetCenter}) {
                drawModelFittedToAABB(*light, x + 8.5f, z + 8.5f, x + 9.5f, z + 9.5f,
                                      4.5f, WHITE);
            }
        }
    }
    // A little life in the park corner.
    if (const Model* bush = assets_.prop("bush")) {
        for (const Vec3 p : {Vec3{48.f, 0.f, 48.f}, Vec3{80.f, 0.f, 52.f},
                             Vec3{52.f, 0.f, 80.f}, Vec3{76.f, 0.f, 76.f}}) {
            drawModelFittedToAABB(*bush, p.x - 1.2f, p.z - 1.2f, p.x + 1.2f, p.z + 1.2f,
                                  1.4f, WHITE);
        }
    }

    // Buildings and street props from the collision AABBs — the models scale
    // to fit the authoritative footprints, never the other way around.
    for (const Building& b : city.buildings()) {
        if (const Model* model = assets_.modelForBuilding(b)) {
            drawModelFittedToAABB(*model, b.minX, b.minZ, b.maxX, b.maxZ, b.height, WHITE);
        } else {
            // Fallback primitive, tinted like the legacy renderer's boxes.
            const unsigned char tint =
                static_cast<unsigned char>(120 + (b.facadeKind * 37) % 90);
            const Vector3 center{(b.minX + b.maxX) * 0.5f, b.height * 0.5f,
                                 (b.minZ + b.maxZ) * 0.5f};
            DrawCube(center, b.maxX - b.minX, b.height, b.maxZ - b.minZ,
                     Color{tint, tint, static_cast<unsigned char>(tint + 20), 255});
            DrawCubeWires(center, b.maxX - b.minX, b.height, b.maxZ - b.minZ,
                          Color{40, 45, 60, 255});
        }
    }
}

void RaylibRenderer::drawCharacter(const CharacterVisual& visual) {
    // Animated characters land with issue #38; marker cylinder meanwhile.
    const Vec3& p = visual.position;
    DrawCylinder({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{200, 120, 80, 255});
    DrawCylinderWires({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{60, 30, 20, 255});
}

void RaylibRenderer::endFrame() { EndMode3D(); }

bool RaylibRenderer::worldToScreen(const Vec3& world, Vector2& out) const {
    // Reject points behind the camera: GetWorldToScreen would mirror them.
    const Vector3 toPoint{world.x - camera_.position.x, world.y - camera_.position.y,
                          world.z - camera_.position.z};
    const Vector3 forward{camera_.target.x - camera_.position.x,
                          camera_.target.y - camera_.position.y,
                          camera_.target.z - camera_.position.z};
    if (toPoint.x * forward.x + toPoint.y * forward.y + toPoint.z * forward.z <= 0.f) {
        return false;
    }
    out = GetWorldToScreen({world.x, world.y, world.z}, camera_);
    return true;
}

}  // namespace llm_npc
