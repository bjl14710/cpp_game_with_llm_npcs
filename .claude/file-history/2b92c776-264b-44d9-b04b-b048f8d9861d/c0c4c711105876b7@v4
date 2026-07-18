#include "City.hpp"

namespace llm_npc {

namespace {

// Squared distance from point (x, z) to the nearest point of footprint a.
float distSqToAABB(const AABB& a, float x, float z) {
    const float cx = clampf(x, a.minX, a.maxX);
    const float cz = clampf(z, a.minZ, a.maxZ);
    const float dx = x - cx;
    const float dz = z - cz;
    return dx * dx + dz * dz;
}

}  // namespace

std::vector<AABB> buildingWallSegments(const Building& b) {
    const float t = kWallThickness;
    const float half = kDoorWidth * 0.5f;
    const float cx = (b.minX + b.maxX) * 0.5f;
    const float cz = (b.minZ + b.maxZ) * 0.5f;
    std::vector<AABB> walls;

    // West (-X) wall, split when the door is here.
    if (b.doorSide == DoorSide::NegX) {
        walls.push_back({b.minX, b.minZ, b.minX + t, cz - half});
        walls.push_back({b.minX, cz + half, b.minX + t, b.maxZ});
    } else {
        walls.push_back({b.minX, b.minZ, b.minX + t, b.maxZ});
    }
    // East (+X) wall.
    if (b.doorSide == DoorSide::PosX) {
        walls.push_back({b.maxX - t, b.minZ, b.maxX, cz - half});
        walls.push_back({b.maxX - t, cz + half, b.maxX, b.maxZ});
    } else {
        walls.push_back({b.maxX - t, b.minZ, b.maxX, b.maxZ});
    }
    // South (-Z) wall.
    if (b.doorSide == DoorSide::NegZ) {
        walls.push_back({b.minX, b.minZ, cx - half, b.minZ + t});
        walls.push_back({cx + half, b.minZ, b.maxX, b.minZ + t});
    } else {
        walls.push_back({b.minX, b.minZ, b.maxX, b.minZ + t});
    }
    // North (+Z) wall.
    if (b.doorSide == DoorSide::PosZ) {
        walls.push_back({b.minX, b.maxZ - t, cx - half, b.maxZ});
        walls.push_back({cx + half, b.maxZ - t, b.maxX, b.maxZ});
    } else {
        walls.push_back({b.minX, b.maxZ - t, b.maxX, b.maxZ});
    }
    return walls;
}

Vec3 buildingDoorCenter(const Building& b) {
    const float t = kWallThickness;
    const float cx = (b.minX + b.maxX) * 0.5f;
    const float cz = (b.minZ + b.maxZ) * 0.5f;
    switch (b.doorSide) {
        case DoorSide::NegX: return {b.minX + t * 0.5f, 0.f, cz};
        case DoorSide::PosX: return {b.maxX - t * 0.5f, 0.f, cz};
        case DoorSide::NegZ: return {cx, 0.f, b.minZ + t * 0.5f};
        case DoorSide::PosZ: return {cx, 0.f, b.maxZ - t * 0.5f};
    }
    return {cx, 0.f, cz};
}

City City::makeDowntown() {
    // Layout reference: 5x5 city blocks on a 64-unit pitch. Block centers sit
    // at -128/-64/0/64/128 on each axis and each block spans +-24 around its
    // center, leaving 16-unit streets between blocks (avenues run along
    // x/z = +-32 and +-96). The center block is an open plaza; the inner
    // north-east block is a park. The inner ring holds the named shops and the
    // police station; the outer ring is solid high-rise filler.
    //
    // The seven named shops are single-story hollow shells (enterable) with a
    // doorway on the side facing the plaza; everything else is a solid block.
    City city;
    city.halfSize_ = 170.f;
    const float shopH = 5.5f;  // single story, so open interiors light cleanly
    city.buildings_ = {
        // Named shops: {id,name, minX,minZ,maxX,maxZ, height, facade, enterable, door}
        {"bakery", "Marge's Bakery", -84.f, -64.f, -56.f, -40.f, shopH, 0, true, DoorSide::PosZ},
        {"police", "City Police Station", -20.f, -80.f, 20.f, -40.f, shopH, 1, true, DoorSide::PosZ},
        {"coffee", "Bean There Coffee", 44.f, -62.f, 72.f, -40.f, shopH, 2, true, DoorSide::PosZ},
        {"library", "City Library", -80.f, -16.f, -40.f, 16.f, shopH, 3, true, DoorSide::PosX},
        {"hardware", "Jensen Hardware", 40.f, -14.f, 72.f, 14.f, shopH, 4, true, DoorSide::NegX},
        {"guitar", "Benny's Guitars", -16.f, 28.f, 16.f, 46.f, shopH, 11, true, DoorSide::NegZ},
        {"grocery", "Plaza Market", -80.f, 20.f, -52.f, 40.f, shopH, 12, true, DoorSide::PosX},

        // The holding cell inside the police station: solid walls on three
        // sides, steel bars across the front (the player is teleported in and
        // held by the movement lock, then released at the door).
        {"cell_wall_back", "", 6.f, -78.f, 18.f, -77.4f, 3.0f, 13},
        {"cell_wall_left", "", 6.f, -78.f, 6.6f, -66.f, 3.0f, 13},
        {"cell_wall_right", "", 17.4f, -78.f, 18.f, -66.f, 3.0f, 13},
        {"cell_bars", "", 6.f, -66.6f, 18.f, -66.f, 3.0f, 14},

        // Plaza furniture and street props (low solid obstacles).
        {"cart", "Gus's Hot Dogs", -3.f, -2.f, 3.f, 2.f, 3.f, 5},
        {"bank", "City Bank", 6.f, 12.f, 18.f, 22.f, 6.f, 1},  // walk up + Interact for cash
        {"taxi_cab", "", 26.f, -6.f, 34.f, -1.f, 1.6f, 6},
        {"fountain", "", 60.f, 56.f, 68.f, 64.f, 1.5f, 7},
        {"bench", "", 66.f, 70.f, 70.f, 71.f, 0.6f, 8},

        // Unnamed solid filler so every block reads as a dense city.
        {"apt_a", "", -88.f, -88.f, -68.f, -70.f, 22.f, 9},
        {"apt_b", "", -60.f, -88.f, -40.f, -72.f, 18.f, 9},
        {"office_a", "", 44.f, -88.f, 88.f, -66.f, 26.f, 10},
        {"apts_c", "", -88.f, 44.f, -44.f, 84.f, 24.f, 9},
        {"tower", "", -24.f, 48.f, 24.f, 88.f, 30.f, 10},

        // Outer ring (the 5x5 grid's perimeter blocks): solid high-rise filler
        // framing the core, with 16-unit avenues (x/z = +-96) between it and
        // the inner ring. Footprints sit at +-104..+-148 on the edge axis so
        // those avenues stay clear for traffic.
        {"hr_nw", "", -148.f, 104.f, -104.f, 148.f, 30.f, 10},
        {"hr_n1", "", -84.f, 104.f, -44.f, 148.f, 20.f, 9},
        {"hr_n0", "", -20.f, 104.f, 20.f, 148.f, 26.f, 9},
        {"hr_n2", "", 44.f, 104.f, 84.f, 148.f, 18.f, 9},
        {"hr_ne", "", 104.f, 104.f, 148.f, 148.f, 32.f, 10},
        {"hr_sw", "", -148.f, -148.f, -104.f, -104.f, 28.f, 10},
        {"hr_s1", "", -84.f, -148.f, -44.f, -104.f, 18.f, 9},
        {"hr_s0", "", -20.f, -148.f, 20.f, -104.f, 22.f, 9},
        {"hr_s2", "", 44.f, -148.f, 84.f, -104.f, 24.f, 9},
        {"hr_se", "", 104.f, -148.f, 148.f, -104.f, 34.f, 10},
        {"hr_w1", "", -148.f, -84.f, -104.f, -44.f, 24.f, 9},
        {"hr_w0", "", -148.f, -20.f, -104.f, 20.f, 28.f, 10},
        {"hr_w2", "", -148.f, 44.f, -104.f, 84.f, 18.f, 9},
        {"hr_e1", "", 104.f, -84.f, 148.f, -44.f, 22.f, 9},
        {"hr_e0", "", 104.f, -20.f, 148.f, 20.f, 26.f, 10},
        {"hr_e2", "", 104.f, 44.f, 148.f, 84.f, 20.f, 9},
    };
    city.rebuildCollision();
    return city;
}

void City::rebuildCollision() {
    collisionBoxes_.clear();
    for (const auto& b : buildings_) {
        if (b.enterable) {
            for (const AABB& wall : buildingWallSegments(b)) collisionBoxes_.push_back(wall);
        } else {
            collisionBoxes_.push_back({b.minX, b.minZ, b.maxX, b.maxZ});
        }
    }
}

const Building* City::findBuilding(const std::string& id) const {
    for (const auto& b : buildings_) {
        if (b.id == id) return &b;
    }
    return nullptr;
}

bool City::circleIntersectsAny(float x, float z, float radius) const {
    const float r2 = radius * radius;
    for (const auto& box : collisionBoxes_) {
        if (distSqToAABB(box, x, z) < r2) return true;
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
