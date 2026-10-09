// Tests for the procedural composite walk: distance-locked phase, planted
// feet, eased starts/stops, the catch-up run blend, and leg IK reach.
#include <cmath>

#include "WalkCycle.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

constexpr float kDt = 1.f / 120.f;

// Walks a state along +z at `speed` for `seconds`, starting at `z`.
float walkFor(WalkState& s, float z, float speed, float seconds) {
    const int frames = static_cast<int>(seconds / kDt);
    for (int i = 0; i < frames; ++i) {
        z += speed * kDt;
        advanceWalk(s, Vec3{0.f, 0.f, z}, kDt);
    }
    return z;
}

}  // namespace

TEST_CASE("a standing character holds the rest pose") {
    WalkState s = makeWalkState(3);
    walkFor(s, 0.f, 0.f, 1.f);
    const WalkPose p = walkPose(s);
    CHECK(p.bob == doctest::Approx(0.f));
    CHECK(p.leanDeg == doctest::Approx(0.f));
    CHECK(p.footForward[0] == doctest::Approx(0.f));
    CHECK(p.footLift[1] == doctest::Approx(0.f));
    CHECK(p.kneeStiff[0] == doctest::Approx(1.f));
}

TEST_CASE("stance feet stay planted in the world while walking") {
    WalkState s = makeWalkState(0);
    float z = walkFor(s, 0.f, 3.8f, 2.f);  // blend fully in
    REQUIRE(s.move == doctest::Approx(1.f));
    REQUIRE(s.run < 0.01f);

    // Track foot 0 across one stance: its world z must not slide.
    int planted = 0;
    float anchor = 0.f;
    for (int i = 0; i < 240; ++i) {
        z += 3.8f * kDt;
        advanceWalk(s, Vec3{0.f, 0.f, z}, kDt);
        const WalkPose p = walkPose(s);
        if (p.footLift[0] > 1e-5f) {  // swinging: restart the anchor
            planted = 0;
            continue;
        }
        const float world = z + p.footForward[0];
        if (planted++ == 0) anchor = world;
        CHECK(world == doctest::Approx(anchor).epsilon(0.02));
    }
}

TEST_CASE("starts and stops ease instead of snapping") {
    WalkState s = makeWalkState(1);
    float z = walkFor(s, 0.f, 3.8f, 2.f);
    float prevLean = walkPose(s).leanDeg;
    // Stop dead: no single frame may change the lean by more than a degree.
    for (int i = 0; i < 240; ++i) {
        advanceWalk(s, Vec3{0.f, 0.f, z}, kDt);
        const float lean = walkPose(s).leanDeg;
        CHECK(std::fabs(lean - prevLean) < 1.f);
        prevLean = lean;
    }
    CHECK(s.move == doctest::Approx(0.f));
}

TEST_CASE("NPCs with different seeds step out of phase") {
    WalkState a = makeWalkState(0);
    WalkState b = makeWalkState(1);
    walkFor(a, 0.f, 3.8f, 1.f);
    walkFor(b, 0.f, 3.8f, 1.f);
    CHECK(std::fabs(a.phase - b.phase) > 0.1f);
}

TEST_CASE("catch-up speed blends into the run gait") {
    WalkState s = makeWalkState(0);
    walkFor(s, 0.f, 6.f, 3.f);
    CHECK(s.run > 0.95f);
    // Steady lean, squash off, head counter-tilted.
    float minLean = 1e9f, maxLean = -1e9f;
    for (int i = 0; i < 120; ++i) {
        walkFor(s, s.last.z, 6.f, kDt * 2.f);
        const WalkPose p = walkPose(s);
        minLean = std::fmin(minLean, p.leanDeg);
        maxLean = std::fmax(maxLean, p.leanDeg);
        CHECK(p.squashY == doctest::Approx(1.f).epsilon(0.002));
        CHECK(p.nodDeg < 0.f);
    }
    CHECK(maxLean - minLean < 0.5f);  // no fore-aft rocking
    CHECK(minLean > 10.f);
}

TEST_CASE("legs reach every planted foot at walk and run") {
    for (const float speed : {3.8f, 6.f}) {
        CAPTURE(speed);
        WalkState s = makeWalkState(0);
        walkFor(s, 0.f, speed, 3.f);
        for (int i = 0; i < 240; ++i) {
            walkFor(s, s.last.z, speed, kDt);
            const WalkPose p = walkPose(s);
            for (int j = 0; j < 2; ++j) {
                if (p.footLift[j] > 1e-5f) continue;
                const Vec3 target = poseAnkle(p, j);
                const LegJoints leg = solveLeg(poseHip(p, j), target, kLegSegment,
                                               kLegStretch, p.kneeStiff[j]);
                CHECK(length(leg.ankle - target) < 1e-3f);  // no floating foot
            }
        }
    }
}

TEST_CASE("the knee bends forward and segments keep their length") {
    const LegJoints leg = solveLeg(Vec3{0.f, 0.6f, 0.f}, Vec3{0.f, 0.1f, 0.f},
                                   kLegSegment, 1.f, 0.f);
    CHECK(leg.knee.z > 0.05f);
    CHECK(length(leg.knee - leg.hip) == doctest::Approx(kLegSegment).epsilon(0.01));
    CHECK(length(leg.ankle - leg.knee) == doctest::Approx(kLegSegment).epsilon(0.01));
}

TEST_CASE("a teleport does not spin the walk cycle") {
    WalkState s = makeWalkState(0);
    advanceWalk(s, Vec3{0.f, 0.f, 0.f}, kDt);
    const float before = s.phase;
    advanceWalk(s, Vec3{50.f, 0.f, 50.f}, kDt);
    CHECK(s.phase == doctest::Approx(before));
    CHECK(s.speed == doctest::Approx(0.f));
}

TEST_CASE("snapshot-rate positions keep a walking NPC in the walk gait") {
    // Network NPCs move in 12 Hz jumps while frames render at 60 Hz.
    WalkState s = makeWalkState(0);
    s.speedSmoothing = 0.3f;
    const float dt = 1.f / 60.f;
    float z = 0.f, maxRun = 0.f;
    for (int i = 0; i < 600; ++i) {
        if (i % 5 == 0) z += 3.8f / 12.f;
        advanceWalk(s, Vec3{0.f, 0.f, z}, dt);
        if (i > 120) maxRun = std::fmax(maxRun, s.run);
    }
    CHECK(s.moving);
    CHECK(maxRun < 0.1f);
}
