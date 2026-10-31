#include "WalkCycle.hpp"

#include <cmath>

namespace llm_npc {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Gait tuning (design: straight-leg walk + catch-up run).
constexpr float kWalkStride = 1.6f;           // u per cycle at walk
constexpr float kRunStride = kWalkStride * 1.6f;
constexpr float kWalkBob = 0.03f;             // sin², peak at mid-stance
constexpr float kRunBob = 0.035f;             // low while a foot is down
constexpr float kWalkSquash = 0.25f;          // fraction of the base squash
constexpr float kWalkLeanDeg = 4.f;
constexpr float kRunLeanDeg = 11.f;           // steady, no fore-aft rocking
constexpr float kWalkClear = 0.08f;           // swing-foot lift
constexpr float kRunClear = 0.24f;

// Blend timing: starts ease in over 0.5 s, stops ease out over 0.7 s.
constexpr float kStartSeconds = 0.5f;
constexpr float kStopSeconds = 0.7f;
constexpr float kMovingSpeed = 0.5f;          // u/s; matches facing threshold
// Run gait fades in between these speeds (walk 3.8, run 6.0 u/s).
constexpr float kRunFrom = 4.2f;
constexpr float kRunFull = 5.4f;
constexpr float kRunSmoothing = 0.35f;
// A jump larger than this in one frame is a teleport, not a step.
constexpr float kTeleport = 3.f;

float lerp(float a, float b, float t) { return a + (b - a) * t; }
float easeInOutSine(float t) { return -(std::cos(kPi * clampf(t, 0.f, 1.f)) - 1.f) / 2.f; }
float wrap01(float v) { return v - std::floor(v); }

// Up-then-down over `d` seconds starting at t = 0; 0 outside.
float bump(float t, float d) {
    if (t < 0.f || t > d) return 0.f;
    return easeInOutSine(t / (d / 2.f)) - easeInOutSine((t - d / 2.f) / (d / 2.f));
}

}  // namespace

float walkStride(float run) { return lerp(kWalkStride, kRunStride, run); }

WalkState makeWalkState(unsigned seed) {
    WalkState s;
    // Golden-ratio spread keeps neighbouring seeds far apart in phase.
    s.phase = wrap01(static_cast<float>(seed) * 0.6180339887f);
    s.idleClock = s.phase;
    return s;
}

void advanceWalk(WalkState& s, const Vec3& position, float dt, bool gesturing) {
    if (!s.primed || dt <= 0.f || distanceXZ(position, s.last) > kTeleport) {
        s.primed = true;
        s.last = position;
        return;
    }
    const float step = distanceXZ(position, s.last);
    s.last = position;

    s.speed += (step / dt - s.speed) * (1.f - std::exp(-dt / s.speedSmoothing));
    const bool moving = s.speed > kMovingSpeed;
    if (moving && !s.moving && s.move < 0.1f) s.startKick = 0.f;
    if (!moving && s.moving) s.stopKick = 0.f;
    s.moving = moving;

    s.move = moving ? std::fmin(1.f, s.move + dt / kStartSeconds)
                    : std::fmax(0.f, s.move - dt / kStopSeconds);
    const float runTarget = clampf((s.speed - kRunFrom) / (kRunFull - kRunFrom), 0.f, 1.f);
    s.run += (runTarget - s.run) * (1.f - std::exp(-dt / kRunSmoothing));
    // Integrate with the CURRENT stride so planted feet stay planted while
    // the stride stretches into the run.
    s.phase = wrap01(s.phase + step / walkStride(s.run));

    const float g = gesturing && !moving ? 1.f : 0.f;
    s.gesture += (g - s.gesture) * (1.f - std::exp(-dt / 0.18f));
    s.idleClock += dt;

    if (s.startKick >= 0.f) s.startKick = s.startKick > 1.f ? -1.f : s.startKick + dt;
    if (s.stopKick >= 0.f) s.stopKick = s.stopKick > 2.f ? -1.f : s.stopKick + dt;
}

WalkPose walkPose(const WalkState& s, float ex) {
    WalkPose o;
    const float w = easeInOutSine(s.move);
    const float run = s.run;
    const float p = s.phase;
    const float c = 2.f * kPi * p;
    const float s1 = std::sin(c);
    const float s2 = std::sin(2.f * c - 1.2f);  // head lags the bob
    const float s3 = std::sin(2.f * c - 1.8f);  // hair lags further
    const float kc = std::pow(std::cos(c) * std::cos(c), 6.f);  // foot contact
    const float sf = lerp(0.5f, 0.33f, run);  // ground-contact fraction per foot

    const float walkBob = kWalkBob * ex * s1 * s1;
    const float runBob =
        kRunBob * ex * (1.f - std::cos(4.f * kPi * (p - sf / 2.f))) / 2.f;
    // Talking idle: a soft sin² bounce while the gesture plays.
    const float talkBob =
        0.025f * ex * std::pow(std::sin(kPi * 2.2f * s.idleClock), 2.f) * s.gesture;

    // Start: a small dip and lean-back. Stop: the lean overshoots and settles.
    const float startK = s.startKick >= 0.f ? bump(s.startKick, 0.6f) : 0.f;
    const float stopK = s.stopKick >= 0.f ? bump(s.stopKick - 0.4f, 0.9f) : 0.f;
    const float kickSq = -0.05f * startK;

    o.bob = lerp(walkBob, runBob, run) * w + talkBob - 0.03f * startK;

    const float sq = kWalkSquash * (1.f - run) * ex * kc * w;  // squash off in the run
    o.squashXZ = 1.f + 0.05f * sq - kickSq * 0.6f;
    o.squashY = 1.f - 0.07f * sq + kickSq;

    const float walkLean = (kWalkLeanDeg + 1.5f * std::sin(2.f * c)) * ex;
    o.leanDeg = lerp(walkLean, kRunLeanDeg, run) * w - 3.f * startK - 3.f * stopK;
    // The run's head counter-tilts against the lean so the gaze stays level.
    o.nodDeg = (4.f * ex * s2 * (1.f - 0.8f * run) - 0.45f * kRunLeanDeg * run) * w;
    o.hairDeg = lerp(6.f + 12.f * s3, 14.f + 4.f * s3, run) * w * ex;
    // Weight shifts over the stance foot: foot 0 (-x) is down while s1 > 0.
    o.rollDeg = lerp(5.f, 7.f, run) * ex * s1 * w;
    o.sway = -lerp(0.04f, 0.07f, run) * ex * s1 * w;
    o.track = lerp(0.2f, 0.11f, run);  // run lands closer to the centre line
    o.gait = w;
    o.clock = s.idleClock;

    const float span = walkStride(run) * sf;
    const float clear = lerp(kWalkClear, kRunClear, run);
    for (int j = 0; j < 2; ++j) {
        const float pf = wrap01(p + 0.5f * static_cast<float>(j));
        float stiff;
        if (pf < sf) {  // stance: planted, sliding back under the hips
            o.footForward[j] = (0.5f - pf / sf) * span * w;
            o.footLift[j] = 0.f;
            stiff = lerp(1.f, 0.3f, run);
        } else {  // swing: eased forward, arcing up
            const float u = (pf - sf) / (1.f - sf);
            o.footForward[j] = (-0.5f + easeInOutSine(u)) * span * w;
            o.footLift[j] = clear * std::sin(kPi * u) * w;
            stiff = 1.f - std::sin(kPi * u);
        }
        o.kneeStiff[j] = lerp(1.f, stiff, w);  // standing: both legs straight
    }
    return o;
}

WalkPose standingPose() { return WalkPose{}; }

LegJoints solveLeg(const Vec3& hip, const Vec3& ankle, float l, float stretch,
                   float stiff, const Vec3& forward) {
    const Vec3 v = ankle - hip;
    const float d = std::fmax(length(v), 1e-4f);
    const Vec3 dir = v * (1.f / d);
    const float dm = std::fmin(d, 2.f * l * stretch - 1e-3f);
    const Vec3 a = hip + dir * dm;
    const float h = std::sqrt(std::fmax(0.f, l * l - (dm / 2.f) * (dm / 2.f))) *
                    (1.f - 0.85f * stiff);
    // Bend toward `forward`, made perpendicular to the leg.
    Vec3 bend = forward - dir * dot(forward, dir);
    if (length(bend) < 1e-4f) bend = Vec3{0.f, 0.f, 1.f};
    bend = normalize(bend);
    return {hip, (hip + a) * 0.5f + bend * h, a};
}

Vec3 poseHip(const WalkPose& pose, int j) {
    const float side = j == 0 ? -1.f : 1.f;
    const float r = degToRad(pose.rollDeg);
    // Hips rotate with the roll: Rz(+roll) tips +x up.
    return {pose.sway + side * kHipHalfWidth * std::cos(r),
            pose.bodyLift + pose.bob + 0.05f + side * kHipHalfWidth * std::sin(r),
            0.f};
}

Vec3 poseAnkle(const WalkPose& pose, int j) {
    const float side = j == 0 ? -1.f : 1.f;
    return {side * pose.track, kAnkleHeight + pose.footLift[j], pose.footForward[j]};
}

}  // namespace llm_npc
