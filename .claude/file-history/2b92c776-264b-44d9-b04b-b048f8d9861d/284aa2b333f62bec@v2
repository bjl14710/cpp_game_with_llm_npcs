#pragma once

// NOT YET IN THE BUILD — see the raylib migration block in CMakeLists.txt.
// Plan: .claude/plans/raylib-visual-overhaul.md, step 3 (city) / 4 (characters).
#include "raylib.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "City.hpp"

namespace llm_npc {

// Loads and owns every model, animation, font, and generated face texture,
// once, at startup (no mid-game loads — acceptance criterion: no frame
// hitches). Maps stable game identifiers → assets the same way the legacy
// renderer mapped them → procedural tints:
//   - Building::facadeKind + spotId ("bakery", "police", ...) → building model
//   - character variantSeed (NPC index / 1000+playerId)       → character model
// Missing files degrade to primitives, never crash (plan edge cases).
class Assets {
   public:
    // Searches `assetsDir` (assets/ at the project root — the tree
    // tools/fetch_assets.sh produces). Missing dir → loaded() stays false
    // and every accessor returns its fallback.
    // TODO(implement): load .glb models + ModelAnimations, the bundled font,
    // and bake the six NpcFace textures via FaceTexture.
    explicit Assets(const std::string& assetsDir);
    ~Assets();

    Assets(const Assets&) = delete;
    Assets& operator=(const Assets&) = delete;

    // False when the asset tree was missing/unreadable; main.cpp shows the
    // "run tools/fetch_assets.sh" notice and the game runs on primitives.
    bool loaded() const { return loaded_; }

    // Model for a building; named spots (bakery, police, ...) get curated
    // picks, filler blocks rotate through generic models by facadeKind.
    // nullptr → caller draws the fallback cube.
    // TODO(implement)
    const Model* modelForBuilding(const Building& building) const;

    // Character model + clips for a stable variant seed. nullptr → capsule.
    // TODO(implement)
    const Model* modelForCharacter(int variantSeed) const;

    // Animation clip ids into the character's ModelAnimation array.
    // TODO(implement): resolved per pack at load time by clip name.
    int idleClip() const { return idleClip_; }
    int walkClip() const { return walkClip_; }
    int gestureClip() const { return gestureClip_; }

    // The 128x128 face texture for a mood (baked once by FaceTexture).
    // TODO(implement)
    const Texture2D* faceTexture(int npcFace) const;

    // The bundled UI font (raylib has no system-font lookup).
    // TODO(implement)
    const Font* uiFont() const;

   private:
    bool loaded_ = false;
    int idleClip_ = 0;
    int walkClip_ = 0;
    int gestureClip_ = 0;

    // TODO(implement): storage — vector<Model> buildings by kind, curated
    // map<string spotId, Model>, vector<Model+ModelAnimations> characters,
    // Texture2D faces[6], Font. Unload everything in the destructor.
};

}  // namespace llm_npc
