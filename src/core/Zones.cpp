#include "Zones.hpp"

namespace llm_npc {
namespace {

// The same constants City::makeDowntown lays the map out with: blocks centred
// at -64/0/64 on each axis, each spanning +-24, leaving 16-unit streets. Named
// here rather than repeated as literals so a map edit has one obvious place to
// look — and tests/test_zones.cpp asserts every named building still falls in
// the zone this table claims for it.
constexpr float kBlockHalf = 24.f;
constexpr float kWest = -64.f, kMid = 0.f, kEast = 64.f;

ZoneDef block(const char* id, const char* name, float cx, float cz) {
    return ZoneDef{id, name, cx - kBlockHalf, cz - kBlockHalf,
                   cx + kBlockHalf, cz + kBlockHalf};
}

}  // namespace

const char* const kStreetsZoneId = "streets";

const std::vector<ZoneDef>& zonesForDowntown() {
    // Real data, not a stub: this is the partition itself, and having it
    // present lets test_zones.cpp pin it against City::makeDowntown from the
    // first commit rather than after the lookup is written.
    static const std::vector<ZoneDef> kZones = {
        block("bakery_block", "Bakery Corner", kWest, -64.f),
        block("police_block", "Station Square", kMid, -64.f),
        block("coffee_block", "Coffee Row", kEast, -64.f),
        block("library_block", "Library Steps", kWest, kMid),
        block("plaza", "The Plaza", kMid, kMid),
        block("hardware_block", "Hardware Yard", kEast, kMid),
        block("west_block", "West Terrace", kWest, kEast),
        block("north_block", "North Walk", kMid, kEast),
        block("park", "The Park", kEast, kEast),
    };
    return kZones;
}

const std::string& zoneAt(float, float) {
    // TODO(zones step 1): scan zonesForDowntown() for a half-open containment
    // match (min <= p < max) and fall back to streets.
    //
    // Returning streets unconditionally is already the correct answer for a
    // caller standing between blocks, and it is the safe default: an
    // unimplemented lookup reports "somewhere in town", never a wrong place.
    static const std::string kStreets = kStreetsZoneId;
    return kStreets;
}

const std::string& zoneName(const std::string& zoneId) {
    // TODO(zones step 1): resolve the display name from zonesForDowntown().
    return zoneId;
}

}  // namespace llm_npc
