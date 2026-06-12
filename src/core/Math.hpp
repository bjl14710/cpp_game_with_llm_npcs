#pragma once

#include <algorithm>
#include <cmath>

namespace llm_npc {

// Minimal 3D vector used for world positions, movement, and the camera.
// Kept SFML-free so core logic and unit tests build without graphics libs.
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

}  // namespace llm_npc
