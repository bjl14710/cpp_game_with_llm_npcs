#pragma once

#include <string>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// An axis-aligned solid block of the city: a building, kiosk, fountain,
// parked car — anything the player cannot walk through.
struct Building {
    std::string id;    // stable identifier, e.g. "bakery"
    std::string name;  // sign / label text; empty for filler and obstacles
    float minX = 0.f;
    float minZ = 0.f;
    float maxX = 0.f;
    float maxZ = 0.f;
    float height = 0.f;
    int facadeKind = 0;  // texture/tint selector for the renderer
};

// The walkable city: a square ground plane bounded at +-halfSize() with
// solid axis-aligned buildings. Collision treats the player as a circle on
// the ground (XZ) plane.
class City {
   public:
    // The hand-laid downtown map: 3x3 blocks, named shops on street fronts,
    // an open plaza in the middle, and a park in the north-east corner.
    static City makeDowntown();

    // Half the side length of the square world; walkable X/Z are within
    // [-halfSize, halfSize].
    float halfSize() const { return halfSize_; }

    const std::vector<Building>& buildings() const { return buildings_; }

    // Find a building by id; nullptr when absent.
    const Building* findBuilding(const std::string& id) const;

    // True when a circle at (x, z) with `radius` overlaps any building that
    // is still solid at foot height `feetY`: a building only blocks while
    // its top is above the feet, so an airborne (or perched) mover clears
    // obstacles it has risen past. The default keeps ground-level behaviour
    // (everything solid) for existing callers.
    bool circleIntersectsAny(float x, float z, float radius, float feetY = 0.f) const;

    // The height of the surface supporting a circle at (x, z) whose feet
    // are at `feetY`: the tallest building top at or below the feet among
    // those the circle overlaps, or 0 (the ground). This is what a jumping
    // player lands on — walk off the edge and the support drops away.
    float supportHeightAt(float x, float z, float radius, float feetY) const;

    // Move a circle of `radius` from `from` toward `to`, sliding along
    // obstacles: each axis is applied independently and dropped if it would
    // collide, so walking diagonally into a wall glides along it. Solidity
    // is judged at the mover's foot height (from.y), so a jump can carry
    // the player over anything lower than the arc.
    // The result is also clamped to the world bounds. y passes through.
    Vec3 resolveMovement(const Vec3& from, const Vec3& to, float radius) const;

   private:
    float halfSize_ = 110.f;
    std::vector<Building> buildings_;
};

}  // namespace llm_npc
