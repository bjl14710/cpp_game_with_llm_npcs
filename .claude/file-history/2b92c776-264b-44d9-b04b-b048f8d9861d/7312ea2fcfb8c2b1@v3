// Tests for City: building lookup, circle collision, and movement sliding.
#include "City.hpp"
#include "doctest.h"

using llm_npc::Building;
using llm_npc::City;
using llm_npc::Vec3;

namespace {

// A minimal city with one 10x10 building centered at the origin.
City makeOneBlockCity() {
    City city = City::makeDowntown();
    // makeDowntown is the real map; for geometric edge cases we rely on its
    // known "cart" obstacle at [-3,3]x[-2,2] and the world bounds.
    return city;
}

}  // namespace

TEST_CASE("makeDowntown contains the five named shops") {
    City city = City::makeDowntown();
    for (const char* id : {"bakery", "police", "coffee", "library", "hardware"}) {
        const Building* b = city.findBuilding(id);
        REQUIRE_MESSAGE(b != nullptr, id);
        CHECK_FALSE(b->name.empty());
        CHECK(b->minX < b->maxX);
        CHECK(b->minZ < b->maxZ);
        CHECK(b->height > 0.f);
    }
    CHECK(city.findBuilding("no_such_building") == nullptr);
}

TEST_CASE("the new shops exist and are enterable") {
    City city = City::makeDowntown();
    for (const char* id : {"guitar", "grocery"}) {
        const Building* b = city.findBuilding(id);
        REQUIRE_MESSAGE(b != nullptr, id);
        CHECK_FALSE(b->name.empty());
    }
    for (const char* id : {"bakery", "police", "coffee", "library", "hardware",
                           "guitar", "grocery"}) {
        const Building* b = city.findBuilding(id);
        REQUIRE_MESSAGE(b != nullptr, id);
        CHECK_MESSAGE(b->enterable, id);
    }
}

TEST_CASE("buildingWallSegments yields a split wall with a doorway gap") {
    Building b;
    b.minX = -10.f; b.maxX = 10.f; b.minZ = -10.f; b.maxZ = 10.f;
    b.enterable = true;
    b.doorSide = llm_npc::DoorSide::PosZ;
    const auto walls = llm_npc::buildingWallSegments(b);
    // Three solid walls plus the door wall split into two = five segments.
    CHECK(walls.size() == 5);
    // The doorway center sits on the +Z edge, at x = 0.
    const Vec3 d = llm_npc::buildingDoorCenter(b);
    CHECK(d.x == doctest::Approx(0.f));
    CHECK(d.z == doctest::Approx(10.f - llm_npc::kWallThickness * 0.5f));
}

TEST_CASE("an enterable shop is hollow: walls block, doorway and interior clear") {
    City city = City::makeDowntown();
    const Building* lib = city.findBuilding("library");  // door on +X edge (x=-40)
    REQUIRE(lib != nullptr);
    // Interior center is open space.
    CHECK_FALSE(city.circleIntersectsAny(-60.f, 0.f, 0.4f));
    // The north wall is solid.
    CHECK(city.circleIntersectsAny(-60.f, 15.7f, 0.4f));
    // The doorway gap on the east wall (z = 0) is open.
    CHECK_FALSE(city.circleIntersectsAny(-40.3f, 0.f, 0.3f));
    // The east wall beside the doorway (z = 8) is solid.
    CHECK(city.circleIntersectsAny(-40.3f, 8.f, 0.3f));
}

TEST_CASE("resolveMovement steps through a doorway but not through a wall") {
    City city = City::makeDowntown();
    const float r = 0.45f;
    // A step from just outside the library door into the doorway gap lands.
    const Vec3 in = city.resolveMovement({-39.5f, 0.f, 0.f}, {-40.5f, 0.f, 0.f}, r);
    CHECK(in.x == doctest::Approx(-40.5f));
    // A step into the solid north wall is rejected (z stays put).
    const Vec3 blocked = city.resolveMovement({-60.f, 0.f, 14.5f}, {-60.f, 0.f, 15.0f}, r);
    CHECK(blocked.z == doctest::Approx(14.5f));
}

TEST_CASE("the holding cell is enclosed: interior clear, bars block") {
    City city = City::makeDowntown();
    // The arrest teleport target inside the cell is open space...
    CHECK_FALSE(city.circleIntersectsAny(12.f, -72.f, 0.4f));
    // ...but the steel bars across the cell front are solid.
    CHECK(city.circleIntersectsAny(12.f, -66.3f, 0.2f));
    // The release spot just outside the station door is clear.
    CHECK_FALSE(city.circleIntersectsAny(0.f, -37.f, 0.45f));
}

TEST_CASE("the map expanded to a larger grid with outer high-rises") {
    City city = City::makeDowntown();
    CHECK(city.halfSize() > 150.f);
    // A sample of the outer-ring filler exists and stays solid (not enterable).
    for (const char* id : {"hr_nw", "hr_ne", "hr_sw", "hr_se", "hr_e0", "hr_w0"}) {
        const Building* b = city.findBuilding(id);
        REQUIRE_MESSAGE(b != nullptr, id);
        CHECK_FALSE(b->enterable);
        CHECK(b->height > 0.f);
    }
    // The core landmarks survived the expansion.
    CHECK(city.findBuilding("police") != nullptr);
    CHECK(city.findBuilding("cell_bars") != nullptr);
}

TEST_CASE("buildings stay inside world bounds") {
    City city = City::makeDowntown();
    for (const auto& b : city.buildings()) {
        CHECK(b.minX >= -city.halfSize());
        CHECK(b.maxX <= city.halfSize());
        CHECK(b.minZ >= -city.halfSize());
        CHECK(b.maxZ <= city.halfSize());
    }
}

TEST_CASE("circleIntersectsAny detects overlap and clear space") {
    City city = makeOneBlockCity();
    // Inside the hot-dog cart footprint.
    CHECK(city.circleIntersectsAny(0.f, 0.f, 0.6f));
    // Just outside the cart: x = 3 (cart edge) + 0.7 clearance, radius 0.6.
    CHECK_FALSE(city.circleIntersectsAny(3.7f, 0.f, 0.6f));
    // Touching via the radius: x = 3.4 means the 0.6 circle reaches 2.8 < 3.0.
    CHECK(city.circleIntersectsAny(3.5f, 0.f, 0.6f));
    // Open street, far from everything.
    CHECK_FALSE(city.circleIntersectsAny(0.f, 32.f, 0.6f));
}

TEST_CASE("resolveMovement allows free movement") {
    City city = makeOneBlockCity();
    const Vec3 from{0.f, 0.f, 32.f};
    const Vec3 to{1.f, 0.f, 33.f};
    const Vec3 res = city.resolveMovement(from, to, 0.6f);
    CHECK(res.x == doctest::Approx(1.f));
    CHECK(res.z == doctest::Approx(33.f));
}

TEST_CASE("resolveMovement blocks walking straight into a wall") {
    City city = makeOneBlockCity();
    // Approach the cart's +x face head-on.
    const Vec3 from{5.f, 0.f, 0.f};
    const Vec3 to{3.0f, 0.f, 0.f};  // would put the 0.6 circle inside the cart
    const Vec3 res = city.resolveMovement(from, to, 0.6f);
    CHECK(res.x == doctest::Approx(5.f));  // x move rejected
    CHECK(res.z == doctest::Approx(0.f));
}

TEST_CASE("resolveMovement slides along a wall on diagonal input") {
    City city = makeOneBlockCity();
    // Move diagonally into the cart's +x face: x is blocked, z should slide.
    const Vec3 from{4.f, 0.f, 0.f};
    const Vec3 to{2.f, 0.f, 1.5f};
    const Vec3 res = city.resolveMovement(from, to, 0.6f);
    CHECK(res.x == doctest::Approx(4.f));   // blocked axis dropped
    CHECK(res.z == doctest::Approx(1.5f));  // free axis applied
}

TEST_CASE("resolveMovement clamps to world bounds") {
    City city = makeOneBlockCity();
    const float h = city.halfSize();
    const Vec3 from{0.f, 0.f, 100.f};
    const Vec3 to{0.f, 0.f, h + 50.f};
    const Vec3 res = city.resolveMovement(from, to, 0.6f);
    CHECK(res.z == doctest::Approx(h - 0.6f));
}
