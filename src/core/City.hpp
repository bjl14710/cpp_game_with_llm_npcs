#pragma once

#include <cmath>
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

// One street lamp: a pole standing at (x, z) whose arm reaches toward the
// street along `yawDeg` (game facing convention: 0 looks toward +Z). The
// pole is solid — City adds a collider for every lamp — and the light hangs
// at lampHead(), the point the renderer's lamp pools radiate from.
struct StreetLamp {
    float x = 0.f;
    float z = 0.f;
    float yawDeg = 0.f;
    float height = 5.5f;  // pole height in world units
};

// Horizontal reach of a lamp's arm from its pole, and how far below the
// pole top the bulb hangs (matches the pack's streetlight silhouette).
constexpr float kLampArmReach = 1.2f;
constexpr float kLampBulbDrop = 0.35f;

// World position of a lamp's bulb: the arm's end, just under the pole top.
inline Vec3 lampHead(const StreetLamp& lamp) {
    const float yaw = degToRad(lamp.yawDeg);
    return {lamp.x + std::sin(yaw) * kLampArmReach, lamp.height - kLampBulbDrop,
            lamp.z + std::cos(yaw) * kLampArmReach};
}

// The walkable city: a square ground plane bounded at +-halfSize() with
// solid axis-aligned buildings. Collision treats the player as a circle on
// the ground (XZ) plane.
class City {
   public:
    // The hand-laid downtown map: 3x3 blocks, named shops on street fronts,
    // an open plaza in the middle, and a park in the north-east corner.
    static City makeDowntown();

    // A city from an explicit building list (sandbox maps compile through
    // this — see SandboxMap::buildCity). Such cities have no downtown
    // street grid: hasStreets() tells the renderer to skip the hardcoded
    // road/slab pass.
    static City fromBuildings(std::vector<Building> buildings, float halfSize);

    // False for sandbox-built cities (no authored street grid).
    bool hasStreets() const { return hasStreets_; }

    // Half the side length of the square world; walkable X/Z are within
    // [-halfSize, halfSize].
    float halfSize() const { return halfSize_; }

    const std::vector<Building>& buildings() const { return buildings_; }

    // Street lamps authored with the map (empty for sandbox cities). Each
    // also appears in buildings() as a pole collider with id
    // "streetlamp#<index>", so lamps can never be walked through.
    const std::vector<StreetLamp>& streetLamps() const { return lamps_; }

    // Bulb positions of every street lamp — the light sources the renderer
    // picks its nearest few from each frame.
    std::vector<Vec3> lampHeads() const;

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
    std::vector<StreetLamp> lamps_;
    bool hasStreets_ = true;
};

}  // namespace llm_npc
