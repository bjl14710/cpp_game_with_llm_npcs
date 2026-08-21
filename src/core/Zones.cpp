#include "Zones.hpp"

#include <cmath>

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

const std::string& zoneAt(float x, float z) {
    // Half-open on both axes (min <= p < max), the same convention
    // activeScheduleIndex uses for hour ranges. Adjacent blocks share an edge
    // without overlapping, so a point on a kerb belongs to exactly one zone —
    // without that, an agent standing on a boundary flickers between two zones
    // every frame and fills the log with phantom transitions.
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (x >= zone.minX && x < zone.maxX && z >= zone.minZ && z < zone.maxZ) {
            return zone.id;
        }
    }
    // economy: a linear scan over nine boxes. Nine axis-aligned rectangles is
    // not a search problem, so there is deliberately no grid hash or spatial
    // index here.
    static const std::string kStreets = kStreetsZoneId;
    return kStreets;
}

const std::string& zoneName(const std::string& zoneId) {
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (zone.id == zoneId) return zone.name;
    }
    // Falling back to the id keeps captions and journal lines readable when a
    // zone is renamed or removed, rather than rendering an empty label.
    return zoneId;
}

Vec3 zoneCentre(const std::string& zoneId) {
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (zone.id != zoneId) continue;
        return Vec3{(zone.minX + zone.maxX) * 0.5f, 0.f,
                    (zone.minZ + zone.maxZ) * 0.5f};
    }
    // Unknown id, or the streets — which span everything not in a block and
    // so have no meaningful centre. The origin is the plaza's doorstep, which
    // is at least somewhere a camera can stand.
    return Vec3{};
}

Vec3 plazaGatherSpot(int index, int count) {
    // Clears the cart (half-extent 3 on x, 2 on z) with room to stand, and
    // stays well inside the plaza block, which spans +-24 around its centre.
    constexpr float kGatherRadius = 8.f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    const Vec3 centre = zoneCentre("plaza");
    const int slots = count > 1 ? count : 1;
    const int slot = index > 0 ? index : 0;
    const float angle = kTwoPi * (static_cast<float>(slot % slots) /
                                  static_cast<float>(slots));
    return Vec3{centre.x + kGatherRadius * std::sin(angle), centre.y,
                centre.z + kGatherRadius * std::cos(angle)};
}

}  // namespace llm_npc
