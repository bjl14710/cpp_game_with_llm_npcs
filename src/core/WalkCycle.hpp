#pragma once

#include "Math.hpp"

namespace llm_npc {

// Procedural walk/run cycle for composite characters (design: "NPC Walk
// Study"). Rendering-free so the gait math is unit-tested; the renderer only
// turns a WalkPose into transforms and leg geometry.
//
// The cycle is driven by DISTANCE walked, not the clock: feet stay planted
// at any speed, and no two NPCs share a beat. A blend eases the whole gait
// in and out so starts and stops never snap, and the measured speed blends
// the walk into a catch-up run (steady lean, level head, side sway).

// How strongly every layer reads; 1 is the tuned, natural walk.
constexpr float kWalkExaggeration = 1.f;

// Figure layout, world units. The legs take the bottom kHipHeight of the
// 1.8 u height contract; body and head shrink to fit the rest.
constexpr float kCharacterHeight = 1.8f;
constexpr float kHipHeight = 0.54f;
constexpr float kLegSegment = 0.31f;   // thigh = shin
// Stance legs may stretch this much to stay straight (the design used 1.06;
// the 3D run also spans the hip-to-track lateral gap, which needs 1.08).
constexpr float kLegStretch = 1.08f;
constexpr float kLegRadius = 0.065f;
constexpr float kHipHalfWidth = 0.14f;
constexpr float kAnkleHeight = 0.095f;

// One sample of the gait in the character's local frame: +z is facing,
// +y up, +x the character's left. Foot 0 sits at -x, foot 1 at +x.
struct WalkPose {
    float bodyLift = kHipHeight;  // body base above ground (before bob)
    float bob = 0.f;              // vertical offset of hips + body
    float shrink = (kCharacterHeight - kHipHeight) / kCharacterHeight;
    float leanDeg = 0.f;          // + pitches the body forward (about +x)
    float rollDeg = 0.f;          // + tips the body toward -x (about +z)
    float sway = 0.f;             // lateral hip offset, local x
    float squashXZ = 1.f;         // contact squash, applied at the body base
    float squashY = 1.f;
    float nodDeg = 0.f;           // + pitches the head forward about the neck
    float hairDeg = 0.f;          // + swings tails/buns up and back
    float footForward[2] = {0.f, 0.f};  // ankle z
    float footLift[2] = {0.f, 0.f};     // ankle height above its rest height
    float kneeStiff[2] = {1.f, 1.f};    // 1 = locked straight, 0 = free bend
    float track = 0.2f;                 // |ankle x| of each foot
};

// Per-character gait memory, advanced once per rendered frame.
struct WalkState {
    bool primed = false;
    Vec3 last{};
    float phase = 0.f;   // position in the cycle, 0..1 (two steps per cycle)
    float speed = 0.f;   // smoothed ground speed, u/s
    float move = 0.f;    // linear 0..1 toward moving; eased into the blend
    float run = 0.f;     // 0 = walk gait, 1 = run gait
    float gesture = 0.f; // 0..1 eased "talking" idle bounce
    float idleClock = 0.f;
    float startKick = -1.f;  // seconds since the last start; <0 = none
    float stopKick = -1.f;   // seconds since the last stop; <0 = none
    bool moving = false;
    // Time constant for the speed estimate. Positions that arrive in jumps
    // (12 Hz network snapshots) need a longer one or the run blend flickers.
    float speedSmoothing = 0.12f;
};

// A fresh state whose phase is offset by `seed`, so NPCs that start walking
// together still don't step in unison.
WalkState makeWalkState(unsigned seed);

// Feeds this frame's ground position. `gesturing` adds a small idle bounce
// so a talking NPC still reads as animated while standing.
void advanceWalk(WalkState& state, const Vec3& position, float dt,
                 bool gesturing = false);

// The pose for the state's current phase/blend.
WalkPose walkPose(const WalkState& state, float exaggeration = kWalkExaggeration);

// At rest: feet under the hips, legs straight, no motion layers.
WalkPose standingPose();

// Stride length (u per full cycle) at a given run blend.
float walkStride(float run);

// Two-bone leg IK. The ankle is pulled in when out of reach (and returned
// clamped); the knee bends toward `forward`, damped by `stiff`.
struct LegJoints {
    Vec3 hip, knee, ankle;
};
LegJoints solveLeg(const Vec3& hip, const Vec3& ankle, float segment,
                   float stretch, float stiff, const Vec3& forward = {0.f, 0.f, 1.f});

// Hip and target ankle of leg `j` (0 or 1) for a pose, local frame.
Vec3 poseHip(const WalkPose& pose, int j);
Vec3 poseAnkle(const WalkPose& pose, int j);

}  // namespace llm_npc
