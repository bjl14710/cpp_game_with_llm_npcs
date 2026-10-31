#pragma once

#include <string>
#include <vector>

#include "CharacterStore.hpp"
#include "City.hpp"
#include "PersonaLoader.hpp"
#include "PieceCatalog.hpp"

namespace llm_npc {

// The sandbox map document (plan: sandbox-map-editor). This JSON shape is
// ALSO the output contract for LLM map generation (plan:
// llm-world-generation) — keep it flat, explicit, and free of derived
// state:
//   { "version": 1, "name": "...", "tile": 8,
//     "pieces": [{"piece": "<pieceId>", "x": <tileX>, "z": <tileZ>}, ...],
//     "npcs":   [{"source": "persona:<stem>" | "character:<characterId>",
//                 "x": <units>, "z": <units>, "facing": <deg>}, ...] }
constexpr int kMapFormatVersion = 1;
constexpr float kMapTileUnits = 8.f;  // grid pitch; divides blocks & streets

struct PlacedPiece {
    std::string pieceId;
    int tileX = 0;  // tile coordinates (world = tile * kMapTileUnits)
    int tileZ = 0;
};

struct PlacedNpc {
    // "persona:baker" (shipped roster) or "character:<characterId>"
    // (creator-made, resolved through CharacterStore).
    std::string source;
    float x = 0.f;  // world units (NPCs are not grid-bound)
    float z = 0.f;
    float facingDeg = 0.f;
};

struct SandboxMap {
    std::string name;
    std::vector<PlacedPiece> pieces;
    std::vector<PlacedNpc> npcs;

    // Compact JSON per the contract above; fromJson returns false on
    // malformed input or a newer version.
    std::string toJson() const;
    static bool fromJson(const std::string& json, SandboxMap& out);
};

// One validation error, specific enough to be BOTH a toast for the human
// editor and verbatim retry feedback for the LLM generator (that reuse is
// the point — see llm-world-generation).
struct MapError {
    std::string where;   // "pieces[3]" / "npcs[0]"
    std::string reason;  // "unknown piece id 'shp_bakery'" ...
};

// Full validation — piece ids exist, placements in bounds, no SOLID
// footprint overlaps, npc positions not inside solids, npc sources
// well-formed. Returns EVERY problem, not just the first.
std::vector<MapError> validateMap(const SandboxMap& map);

// Resolves a placed NPC to a spawnable LoadedPersona: identity from the
// shipped roster ("persona:<stem>") or the character store
// ("character:<id>"), position/facing from the placement, schedule
// CLEARED — town schedules reference town coordinates, which are
// nonsense in a custom map (logged decision). Returns false when the
// source is unknown or its records are gone; the map still loads and the
// caller logs the skip.
bool resolvePlacedNpc(const PlacedNpc& placed,
                      const std::vector<LoadedPersona>& roster,
                      const CharacterStore& store, LoadedPersona& out);

// Compiles the document to a normal City — each solid piece becomes a
// Building row (id = assetId + "#" + n, AABB from tile footprint, height
// from the piece) so collision AND rendering come from the existing
// systems with zero new runtime. Visual pieces emit no Building.
City buildCity(const SandboxMap& map);

}  // namespace llm_npc
