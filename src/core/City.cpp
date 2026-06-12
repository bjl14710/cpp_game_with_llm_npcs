#include "City.hpp"

namespace llm_npc {

namespace {

// Squared distance from point (x, z) to the nearest point of building b.
float distSqToBuilding(const Building& b, float x, float z) {
    const float cx = clampf(x, b.minX, b.maxX);
    const float cz = clampf(z, b.minZ, b.maxZ);
    const float dx = x - cx;
    const float dz = z - cz;
    return dx * dx + dz * dz;
}

}  // namespace

City City::makeDowntown() {
    // Layout reference: 3x3 city blocks on a 64-unit pitch. Block centers sit
    // at -64/0/64 on each axis and each block spans +-24 around its center,
    // leaving 16-unit streets between blocks. The center block is an open
    // plaza; the north-east block is a park.
    City city;
    city.halfSize_ = 110.f;
    city.buildings_ = {
        // Named shops, fronts flush with their street edge.
        {"bakery", "Marge's Bakery", -84.f, -64.f, -56.f, -40.f, 12.f, 0},
        {"police", "City Police Station", -20.f, -80.f, 20.f, -40.f, 16.f, 1},
        {"coffee", "Bean There Coffee", 44.f, -62.f, 72.f, -40.f, 10.f, 2},
        {"library", "City Library", -80.f, -16.f, -40.f, 16.f, 15.f, 3},
        {"hardware", "Jensen Hardware", 40.f, -14.f, 72.f, 14.f, 11.f, 4},

        // Plaza furniture and street props (low obstacles).
        {"cart", "Gus's Hot Dogs", -3.f, -2.f, 3.f, 2.f, 3.f, 5},
        {"taxi_cab", "", 26.f, -6.f, 34.f, -1.f, 1.6f, 6},
        {"fountain", "", 60.f, 56.f, 68.f, 64.f, 1.5f, 7},
        {"bench", "", 66.f, 70.f, 70.f, 71.f, 0.6f, 8},

        // Unnamed filler so every block reads as a dense city.
        {"apt_a", "", -88.f, -88.f, -68.f, -70.f, 22.f, 9},
        {"apt_b", "", -60.f, -88.f, -40.f, -72.f, 18.f, 9},
        {"office_a", "", 44.f, -88.f, 88.f, -66.f, 26.f, 10},
        {"apts_c", "", -88.f, 44.f, -44.f, 84.f, 24.f, 9},
        {"tower", "", -24.f, 48.f, 24.f, 88.f, 30.f, 10},
    };
    return city;
}

const Building* City::findBuilding(const std::string& id) const {
    for (const auto& b : buildings_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

bool City::circleIntersectsAny(float x, float z, float radius) const {
    const float r2 = radius * radius;
    for (const auto& b : buildings_) {
        if (distSqToBuilding(b, x, z) < r2) return true;
    }
    return false;
}

Vec3 City::resolveMovement(const Vec3& from, const Vec3& to, float radius) const {
    const float lo = -halfSize_ + radius;
    const float hi = halfSize_ - radius;

    Vec3 pos = from;
    const float nx = clampf(to.x, lo, hi);
    if (!circleIntersectsAny(nx, pos.z, radius)) pos.x = nx;
    const float nz = clampf(to.z, lo, hi);
    if (!circleIntersectsAny(pos.x, nz, radius)) pos.z = nz;
    pos.y = to.y;
    return pos;
}

}  // namespace llm_npc
