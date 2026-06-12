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
