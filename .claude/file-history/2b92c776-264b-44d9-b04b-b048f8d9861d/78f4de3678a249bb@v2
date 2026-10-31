#pragma once

#include <cstdint>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// One car driving the avenue grid. Cars travel in a straight line along one
// avenue (never turning), in the right-hand lane for their direction, and loop
// from one edge of the map to the other. Pose fields (x/z/headingDeg) are
// derived each update from the lane state for rendering and collision.
struct Vehicle {
    int axis = 0;        // 0 = travels along Z (N-S street); 1 = travels along X
    float line = 0.f;    // the avenue centerline coordinate (perpendicular axis)
    int dir = 1;         // +1 or -1 along the travel axis
    float pos = 0.f;     // position along the travel axis
    float speed = 0.f;   // current speed (units/sec)
    float maxSpeed = 9.f;
    int colorId = 0;     // renderer palette selector

    float x = 0.f;          // derived world position
    float z = 0.f;
    float headingDeg = 0.f;  // derived facing (0 = +Z, 90 = +X)
};

// Deterministic, SFML-free street traffic over the avenue grid. Cars follow
// their lane, brake for the player and for cars ahead, and yield at crossings
// (north-south traffic yields to east-west) so they don't drive through each
// other. Built and unit-tested headlessly; the renderer only draws what it
// reports.
class Traffic {
   public:
    Traffic() = default;

    // Builds `count` cars spread across the avenue grid within +-halfSize,
    // seeded so the same seed yields the same traffic.
    Traffic(float halfSize, std::uint32_t seed, int count);

    // Advances every car by `dt` seconds given the player's position (cars
    // always brake for the player).
    void update(float dt, const Vec3& playerPos);

    const std::vector<Vehicle>& vehicles() const { return vehicles_; }

    // True when a circle at (x, z) with `radius` overlaps any car's body, whose
    // footprint is axis-aligned because cars only drive along the grid. Used
    // for player-vs-car collision.
    bool circleHitsVehicle(float x, float z, float radius) const;

   private:
    // Recomputes x/z/headingDeg from a car's lane state.
    void refreshPose(Vehicle& v) const;
    // Next pseudo-random float in [0, 1) (no <random> in core).
    float rand01();

    float halfSize_ = 0.f;
    std::vector<Vehicle> vehicles_;
    std::uint32_t rng_ = 1u;
};

}  // namespace llm_npc
