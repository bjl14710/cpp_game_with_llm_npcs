#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "City.hpp"

namespace llm_npc {

// Loads and owns every model, animation, and generated face texture, once,
// at startup (no mid-game loads — no frame hitches). Maps stable game
// identifiers → assets the same way the legacy renderer mapped them to
// procedural tints:
//   - Building::id ("bakery", "police", ...)  → curated model pick
//   - other buildings                         → generic pick hashed from id
//   - character variantSeed (NPC index, 1000+playerId) → character model
// Missing files degrade to primitives, never crash.
class Assets {
   public:
    // Searches `assetsDir` (assets/ at the project root — the tree
    // tools/fetch_assets.sh produces). Missing dir → loaded() stays false
    // and every accessor returns its fallback.
    explicit Assets(const std::string& assetsDir);
    ~Assets();

    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    // False when the city asset tree was missing/unreadable; main.cpp shows
    // the "run tools/fetch_assets.sh" notice and primitives are drawn.
    bool loaded() const { return loaded_; }

    // Model for a building; named spots (bakery, police, ...) get curated
    // picks, filler rotates through the generic buildings by id hash.
    // nullptr → caller draws the fallback cube.
    const Model* modelForBuilding(const Building& building) const;

    // Named prop/road models by pack file stem ("road_straight", "bush",
    // "car_police", ...). nullptr when absent.
    const Model* prop(const std::string& stem) const;

    // Character model + clips for a stable variant seed (issue #38).
    // nullptr → capsule fallback.
    const Model* modelForCharacter(int variantSeed) const;

   private:
    // Loads one city model by file stem; records it in models_ and returns
    // success. Missing files are logged once and tolerated.
    bool loadCityModel(const std::string& stem);

    bool loaded_ = false;
    std::string cityDir_;

    // All loaded city/prop models by file stem. Stable storage: the map
    // never changes after the constructor, so pointers into it live as long
    // as the Assets object.
    std::unordered_map<std::string, Model> models_;

    // Building id → curated model stem for the hand-picked landmarks.
    std::unordered_map<std::string, std::string> curated_;

    // Stems of the generic building pool used for filler blocks.
    std::vector<std::string> genericBuildings_;
};

}  // namespace llm_npc
