#include "RaylibRenderer.hpp"

namespace llm_npc {

RaylibRenderer::RaylibRenderer(Assets& assets) : assets_(assets) {}

void RaylibRenderer::beginFrame(const CameraPose& pose) {
    // TODO(implement): camera_.position = pose.position (+eye height);
    // target = position + forward(yawDeg, pitchDeg); fovy ~70; BeginMode3D.
    // Clear to the sky color; enable the fog/tint shader.
    (void)pose;
}

void RaylibRenderer::drawCity(const City& city) {
    // TODO(implement): DrawPlane ground + road strips; for each
    // city.buildings() pick assets_.modelForBuilding(b) and DrawModelEx at
    // the AABB footprint (scale to fit minX/maxX × minZ/maxZ, height).
    // Fallback: DrawCube with the legacy tint when assets are missing.
    (void)city;
}

void RaylibRenderer::drawCharacter(const CharacterVisual& visual) {
    // TODO(implement): model = assets_.modelForCharacter(visual.variantSeed);
    // pick idle/walk/gesture clip, UpdateModelAnimation with a per-entity
    // clock, set the head material's texture to
    // assets_.faceTexture(visual.face), DrawModelEx rotated by facingDeg.
    // Fallback: capsule + billboard face when the model is unavailable.
    (void)visual;
}

void RaylibRenderer::endFrame() {
    // TODO(implement): EndMode3D().
}

bool RaylibRenderer::worldToScreen(const Vec3& world, Vector2& out) const {
    // TODO(implement): behind-camera check via dot(world - cam, forward),
    // then out = GetWorldToScreen({x,y,z}, camera_).
    (void)world;
    (void)out;
    return false;
}

}  // namespace llm_npc
