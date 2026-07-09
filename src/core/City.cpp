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

        // Street furniture with real collision — everything that renders
        // as a solid object is authored here so visuals and colliders can
        // never diverge (the renderer draws these from this list).
        {"trafficlight_a", "", -23.5f, -23.5f, -22.5f, -22.5f, 4.5f, 11},
        {"trafficlight_b", "", -23.5f, 40.5f, -22.5f, 41.5f, 4.5f, 11},
        {"trafficlight_c", "", 40.5f, -23.5f, 41.5f, -22.5f, 4.5f, 11},
        {"trafficlight_d", "", 40.5f, 40.5f, 41.5f, 41.5f, 4.5f, 11},
        {"bush_a", "", 46.8f, 46.8f, 49.2f, 49.2f, 1.1f, 12},
        {"bush_b", "", 78.8f, 50.8f, 81.2f, 53.2f, 1.1f, 12},
        {"bush_c", "", 50.8f, 78.8f, 53.2f, 81.2f, 1.1f, 12},
        {"bush_d", "", 74.8f, 74.8f, 77.2f, 77.2f, 1.1f, 12},

        // Parked cars, curbside on the two Z-running streets (KayKit cars
        // face along Z at identity and the Uniform draw path doesn't
        // rotate). Spots avoid the zebra crossings (x/z +-16 and +-48), the
        // junctions, shop doors, and every persona's home position. The
        // police car parks beside its station block.
        {"police_car", "", -27.4f, -62.8f, -23.6f, -58.8f, 1.5f, 13},
        {"hatchback_b", "", 23.8f, -68.8f, 27.4f, -64.8f, 1.35f, 13},
        {"sedan_a", "", 36.7f, -8.f, 40.4f, -4.f, 1.35f, 13},
        {"sedan_b", "", 36.7f, 61.5f, 40.4f, 65.5f, 1.35f, 13},
        {"hatchback_a", "", -40.4f, 59.5f, -36.7f, 63.5f, 1.35f, 13},

        // Alley props: dumpster + trash between the SW apartments, a bin in
        // the coffee/office alley.
        {"dumpster_a", "", -67.2f, -86.f, -63.2f, -83.5f, 1.4f, 14},
        {"trash_a", "", -62.5f, -87.4f, -61.1f, -86.f, 0.9f, 14},
        {"trash_b", "", 45.f, -65.4f, 46.2f, -64.2f, 0.7f, 14},

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

bool City::circleIntersectsAny(float x, float z, float radius, float feetY) const {
    const float r2 = radius * radius;
    // A building stops being solid once the mover's feet reach its top
    // (small tolerance so standing exactly on a roof doesn't wedge).
    const float clearance = feetY + 0.01f;
    for (const auto& b : buildings_) {
        if (b.height <= clearance) continue;
        if (distSqToBuilding(b, x, z) < r2) return true;
    }
    return false;
}

float City::supportHeightAt(float x, float z, float radius, float feetY) const {
    const float r2 = radius * radius;
    float support = 0.f;  // the ground plane
    for (const auto& b : buildings_) {
        // Only tops at or below the feet can hold them up; anything higher
        // is a wall beside the mover, not a floor beneath them.
        if (b.height > feetY + 0.01f || b.height <= support) continue;
        if (distSqToBuilding(b, x, z) < r2) support = b.height;
    }
    return support;
}

Vec3 City::resolveMovement(const Vec3& from, const Vec3& to, float radius) const {
    const float lo = -halfSize_ + radius;
    const float hi = halfSize_ - radius;

    Vec3 pos = from;
    const float nx = clampf(to.x, lo, hi);
    if (!circleIntersectsAny(nx, pos.z, radius, from.y)) pos.x = nx;
    const float nz = clampf(to.z, lo, hi);
    if (!circleIntersectsAny(pos.x, nz, radius, from.y)) pos.z = nz;
    pos.y = to.y;
    return pos;
}

}  // namespace llm_npc
