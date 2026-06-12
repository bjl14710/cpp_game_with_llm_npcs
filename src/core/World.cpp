#include "World.hpp"

namespace llm_npc {

int World::nearestNpcWithin(const Vec3& pos, float radius) const {
    int best = -1;
    float bestDist = radius;
    for (std::size_t i = 0; i < npcs_.size(); ++i) {
        const float dist = distanceXZ(pos, npcs_[i].position());
        if (dist <= bestDist) {
            bestDist = dist;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}  // namespace llm_npc
