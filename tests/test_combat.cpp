// Tests for Combat::pickTargetInCone — weapon target selection.
#include <vector>

#include "Combat.hpp"
#include "Math.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("pickTargetInCone selects the nearest target ahead, ignoring behind") {
    const Vec3 eye{0.f, 0.f, 0.f};  // facing +Z (yaw 0)
    std::vector<Vec3> targets = {
        {0.f, 0.f, 30.f},  // far ahead
        {0.f, 0.f, 5.f},   // near ahead  <- nearest in cone
        {0.f, 0.f, -5.f},  // behind
    };
    CHECK(pickTargetInCone(eye, 0.f, 70.f, 5.f, targets) == 1);
}

TEST_CASE("targets beyond range or outside the cone are ignored") {
    const Vec3 eye{0.f, 0.f, 0.f};
    std::vector<Vec3> targets = {
        {0.f, 0.f, 100.f},  // dead ahead but past the 70-unit range
        {20.f, 0.f, 1.f},   // close-ish but ~87 deg off the +Z axis
    };
    CHECK(pickTargetInCone(eye, 0.f, 70.f, 5.f, targets) == -1);
}

TEST_CASE("a wide melee cone catches an off-axis target a narrow cone misses") {
    const Vec3 eye{0.f, 0.f, 0.f};
    std::vector<Vec3> targets = {{1.0f, 0.f, 1.732f}};  // 30 deg off +Z, 2 units away
    CHECK(pickTargetInCone(eye, 0.f, 3.f, 35.f, targets) == 0);   // wide melee cone: hit
    CHECK(pickTargetInCone(eye, 0.f, 3.f, 5.f, targets) == -1);   // narrow pistol cone: miss
}

TEST_CASE("yaw aims the cone (0 = +Z, 90 = +X)") {
    const Vec3 eye{0.f, 0.f, 0.f};
    std::vector<Vec3> targets = {
        {6.f, 0.f, 0.f},  // due +X
        {0.f, 0.f, 6.f},  // due +Z
    };
    CHECK(pickTargetInCone(eye, 90.f, 70.f, 10.f, targets) == 0);  // facing +X
    CHECK(pickTargetInCone(eye, 0.f, 70.f, 10.f, targets) == 1);   // facing +Z
}
