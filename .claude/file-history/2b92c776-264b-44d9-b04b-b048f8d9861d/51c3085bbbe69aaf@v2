#include "SandboxMap.hpp"

#include <cmath>

#include "Math.hpp"
#include "json.hpp"

namespace llm_npc {

namespace {

// Sandbox maps use the same world extent as the downtown so movement
// clamping and camera behavior carry over unchanged.
constexpr float kMapHalfSize = 110.f;

// The player/NPC collision radius the NPC-in-solid check assumes (matches
// kPlayerRadius in main and the NPC spawn checks).
constexpr float kBodyRadius = 0.45f;

// World-space AABB of a placed piece (tile anchor = SW corner).
struct Footprint {
    float minX, minZ, maxX, maxZ;
};

Footprint footprintOf(const PlacedPiece& placed, const PieceDef& piece) {
    const float minX = static_cast<float>(placed.tileX) * kMapTileUnits;
    const float minZ = static_cast<float>(placed.tileZ) * kMapTileUnits;
    return {minX, minZ, minX + static_cast<float>(piece.tilesW) * kMapTileUnits,
            minZ + static_cast<float>(piece.tilesD) * kMapTileUnits};
}

bool overlaps(const Footprint& a, const Footprint& b) {
    // Strict interior overlap — sharing an edge is legal (pieces tile).
    return a.minX < b.maxX && b.minX < a.maxX && a.minZ < b.maxZ && b.minZ < a.maxZ;
}

// Distance-squared from a point to a footprint (clamp math, mirrors
// City's collision helper).
float distSqToFootprint(const Footprint& f, float x, float z) {
    const float cx = clampf(x, f.minX, f.maxX);
    const float cz = clampf(z, f.minZ, f.maxZ);
    const float dx = x - cx;
    const float dz = z - cz;
    return dx * dx + dz * dz;
}

}  // namespace

std::string SandboxMap::toJson() const {
    nlohmann::json j;
    j["version"] = kMapFormatVersion;
    j["name"] = name;
    j["tile"] = static_cast<int>(kMapTileUnits);
    j["pieces"] = nlohmann::json::array();
    for (const PlacedPiece& p : pieces) {
        j["pieces"].push_back({{"piece", p.pieceId}, {"x", p.tileX}, {"z", p.tileZ}});
    }
    j["npcs"] = nlohmann::json::array();
    for (const PlacedNpc& n : npcs) {
        j["npcs"].push_back({{"source", n.source},
                             {"x", n.x},
                             {"z", n.z},
                             {"facing", n.facingDeg}});
    }
    return j.dump(2);
}

bool SandboxMap::fromJson(const std::string& json, SandboxMap& out) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) return false;
    // Version gate: older loaders must refuse newer documents rather than
    // silently dropping fields they don't know about.
    if (!j.contains("version") || !j["version"].is_number_integer() ||
        j["version"].get<int>() > kMapFormatVersion) {
        return false;
    }
    if (!j.contains("name") || !j["name"].is_string()) return false;
    if (!j.contains("pieces") || !j["pieces"].is_array()) return false;
    if (!j.contains("npcs") || !j["npcs"].is_array()) return false;

    SandboxMap map;
    map.name = j["name"].get<std::string>();
    for (const auto& e : j["pieces"]) {
        // Tile coordinates are STRICT integers — a fractional tile is a
        // malformed document (off-grid is impossible by construction).
        if (!e.is_object() || !e.contains("piece") || !e["piece"].is_string() ||
            !e.contains("x") || !e["x"].is_number_integer() ||
            !e.contains("z") || !e["z"].is_number_integer()) {
            return false;
        }
        map.pieces.push_back({e["piece"].get<std::string>(), e["x"].get<int>(),
                              e["z"].get<int>()});
    }
    for (const auto& e : j["npcs"]) {
        if (!e.is_object() || !e.contains("source") || !e["source"].is_string() ||
            !e.contains("x") || !e["x"].is_number() ||
            !e.contains("z") || !e["z"].is_number() ||
            !e.contains("facing") || !e["facing"].is_number()) {
            return false;
        }
        map.npcs.push_back({e["source"].get<std::string>(), e["x"].get<float>(),
                            e["z"].get<float>(), e["facing"].get<float>()});
    }
    out = std::move(map);
    return true;
}

std::vector<MapError> validateMap(const SandboxMap& map) {
    std::vector<MapError> errors;

    // Pieces: known ids, in bounds, no solid-footprint interior overlap.
    std::vector<std::pair<Footprint, bool>> footprints;  // {aabb, solid}
    for (std::size_t i = 0; i < map.pieces.size(); ++i) {
        const std::string where = "pieces[" + std::to_string(i) + "]";
        const PieceDef* piece = findPiece(map.pieces[i].pieceId);
        if (!piece) {
            errors.push_back({where, "unknown piece id '" + map.pieces[i].pieceId +
                                         "' (see pieceCatalog for valid ids)"});
            continue;
        }
        const Footprint f = footprintOf(map.pieces[i], *piece);
        if (f.minX < -kMapHalfSize || f.maxX > kMapHalfSize ||
            f.minZ < -kMapHalfSize || f.maxZ > kMapHalfSize) {
            errors.push_back({where, "footprint out of bounds (world is +-" +
                                         std::to_string(static_cast<int>(kMapHalfSize)) +
                                         " units)"});
            continue;
        }
        if (piece->solid) {
            for (std::size_t k = 0; k < footprints.size(); ++k) {
                if (footprints[k].second && overlaps(f, footprints[k].first)) {
                    errors.push_back({where, "solid footprint overlaps pieces[" +
                                                 std::to_string(k) + "]"});
                    break;
                }
            }
        }
        footprints.push_back({f, piece->solid});
    }

    // NPCs: well-formed source, in bounds, not inside a solid piece.
    for (std::size_t i = 0; i < map.npcs.size(); ++i) {
        const std::string where = "npcs[" + std::to_string(i) + "]";
        const PlacedNpc& npc = map.npcs[i];
        const bool persona = npc.source.rfind("persona:", 0) == 0;
        const bool character = npc.source.rfind("character:", 0) == 0;
        if ((!persona && !character) || npc.source.find(':') + 1 >= npc.source.size()) {
            errors.push_back({where, "source must be 'persona:<stem>' or "
                                     "'character:<characterId>', got '" +
                                         npc.source + "'"});
        }
        if (npc.x < -kMapHalfSize || npc.x > kMapHalfSize || npc.z < -kMapHalfSize ||
            npc.z > kMapHalfSize) {
            errors.push_back({where, "position out of bounds"});
            continue;
        }
        for (std::size_t k = 0; k < footprints.size(); ++k) {
            if (!footprints[k].second) continue;
            if (distSqToFootprint(footprints[k].first, npc.x, npc.z) <
                kBodyRadius * kBodyRadius) {
                errors.push_back({where, "position is inside solid pieces[" +
                                             std::to_string(k) + "]"});
                break;
            }
        }
    }
    return errors;
}

City buildCity(const SandboxMap& map) {
    // Solid pieces compile to normal Building rows — collision (City) and
    // rendering (Assets, via the '#'-stripped id) follow with zero new
    // runtime. Visual pieces emit nothing. Invalid entries are skipped
    // (callers gate on validateMap first; this stays total regardless).
    std::vector<Building> buildings;
    int instance = 0;
    for (const PlacedPiece& placed : map.pieces) {
        const PieceDef* piece = findPiece(placed.pieceId);
        if (!piece || !piece->solid) continue;
        const Footprint f = footprintOf(placed, *piece);
        Building b;
        b.id = piece->assetId + "#" + std::to_string(instance++);
        b.name = "";  // sandbox pieces carry no dialogue-spot names in v1
        b.minX = f.minX;
        b.minZ = f.minZ;
        b.maxX = f.maxX;
        b.maxZ = f.maxZ;
        b.height = piece->height;
        b.facadeKind = 0;
        buildings.push_back(std::move(b));
    }
    return City::fromBuildings(std::move(buildings), kMapHalfSize);
}

}  // namespace llm_npc
