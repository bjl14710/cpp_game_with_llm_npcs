#pragma once

#include <vector>

#include "Math.hpp"

namespace llm_npc {

// Index into `targets` of the nearest one that lies inside a forward cone from
// `eye` facing yaw `yawDeg` (0 = +Z, 90 = +X), within `range` units and within
// `halfAngleDeg` of the facing direction; -1 when nothing qualifies.
// Ground-plane (XZ) only. Drives both melee (short range, wide cone) and the
// pistol (long range, narrow cone) so aim logic is one tested function.
int pickTargetInCone(const Vec3& eye, float yawDeg, float range, float halfAngleDeg,
                     const std::vector<Vec3>& targets);

}  // namespace llm_npc
