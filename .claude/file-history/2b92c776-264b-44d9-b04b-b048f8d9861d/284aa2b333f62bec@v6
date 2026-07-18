#pragma once

#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "City.hpp"
#include "FaceTexture.hpp"

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

    // How an entity's model maps into world space — the size CONTRACT that
    // makes relative scale independent of any specific asset's native
    // dimensions (swap the art pack, keep the sizes):
    //   Fill    — stretch to the collision AABB exactly (buildings: the
    //             visual must occupy its whole footprint).
    //   Uniform — scale uniformly to `worldHeight`, grounded and centered on
    //             the footprint (vehicles/props: proportions are part of the
    //             asset's identity; the AABB stays collision-only).
    struct SizeSpec {
        enum class Mode { Fill, Uniform };
        Mode mode = Mode::Fill;
        float worldHeight = 0.f;  // Uniform only; Fill takes the AABB height
    };

    // The size contract for a building/prop entity, by id. Unlisted ids
    // default to Fill (dense city blocks must fill their footprints).
    const SizeSpec& sizeSpecFor(const Building& building) const;

    // Model for a building; named spots (bakery, police, ...) get curated
    // picks, filler rotates through the generic buildings by id hash.
    // nullptr → caller draws the fallback cube.
    const Model* modelForBuilding(const Building& building) const;

    // Named prop/road models by pack file stem ("road_straight", "bush",
    // "car_police", ...). nullptr when absent.
    const Model* prop(const std::string& stem) const;

    // One rigged character: the model plus its animation clips, with the
    // clip indexes the renderer switches between resolved by name at load.
    struct CharacterAsset {
        Model model{};
        ModelAnimation* clips = nullptr;
        int clipCount = 0;
        int idle = -1;     // "Idle"
        int walk = -1;     // "Walking_A"
        int gesture = -1;  // "Cheer" — stands in for wave/raise-hand
        int death = -1;    // "Death_A_Pose" — collapsed on the ground
    };

    // Character for a stable variant seed; police always get the Knight (the
    // armored silhouette reads as a uniform). nullptr → capsule fallback.
    const CharacterAsset* characterFor(int variantSeed, bool police) const;

    // The baked emote texture for a mood face (always available — baked at
    // construction, no asset download involved).
    const Texture2D& faceTexture(NpcFace face) const {
        return faces_[static_cast<std::size_t>(face)];
    }

    // The atmosphere shader (distance fog + warm tint), assigned to every
    // model material at load. The renderer also wraps the primitive batch
    // in it (BeginShaderMode) so ground/props haze consistently with
    // models. nullptr when shader compilation failed (plain look).
    const Shader* fogShader() const { return fogLoaded_ ? &fogShader_ : nullptr; }

    // Location of the shader's per-frame camera-position uniform.
    int fogCameraLoc() const { return fogCameraLoc_; }

   private:
    // Loads one city model by file stem; records it in models_ and returns
    // success. Missing files are logged once and tolerated.
    bool loadCityModel(const std::string& stem);

    // Loads one character glb + animations; resolves the named clips.
    void loadCharacter(const std::string& stem);

    bool loaded_ = false;
    std::string cityDir_;
    std::string charDir_;

    // All loaded city/prop models by file stem. Stable storage: the map
    // never changes after the constructor, so pointers into it live as long
    // as the Assets object.
    std::unordered_map<std::string, Model> models_;

    // Building id → curated model stem for the hand-picked landmarks.
    std::unordered_map<std::string, std::string> curated_;

    // Entity id → size contract for everything that must NOT stretch to its
    // collision box (vehicles, street furniture). One table, no per-asset
    // scale constants anywhere else.
    std::unordered_map<std::string, SizeSpec> sizeSpecs_;

    // Stems of the generic building pool used for filler blocks.
    std::vector<std::string> genericBuildings_;

    // Loaded characters in file order; Knight's index for the police pick.
    std::vector<CharacterAsset> characters_;
    int knightIndex_ = -1;

    // One baked emote per NpcFace value, indexed by the enum.
    Texture2D faces_[6] = {};

    // Distance-fog + warm-tint shader shared by every material.
    Shader fogShader_{};
    bool fogLoaded_ = false;
    int fogCameraLoc_ = -1;
};

}  // namespace llm_npc
