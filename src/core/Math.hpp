#pragma once

#include <algorithm>
#include <cmath>

namespace llm_npc {

// Minimal 3D vector used for world positions, movement, and the camera.
// Kept rendering-free so core logic and unit tests build without graphics libs.
struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// Component-wise vector sum.
inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

// Component-wise vector difference.
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

// Scale a vector by a scalar.
inline Vec3 operator*(const Vec3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }

// In-place component-wise sum.
inline Vec3& operator+=(Vec3& a, const Vec3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

// Dot product.
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// Cross product (right-handed).
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Euclidean length.
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

// Unit-length copy of v; returns v unchanged when its length is ~zero.
inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    if (len < 1e-6f) return v;
    return v * (1.f / len);
}

// Unit-length direction from `from` to `to` ON THE GROUND PLANE, y always 0.
// Returns a zero vector when the two are vertically stacked or coincident.
//
// THE ONE WAY A GROUND CREATURE STEERS. Using normalize() on a difference of
// two world positions puts the vertical gap into the step, and every mover in
// this codebase then hands that step to City::resolveMovement, which ends with
// `pos.y = to.y` and writes it through. Nothing clamps it afterwards, so an NPC
// following a jumping player climbs into the air and an NPC fleeing a player
// on a roof burrows into the ground — permanently, because the drift
// accumulates.
//
// It is the same disagreement the moonwalk bug was: the distance test beside
// each of those steps already used distanceXZ, so the code was half-horizontal
// and half not. Pair this with distanceXZ and they agree.
inline Vec3 steerXZ(const Vec3& from, const Vec3& to) {
    return normalize(Vec3{to.x - from.x, 0.f, to.z - from.z});
}

// Distance between two points, ignoring the vertical (y) axis. Used for
// ground-plane queries like "is the player near this NPC".
inline float distanceXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Clamp v into [lo, hi].
inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

// Degrees -> radians.
inline float degToRad(float deg) { return deg * 3.14159265358979323846f / 180.f; }

// Signed shortest angular difference (toDeg - fromDeg) wrapped to [-180, 180].
// Used to ease a rotation toward a target along the short way round.
inline float shortestAngleDelta(float fromDeg, float toDeg) {
    float d = std::fmod(toDeg - fromDeg + 180.f, 360.f);
    if (d < 0.f) d += 360.f;
    return d - 180.f;
}

// Player eye height above the ground position. The renderer camera and the
// weapon spawn origin BOTH read this so a shot always leaves from the same
// point the player is looking through (the camera literal at
// RaylibRenderer::beginFrame derives from this too).
constexpr float kEyeHeight = 1.7f;

// The single authoritative "where the player is looking" vector, derived from
// yaw AND pitch. The camera (RaylibRenderer::beginFrame) and every weapon
// (World::playerAttack) both derive their aim from THIS one function, so a shot
// can never drift out of sync with the crosshair the way it did when combat
// reconstructed a separate yaw-only, horizontal aim (the moonwalk class of
// bug — see issue #91). Convention (matches the camera basis):
//   yaw 0 -> +Z,  yaw +90 -> +X,  pitch >0 -> +Y (looking up).
// Returns a normalized vector; at zero pitch it equals the horizontal forward
// (sin yaw, 0, cos yaw) that WASD movement uses.
inline Vec3 lookDirection(float yawDeg, float pitchDeg) {
    const float yr = degToRad(yawDeg);
    const float pr = degToRad(pitchDeg);
    const float cp = std::cos(pr);
    return normalize(Vec3{std::sin(yr) * cp, std::sin(pr), std::cos(yr) * cp});
}

// First-person camera pose. `position` is the FEET on the ground plane —
// RaylibRenderer::beginFrame adds kEyeHeight to get the eye. Anything wanting
// to place the camera at a literal height hands over that height MINUS
// kEyeHeight, which is what the sandbox editor's vantage and cutscene playback
// both do.
//
// This lives in core rather than next to the renderer because it is the
// interface between whatever decides where the camera goes and the one place
// that builds a Camera3D from it. Player input drives it, and so does cutscene
// playback (Cutscene.hpp) — which is a core type and cannot include an app
// header. Duplicating the struct so each layer owns a copy is the first step
// toward the two drifting apart.
//
// Fields match the legacy Renderer3D's CameraPose, so main.cpp's movement and
// mouse-look code carried over unchanged.
struct CameraPose {
    Vec3 position{};
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

}  // namespace llm_npc
