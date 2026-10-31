#pragma once

#include <string>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// Named regions of downtown (plan: clue-like-zones).
//
// A murder mystery has to answer "who was in the bakery between 14:00 and
// 15:00". That needs named places the code can reason about, which the game
// has never had — Building carries a `name` that has never reached the screen,
// and nothing else names anywhere.
//
// Downtown is already a 3x3 block grid: blocks centred at -64/0/64 on each
// axis, each spanning +-24, with 16-unit streets between them. That partitions
// into NINE named zones, the same count as Clue's nine rooms, plus the street
// network. The bounds below are derived from the same constants
// City::makeDowntown uses, so the partition cannot drift from the map.
//
// Note these are OUTDOOR regions, not rooms. Buildings are solid axis-aligned
// boxes with no interiors anywhere in src/ — enterable buildings live only in
// the unported SFML branches. That does not matter here: a zone is a named
// region with a boundary, and whether that boundary is a wall with a door or
// the kerb of a city block changes the feel, not the query. When interiors do
// land, a room becomes another zone with a smaller footprint and one entrance,
// and nothing above this layer changes.

struct ZoneDef {
    std::string id;    // stable key, e.g. "bakery_block" — used in fact subjects
    std::string name;  // player-facing label, e.g. "Bakery Corner"
    // Half-open bounds on the XZ plane: min <= p < max, the same convention
    // activeScheduleIndex uses for hour ranges. A point on a boundary belongs
    // to exactly one zone, so standing on a kerb cannot make an agent flicker
    // between two of them every frame.
    float minX = 0.f;
    float minZ = 0.f;
    float maxX = 0.f;
    float maxZ = 0.f;
};

// The zone id for anything not inside a block: the street network, the plaza
// approaches, and everything beyond the world bounds.
//
// This is a real zone rather than an empty string or a nullopt. "Nobody was
// anywhere" is not a useful answer to an alibi question, and an optional would
// push that branch onto every caller.
extern const char* const kStreetsZoneId;

// The authored partition, in a stable order. Nine blocks; `streets` is implied
// rather than listed, because it is defined as the complement of the others.
const std::vector<ZoneDef>& zonesForDowntown();

// The zone containing (x, z), or kStreetsZoneId when the point is between
// blocks or outside the world. Never returns empty.
const std::string& zoneAt(float x, float z);

// The player-facing name for a zone id, or the id itself when unknown —
// captions and journal entries read better as "Bakery Corner" than
// "bakery_block".
const std::string& zoneName(const std::string& zoneId);

// The centre of a zone on the XZ plane, y = 0. Returns the origin for an
// unknown id, including the streets, which have no bounds to average.
//
// Exists because Mystery.cpp already keeps a private findZone and the win
// montage needs the same lookup to aim a camera at a clue's location. A third
// private copy is how three of them start disagreeing.
Vec3 zoneCentre(const std::string& zoneId);

}  // namespace llm_npc
