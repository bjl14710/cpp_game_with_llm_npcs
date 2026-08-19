#pragma once

#include <string>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// Which face of a building footprint holds its doorway, named by the outward
// direction of that wall.
enum class DoorSide { NegX, PosX, NegZ, PosZ };

// An axis-aligned footprint in the ground (XZ) plane.
struct AABB {
    float minX = 0.f;
    float minZ = 0.f;
    float maxX = 0.f;
    float maxZ = 0.f;
};

// A block of the city. Solid by default (a parked car, the hot-dog cart, a
// tall apartment); when `enterable`, it is a hollow shell the player can walk
// into through a doorway on `doorSide`, and collision uses its wall segments
// instead of the whole footprint.
struct Building {
    std::string id;    // stable identifier, e.g. "bakery"
    std::string name;  // sign / label text; empty for filler and obstacles
    float minX = 0.f;
    float minZ = 0.f;
    float maxX = 0.f;
    float maxZ = 0.f;
    float height = 0.f;
    int facadeKind = 0;            // texture/tint selector for the renderer
    bool enterable = false;        // hollow shell with a doorway?
    DoorSide doorSide = DoorSide::PosZ;  // which wall the doorway sits in
};

// Shared shell geometry constants, used by both collision and rendering so the
// walls a player bumps line up exactly with the walls drawn.
inline constexpr float kWallThickness = 0.6f;  // shop wall depth
inline constexpr float kDoorWidth = 4.0f;       // doorway gap (player r = 0.45)
inline constexpr float kDoorHeight = 3.2f;      // doorway opening height

// The wall footprints of an enterable building: four perimeter walls with the
// door side split into two segments leaving a `kDoorWidth` gap. Single source
// of truth for the hollow-shell layout.
std::vector<AABB> buildingWallSegments(const Building& b);

// World-space center of an enterable building's doorway, at ground level.
Vec3 buildingDoorCenter(const Building& b);

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

    // The solid obstacles for collision: solid buildings contribute their
    // whole footprint, enterable shops contribute their wall segments (so the
    // doorway and interior are walkable). Cached; rebuilt when the map loads.
    const std::vector<AABB>& collisionBoxes() const { return collisionBoxes_; }

    // True when a circle at (x, z) with `radius` overlaps any collision box.
    bool circleIntersectsAny(float x, float z, float radius) const;

    // Move a circle of `radius` from `from` toward `to`, sliding along
    // obstacles: each axis is applied independently and dropped if it would
    // collide, so walking diagonally into a wall glides along it.
    // The result is also clamped to the world bounds. y passes through.
    Vec3 resolveMovement(const Vec3& from, const Vec3& to, float radius) const;

   private:
    // Recomputes collisionBoxes_ from buildings_; call after the map is set.
    void rebuildCollision();

    float halfSize_ = 110.f;
    std::vector<Building> buildings_;
    std::vector<AABB> collisionBoxes_;
};

}  // namespace llm_npc
