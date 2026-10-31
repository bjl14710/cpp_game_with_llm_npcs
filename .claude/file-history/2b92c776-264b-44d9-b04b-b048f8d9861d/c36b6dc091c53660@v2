#include "Combat.hpp"

#include <cmath>
#include <cstddef>

namespace llm_npc {

int pickTargetInCone(const Vec3& eye, float yawDeg, float range, float halfAngleDeg,
                     const std::vector<Vec3>& targets) {
    const float yaw = degToRad(yawDeg);
    const float fx = std::sin(yaw);  // forward on the ground plane: 0deg = +Z
    const float fz = std::cos(yaw);
    const float cosHalf = std::cos(degToRad(halfAngleDeg));

    int best = -1;
    float bestDist = range;  // only beats targets nearer than the max range
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const float dx = targets[i].x - eye.x;
        const float dz = targets[i].z - eye.z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > range || dist < 1e-3f) continue;
        const float facingDot = (dx * fx + dz * fz) / dist;  // cos of angle to forward
        if (facingDot < cosHalf) continue;                   // outside the cone
        if (dist < bestDist) {
            bestDist = dist;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}  // namespace llm_npc
