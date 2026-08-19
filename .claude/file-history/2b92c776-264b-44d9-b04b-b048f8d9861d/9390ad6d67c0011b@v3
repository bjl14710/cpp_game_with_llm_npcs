// Sandbox map editor core (plan: sandbox-map-editor). The two remaining
// skipped stubs land with their steps (#110 seam, #113 NPC spawning).
#include <string>

#include "LlmClient.hpp"
#include "Math.hpp"
#include "SandboxMap.hpp"
#include "World.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A small valid map used across cases: one shop, one prop beside it, and
// two NPCs standing clear of both.
SandboxMap sampleMap() {
    SandboxMap map;
    map.name = "test-square";
    map.pieces.push_back({"shop_bakery", -4, -4});   // spans [-32,0]x[-32,-8]
    map.pieces.push_back({"bench", 2, 2});           // spans [16,24]x[16,24]
    map.npcs.push_back({"persona:baker", 8.f, 8.f, 180.f});
    map.npcs.push_back({"character:custom_123", -8.f, 8.f, 0.f});
    return map;
}

}  // namespace

TEST_CASE("piece catalog rows are well-formed") {
    CHECK(pieceCatalog().size() >= 20);
    for (const PieceDef& piece : pieceCatalog()) {
        CAPTURE(piece.id);
        CHECK_FALSE(piece.id.empty());
        CHECK_FALSE(piece.label.empty());
        CHECK_FALSE(piece.assetId.empty());
        CHECK(piece.tilesW > 0);
        CHECK(piece.tilesD > 0);
        if (piece.solid) CHECK(piece.height > 0.f);
        CHECK(piece.pack == "core");
        CHECK(findPiece(piece.id) == &piece);
    }
    CHECK(findPiece("no_such_piece") == nullptr);
}

TEST_CASE("map JSON round-trips byte-stable") {
    const SandboxMap map = sampleMap();
    const std::string once = map.toJson();
    SandboxMap parsed;
    REQUIRE(SandboxMap::fromJson(once, parsed));
    CHECK(parsed.name == map.name);
    REQUIRE(parsed.pieces.size() == map.pieces.size());
    CHECK(parsed.pieces[0].pieceId == "shop_bakery");
    CHECK(parsed.pieces[0].tileX == -4);
    REQUIRE(parsed.npcs.size() == 2);
    CHECK(parsed.npcs[0].source == "persona:baker");
    CHECK(parsed.npcs[1].facingDeg == doctest::Approx(0.f));
    // Byte-stable: the second render is identical text.
    CHECK(parsed.toJson() == once);

    SandboxMap rejected;
    CHECK_FALSE(SandboxMap::fromJson("not json at all", rejected));
    CHECK_FALSE(SandboxMap::fromJson("{}", rejected));
    // Newer versions are refused, never silently field-dropped.
    CHECK_FALSE(SandboxMap::fromJson(
        R"({"version": 99, "name": "x", "pieces": [], "npcs": []})", rejected));
    // Fractional tile coordinates are malformed (off-grid is impossible
    // by construction — the format only admits integers).
    CHECK_FALSE(SandboxMap::fromJson(
        R"({"version": 1, "name": "x", "npcs": [],
            "pieces": [{"piece": "bench", "x": 1.5, "z": 0}]})",
        rejected));
}

TEST_CASE("validateMap names every failure class") {
    CHECK(validateMap(sampleMap()).empty());

    SandboxMap bad = sampleMap();
    bad.pieces.push_back({"shp_bakery", 0, 0});                  // unknown id
    bad.pieces.push_back({"tower", 12, 12});                     // out of bounds
    bad.pieces.push_back({"shop_bakery", -4, -4});               // overlaps [0]
    bad.npcs.push_back({"persona:baker", -16.f, -16.f, 0.f});    // inside bakery
    bad.npcs.push_back({"somebody", 0.f, 40.f, 0.f});            // bad source
    bad.npcs.push_back({"persona:baker", 500.f, 0.f, 0.f});      // out of bounds

    const auto errors = validateMap(bad);
    REQUIRE(errors.size() == 6);
    auto has = [&](const std::string& whereBit, const std::string& reasonBit) {
        for (const MapError& e : errors) {
            if (e.where.find(whereBit) != std::string::npos &&
                e.reason.find(reasonBit) != std::string::npos) {
                return true;
            }
        }
        return false;
    };
    CHECK(has("pieces[2]", "unknown piece id"));
    CHECK(has("pieces[3]", "out of bounds"));
    CHECK(has("pieces[4]", "overlaps pieces[0]"));
    CHECK(has("npcs[2]", "inside solid pieces[0]"));
    CHECK(has("npcs[3]", "source must be"));
    CHECK(has("npcs[4]", "out of bounds"));
}

TEST_CASE("buildCity compiles pieces to native collision") {
    const SandboxMap map = sampleMap();
    const City city = buildCity(map);
    CHECK_FALSE(city.hasStreets());

    // Exact AABBs from tile footprints, instance-suffixed asset ids.
    REQUIRE(city.buildings().size() == 2);
    const Building& bakery = city.buildings()[0];
    CHECK(bakery.id == "bakery#0");
    CHECK(bakery.minX == doctest::Approx(-32.f));
    CHECK(bakery.minZ == doctest::Approx(-32.f));
    CHECK(bakery.maxX == doctest::Approx(0.f));
    CHECK(bakery.maxZ == doctest::Approx(-8.f));
    CHECK(bakery.height == doctest::Approx(12.f));
    CHECK(city.buildings()[1].id == "bench#1");

    // A placed solid blocks a walking circle exactly like a native
    // building: aim from just south of the bakery INTO it.
    const Vec3 blocked = city.resolveMovement(Vec3{-16.f, 0.f, -7.f},
                                              Vec3{-16.f, 0.f, -9.f}, 0.45f);
    CHECK(blocked.z == doctest::Approx(-7.f));
    // Open ground is unaffected.
    const Vec3 open = city.resolveMovement(Vec3{40.f, 0.f, 40.f},
                                           Vec3{40.f, 0.f, 42.f}, 0.45f);
    CHECK(open.z == doctest::Approx(42.f));
}

TEST_CASE("town <-> sandbox <-> town round-trip via loadCity") {
    // Step 2 (#110). World swaps contents in place (it cannot be
    // reassigned — NPCs hold the client reference); the world bus
    // deliberately survives the swap. Main-side array rebuilding is
    // covered by the extracted resetNpcSideArrays (smoke-verified).
    LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    World world(City::makeDowntown());
    Persona p;
    p.name = "Roundtrip Rita";
    Npc npc(p, client);
    npc.setPlacement(Vec3{0.f, 0.f, 20.f}, 0.f, "plaza");
    world.addNpc(std::move(npc));
    REQUIRE(world.npcs().size() == 1);
    CHECK(world.city().hasStreets());
    const std::size_t townBuildings = world.city().buildings().size();
    world.state().setTimeOfDayHours(15.0);  // bus state must survive swaps

    // Into the sandbox map: buildings swap, inhabitants clear.
    world.loadCity(buildCity(sampleMap()));
    CHECK_FALSE(world.city().hasStreets());
    CHECK(world.city().buildings().size() == 2);
    CHECK(world.npcs().empty());
    CHECK(world.state().timeOfDayHours() == doctest::Approx(15.0));
    // Collision is live on the new city (into the placed bakery).
    const Vec3 blocked = world.city().resolveMovement(
        Vec3{-16.f, 0.f, -7.f}, Vec3{-16.f, 0.f, -9.f}, 0.45f);
    CHECK(blocked.z == doctest::Approx(-7.f));

    // And back to town: geometry, streets, and collision all restore.
    world.loadCity(City::makeDowntown());
    CHECK(world.city().hasStreets());
    CHECK(world.city().buildings().size() == townBuildings);
    CHECK(world.city().findBuilding("bakery") != nullptr);
}

TEST_CASE("placed NPCs spawn with stored persona and look, schedules off" *
          doctest::skip()) {
    // Step 5 (#113). TODO(sandbox): persona:/character: resolution,
    // placement override, schedule suppression, deleted-id skip.
}
