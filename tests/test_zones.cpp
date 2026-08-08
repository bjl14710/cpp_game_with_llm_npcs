// Named downtown zones (plan: clue-like-zones, step 1).
//
// One case is NOT skipped: the partition-versus-map check below tests only
// zonesForDowntown() and City::makeDowntown, both of which already exist. It
// is the assertion that stops a later map edit from silently mis-filing an
// alibi, so it earns its place from the first commit.
#include <string>

#include "City.hpp"
#include "Zones.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Where each named building is expected to live. Kept as data so a failure
// names the building AND the zone rather than just a line number.
struct Expectation {
    const char* buildingId;
    const char* zoneId;
};

constexpr Expectation kNamedBuildings[] = {
    {"bakery", "bakery_block"},
    {"police", "police_block"},
    {"coffee", "coffee_block"},
    {"library", "library_block"},
    {"hardware", "hardware_block"},
    {"cart", "plaza"},
};

const ZoneDef* findZone(const std::string& id) {
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (zone.id == id) return &zone;
    }
    return nullptr;
}

bool contains(const ZoneDef& zone, float x, float z) {
    return x >= zone.minX && x < zone.maxX && z >= zone.minZ && z < zone.maxZ;
}

}  // namespace

TEST_CASE("every named building sits inside the zone the partition claims") {
    // NOT skipped. The zone table is derived by hand from the same block
    // constants City::makeDowntown uses, and nothing but this stops the two
    // drifting apart. If someone moves the bakery, an alibi about the bakery
    // starts pointing at the wrong place — silently, and only in a mystery
    // nobody can solve.
    const City city = City::makeDowntown();
    REQUIRE(zonesForDowntown().size() == 9);

    for (const Expectation& expected : kNamedBuildings) {
        const Building* building = city.findBuilding(expected.buildingId);
        REQUIRE_MESSAGE(building != nullptr, expected.buildingId);
        CHECK_FALSE_MESSAGE(building->name.empty(),
                            (std::string(expected.buildingId) + " lost its name"));

        const ZoneDef* zone = findZone(expected.zoneId);
        REQUIRE_MESSAGE(zone != nullptr, expected.zoneId);

        const float cx = (building->minX + building->maxX) * 0.5f;
        const float cz = (building->minZ + building->maxZ) * 0.5f;
        CHECK_MESSAGE(contains(*zone, cx, cz),
                      (std::string(expected.buildingId) + " is not inside " +
                       expected.zoneId));
    }
}

TEST_CASE("zone ids and names are unique and non-empty") {
    // A duplicate id would make two places indistinguishable in an alibi.
    for (const ZoneDef& a : zonesForDowntown()) {
        CHECK_FALSE(a.id.empty());
        CHECK_FALSE(a.name.empty());
        int matches = 0;
        for (const ZoneDef& b : zonesForDowntown()) {
            if (a.id == b.id) ++matches;
        }
        CHECK_MESSAGE(matches == 1, (a.id + " is not unique"));
    }
}

TEST_CASE("zones do not overlap" * doctest::skip()) {
    // Half-open bounds mean adjacent blocks share an edge without overlapping.
    // A point in two zones would make zoneAt's answer order-dependent.
    const auto& zones = zonesForDowntown();
    for (std::size_t i = 0; i < zones.size(); ++i) {
        for (std::size_t j = i + 1; j < zones.size(); ++j) {
            const bool xOverlap = zones[i].minX < zones[j].maxX &&
                                  zones[j].minX < zones[i].maxX;
            const bool zOverlap = zones[i].minZ < zones[j].maxZ &&
                                  zones[j].minZ < zones[i].maxZ;
            // Assigned to a bool first: doctest cannot decompose `&&` inside
            // an assertion ("Expression Too Complex Please Rewrite As Binary
            // Comparison").
            const bool overlaps = xOverlap && zOverlap;
            CHECK_FALSE_MESSAGE(overlaps,
                                (zones[i].id + " overlaps " + zones[j].id));
        }
    }
}

TEST_CASE("each block centre resolves to its own zone" * doctest::skip()) {
    CHECK(zoneAt(-64.f, -64.f) == "bakery_block");
    CHECK(zoneAt(0.f, -64.f) == "police_block");
    CHECK(zoneAt(64.f, -64.f) == "coffee_block");
    CHECK(zoneAt(-64.f, 0.f) == "library_block");
    CHECK(zoneAt(0.f, 0.f) == "plaza");
    CHECK(zoneAt(64.f, 0.f) == "hardware_block");
    CHECK(zoneAt(-64.f, 64.f) == "west_block");
    CHECK(zoneAt(0.f, 64.f) == "north_block");
    CHECK(zoneAt(64.f, 64.f) == "park");
}

TEST_CASE("a point between blocks is on the streets" * doctest::skip()) {
    // Blocks span +-24 around their centre, so +-32 is mid-street.
    CHECK(zoneAt(-32.f, 0.f) == kStreetsZoneId);
    CHECK(zoneAt(0.f, 32.f) == kStreetsZoneId);
    CHECK(zoneAt(32.f, 32.f) == kStreetsZoneId);
}

TEST_CASE("a point outside the world is on the streets, not a crash" *
          doctest::skip()) {
    // halfSize is 110. Out of bounds must be an answer, never empty.
    CHECK(zoneAt(500.f, 500.f) == kStreetsZoneId);
    CHECK(zoneAt(-500.f, -500.f) == kStreetsZoneId);
    CHECK_FALSE(zoneAt(500.f, 500.f).empty());
}

TEST_CASE("zone bounds are half-open, so a boundary belongs to one zone" *
          doctest::skip()) {
    // The plaza spans [-24, 24). Its low edge is inside it; its high edge is
    // not. Without this an agent standing on a kerb flickers between zones
    // every frame and the log fills with phantom transitions.
    CHECK(zoneAt(-24.f, 0.f) == "plaza");
    CHECK(zoneAt(24.f, 0.f) == kStreetsZoneId);
}

TEST_CASE("zoneName resolves a display label and falls back to the id" *
          doctest::skip()) {
    CHECK(zoneName("bakery_block") == "Bakery Corner");
    CHECK(zoneName("plaza") == "The Plaza");
    CHECK(zoneName("no_such_zone") == "no_such_zone");
}
