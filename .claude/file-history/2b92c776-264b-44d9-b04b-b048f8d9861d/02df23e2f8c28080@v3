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

}  // namespace llm_npc
