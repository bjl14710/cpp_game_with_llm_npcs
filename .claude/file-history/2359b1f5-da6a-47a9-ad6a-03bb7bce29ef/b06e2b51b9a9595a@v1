#pragma once

#include <vector>

#include "City.hpp"
#include "Math.hpp"
#include "Npc.hpp"

namespace llm_npc {

// The complete game world: the city geometry plus every NPC living in it.
// SFML-free so proximity and placement logic stays unit-testable.
class World {
   public:
    explicit World(City city) : city_(std::move(city)) {}

    const City& city() const { return city_; }

    // Adds an NPC (already placed via setPlacement).
    void addNpc(Npc npc) { npcs_.push_back(std::move(npc)); }

    std::vector<Npc>& npcs() { return npcs_; }
    const std::vector<Npc>& npcs() const { return npcs_; }

    // Index of the NPC closest to `pos` on the ground plane, provided they
    // are within `radius`; -1 when nobody is in range. Drives the
    // "[T] Talk to <name>" prompt and chat targeting.
    int nearestNpcWithin(const Vec3& pos, float radius) const;

   private:
    City city_;
    std::vector<Npc> npcs_;
};

}  // namespace llm_npc
