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

// Squared Euclidean distance — for comparing against a squared threshold
// (range gates like the outline cutoff in issue #171), where length()'s
// sqrt buys nothing. Full 3D on purpose: pair with distanceXZ only when the
// test must agree with ground-plane steering, which a view-distance check
// must not.
inline float distanceSquared(const Vec3& a, const Vec3& b) {
    return dot(a - b, a - b);
}

// The distance at which exponential-squared fog of the given density fully
// hides a surface on an 8-bit framebuffer (issue #170). The shaders mix
// toward the fog color by 1 - exp(-(d * density)^2); once the surviving
// contribution exp(-(d * density)^2) drops below one 8-bit step (1/255),
// mixing the surface in changes no pixel:
//     exp(-(d * density)^2) <= 1/255   =>   d >= sqrt(ln 255) / density
// The NPC cull radius is DERIVED from this so retuning the haze moves the
// cull with it — a hand-picked radius is the popping bug waiting to happen.
inline float fogOpaqueDistance(float density) {
    return std::sqrt(std::log(255.f)) / density;
}

// Conservative behind-camera reject (issue #272): true only when the whole
// bounding sphere (center, radius) lies strictly behind the plane through
// `eye` with outward normal `forward`. Everything a perspective camera can
// see lies in front of that plane, so a rejected sphere could not have
// contributed a pixel — the test can only ever skip invisible work, never
// clip a visible one. Half-space only, deliberately NOT the full frustum
// cone: the cheap plane test already removes the dominant case (figures
// behind the eye) and cannot be wrong at the screen edges, where a cone
// test with a mis-sized margin would clip shoulders. `forward` need not be
// unit length — the margin is scaled by it, and a degenerate zero forward
// rejects nothing (0 < 0 is false), which fails safe: the figure draws.
inline bool sphereFullyBehind(const Vec3& eye, const Vec3& forward,
                              const Vec3& center, float radius) {
    return dot(center - eye, forward) < -radius * length(forward);
}

// Screen-space HUD label de-collision (issue #274): two text labels whose
// anchors sit closer than the clear box on both axes garble into each other.
// The box is FIXED-SIZE on purpose, not measured from the text: one
// reservation whatever the string, so placement cannot oscillate as a
// nameplate's activity suffix changes length frame to frame. Strict < on
// both axes — anchors exactly a clearance apart do not collide, matching
// the strict-< convention of every other gate in this file. Callers: the
// single shared placed-labels pass in main.cpp (nameplates claim first,
// signage takes what is left).
inline bool labelBoxesOverlap(float ax, float ay, float bx, float by,
                              float clearX, float clearY) {
    return std::fabs(ax - bx) < clearX && std::fabs(ay - by) < clearY;
}

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
