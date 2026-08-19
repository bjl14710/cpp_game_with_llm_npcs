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

TEST_CASE("every visible street obstacle has real collision") {
    // Street furniture, parked cars, and alley props are authored in City
    // (the renderer draws them from this list), so their visual footprints
    // and colliders can never diverge — walking through them is impossible
    // by data. The probe point is inside each obstacle's AABB.
    const llm_npc::City city = llm_npc::City::makeDowntown();
    const struct { const char* id; float x, z; } obstacles[] = {
        {"trafficlight_a", -23.f, -23.f}, {"trafficlight_b", -23.f, 41.f},
        {"trafficlight_c", 41.f, -23.f},  {"trafficlight_d", 41.f, 41.f},
        {"bush_a", 48.f, 48.f},           {"bush_b", 80.f, 52.f},
        {"bush_c", 52.f, 80.f},           {"bush_d", 76.f, 76.f},
        {"police_car", -25.5f, -61.f},    {"hatchback_b", 25.5f, -67.f},
        {"sedan_a", 38.5f, -6.f},         {"sedan_b", 38.5f, 63.f},
        {"hatchback_a", -38.5f, 61.f},    {"dumpster_a", -65.f, -85.f},
        {"trash_a", -61.8f, -86.7f},      {"trash_b", 45.6f, -64.8f},
    };
    for (const auto& o : obstacles) {
        CAPTURE(o.id);
        CHECK(city.findBuilding(o.id) != nullptr);
        CHECK(city.circleIntersectsAny(o.x, o.z, 0.3f));
    }
}

TEST_CASE("parked cars leave persona home spots and crossings clear") {
    // Set dressing must not trap an NPC: every persona home position from
    // personas/ (hardware Hal at (36,0) is the closest call) keeps a clear
    // walking circle, and the zebra-crossing tile centers stay walkable.
    const llm_npc::City city = llm_npc::City::makeDowntown();
    const struct { const char* who; float x, z; } spots[] = {
        {"barista", 58.f, -36.f}, {"hardware", 36.f, 0.f},
        {"librarian", -36.f, 0.f}, {"cop", 0.f, -36.f},
        {"baker", -70.f, -36.f}, {"taxi", 30.f, 8.f},
        {"tourist", -8.f, 10.f}, {"hotdog", 6.f, 0.f},
        {"musician", 52.f, 52.f}, {"teacher", 68.f, 73.f},
        {"crossing_w", -16.f, -32.f}, {"crossing_e", 16.f, -32.f},
        {"crossing_n", 32.f, 48.f}, {"crossing_s", 32.f, -48.f},
    };
    for (const auto& s : spots) {
        CAPTURE(s.who);
        CHECK_FALSE(city.circleIntersectsAny(s.x, s.z, 0.5f));
    }
}

// --- Height-aware collision + jumping support (feature: punch-and-jump) ---

TEST_CASE("a building only blocks while its top is above the feet") {
    City city = City::makeDowntown();
    const Building* bench = city.findBuilding("bench");
    REQUIRE(bench != nullptr);
    const float cx = (bench->minX + bench->maxX) * 0.5f;
    const float cz = (bench->minZ + bench->maxZ) * 0.5f;

    // Grounded: the bench is solid, exactly as before.
    CHECK(city.circleIntersectsAny(cx, cz, 0.45f));
    CHECK(city.circleIntersectsAny(cx, cz, 0.45f, 0.f));
    // Feet above the bench top (0.6): it no longer blocks.
    CHECK_FALSE(city.circleIntersectsAny(cx, cz, 0.45f, bench->height + 0.05f));
    // A tall building still blocks an airborne mover at hop height.
    const Building* bakery = city.findBuilding("bakery");
    REQUIRE(bakery != nullptr);
    CHECK(city.circleIntersectsAny((bakery->minX + bakery->maxX) * 0.5f,
                                   (bakery->minZ + bakery->maxZ) * 0.5f, 0.45f,
                                   1.4f));
}

TEST_CASE("resolveMovement judges solidity at the mover's foot height") {
    City city = City::makeDowntown();
    const Building* bench = city.findBuilding("bench");
    REQUIRE(bench != nullptr);
    const float cx = (bench->minX + bench->maxX) * 0.5f;
    const float startZ = bench->minZ - 1.0f;
    // Aim INTO the bench footprint: resolveMovement judges the endpoint
    // (frames step ~0.1u, so swept tunneling is not a real-game case).
    const float intoZ = bench->minZ + 0.3f;

    // On the ground the bench is a wall: z movement is dropped.
    const Vec3 blocked = city.resolveMovement(Vec3{cx, 0.f, startZ},
                                              Vec3{cx, 0.f, intoZ}, 0.45f);
    CHECK(blocked.z == startZ);
    // Mid-hop (feet above the bench top) the same move passes.
    const Vec3 cleared = city.resolveMovement(Vec3{cx, 0.8f, startZ},
                                              Vec3{cx, 0.8f, intoZ}, 0.45f);
    CHECK(cleared.z == intoZ);
}

TEST_CASE("supportHeightAt reports what the feet can stand on") {
    City city = City::makeDowntown();
    const Building* bench = city.findBuilding("bench");
    REQUIRE(bench != nullptr);
    const float cx = (bench->minX + bench->maxX) * 0.5f;
    const float cz = (bench->minZ + bench->maxZ) * 0.5f;

    // Open ground supports at 0.
    CHECK(city.supportHeightAt(0.f, 20.f, 0.45f, 2.f) == 0.f);
    // Above the bench, the bench top is the support...
    CHECK(city.supportHeightAt(cx, cz, 0.45f, bench->height + 0.4f) ==
          doctest::Approx(bench->height));
    // ...but from below it is a wall, not a floor.
    CHECK(city.supportHeightAt(cx, cz, 0.45f, 0.2f) == 0.f);
}
