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

    // The one fog density every atmosphere shader is driven with (fog, cel,
    // outline — all three read this same value). Public and singular on
    // purpose: the NPC cull radius (issue #170) is DERIVED from it via
    // fogOpaqueDistance(), so tuning the haze automatically moves the cull
    // and the two cannot drift apart and cause visible popping. It used to
    // be three identical locals in Assets.cpp, which is exactly the drift
    // waiting to happen.
    static constexpr float kFogDensity = 0.006f;

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

    // Stylized face decal for the quaternius mesh family (issue #140),
    // by creator pick (eye/mouth style) and current mood. Out-of-range
    // styles clamp to 0 so stored looks never index past the set.
    const Texture2D& stylizedFace(int eyeStyle, int mouthStyle, NpcFace mood) const {
        if (eyeStyle < 0 || eyeStyle >= FaceTexture::kEyeStyleCount) eyeStyle = 0;
        if (mouthStyle < 0 || mouthStyle >= FaceTexture::kMouthStyleCount)
            mouthStyle = 0;
        return stylizedFaces_[eyeStyle][mouthStyle][static_cast<std::size_t>(mood)];
    }

    // The atmosphere shader (distance fog + warm tint), assigned to every
    // model material at load. The renderer also wraps the primitive batch
    // in it (BeginShaderMode) so ground/props haze consistently with
    // models. nullptr when shader compilation failed (plain look).
    const Shader* fogShader() const { return fogLoaded_ ? &fogShader_ : nullptr; }

    // Locations of the shader's per-frame uniforms: camera position, plus
    // fog color and scene light level, which the renderer drives every
    // frame from the shared world clock's day/night curves.
    int fogCameraLoc() const { return fogCameraLoc_; }
    int fogColorLoc() const { return fogColorLoc_; }
    int fogLightLoc() const { return fogLightLoc_; }

    // The character shader (issue #138): banded cel diffuse composed with
    // the SAME fog stage in one program. Assigned to every character
    // material at load; the renderer wraps composite-part and viewmodel
    // primitive draws in it so no character surface renders on the default
    // lit material. nullptr when compilation failed — characters then keep
    // the fog shader (never the default material).
    const Shader* celShader() const { return celLoaded_ ? &celShader_ : nullptr; }
    int celCameraLoc() const { return celCameraLoc_; }
    int celColorLoc() const { return celColorLoc_; }
    int celLightLoc() const { return celLightLoc_; }

    // Inverted-hull outline shader for MESH characters (the model-space
    // counterpart of the #103 primitive hull): inflates along vertex
    // normals, shades solid rim color, fades into fog with distance.
    const Shader* outlineShader() const {
        return outlineLoaded_ ? &outlineShader_ : nullptr;
    }
    int outlineCameraLoc() const { return outlineCameraLoc_; }
    int outlineColorLoc() const { return outlineColorLoc_; }

    // Ready-to-use material carrying the outline shader, so the renderer's
    // rim pass can DrawMesh directly without mutating a model's own
    // materials mid-frame. nullptr when the shader didn't compile.
    const Material* outlineMaterial() const {
        return outlineLoaded_ ? &outlineMaterial_ : nullptr;
    }

    // Mesh-backed character part (issue #139): the raylib meshes one
    // catalog PartDef draws, the model whose materials draw them, and the
    // measured bind-pose bounds the renderer anchors by (bottom-center on
    // the assembly anchor — the same convention primitive recipes use).
    // Resolved once at load from PartDef::meshName ("File:NodeA+NodeB",
    // where glTF node names are File_NodeA, ...). nullptr when the file or
    // node is missing — the part falls back to its primitive box.
    struct PartMeshes {
        const Model* model = nullptr;
        std::vector<int> meshes;  // raylib mesh indices (one per primitive)
        Vector3 boundsMin{};
        Vector3 boundsMax{};
    };
    const PartMeshes* partMeshes(const std::string& meshName) const;

    // Tier-B locomotion (issue #142): CPU-skins one modular pack model to
    // its Idle or Walk clip at `timeSec` (60 samples/second, looped). The
    // renderer calls this per figure for each file its parts draw from —
    // every file shares one animation library, so a mixed head/body pose
    // in agreement. Unknown stem or missing clip is a safe no-op.
    void poseModular(const std::string& stem, bool walking, float timeSec);

   private:
    // Loads one city model by file stem; records it in models_ and returns
    // success. Missing files are logged once and tolerated.
    bool loadCityModel(const std::string& stem);

    // Loads one character glb + animations; resolves the named clips.
    void loadCharacter(const std::string& stem);

    // Loads the modular part models the catalog references and resolves
    // every mesh-backed PartDef to its raylib mesh indices (issue #139).
    void loadModularParts();

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

    // Modular part pack (issue #139): one model per file stem, plus the
    // per-PartDef mesh resolution keyed by the exact meshName string.
    // meshRemaps_ carries each file's pre- -> post-compaction mesh index
    // mapping (unskinned prop meshes are dropped at load).
    std::string modularDir_;
    std::unordered_map<std::string, Model> modularModels_;
    std::unordered_map<std::string, std::vector<int>> meshRemaps_;
    std::unordered_map<std::string, PartMeshes> partMeshes_;

    // Per-file animation library for the modular pack (issue #142).
    struct ModularClips {
        ModelAnimation* clips = nullptr;
        int count = 0;
        int idle = -1;  // "Idle"
        int walk = -1;  // "Walk"
    };
    std::unordered_map<std::string, ModularClips> modularClips_;

    // One baked emote per NpcFace value, indexed by the enum.
    Texture2D faces_[6] = {};

    // Stylized face decals (issue #140): eye style x mouth style x mood.
    Texture2D stylizedFaces_[FaceTexture::kEyeStyleCount]
                            [FaceTexture::kMouthStyleCount][6] = {};

    // Distance-fog + warm-tint shader shared by every material.
    Shader fogShader_{};
    bool fogLoaded_ = false;
    int fogCameraLoc_ = -1;
    int fogColorLoc_ = -1;
    int fogLightLoc_ = -1;

    // Character cel shader + mesh outline shader (issue #138). Same
    // per-frame uniforms as fog (cameraPos / fogColor / lightLevel), so
    // the renderer drives all programs from one place.
    Shader celShader_{};
    bool celLoaded_ = false;
    int celCameraLoc_ = -1;
    int celColorLoc_ = -1;
    int celLightLoc_ = -1;
    Shader outlineShader_{};
    bool outlineLoaded_ = false;
    int outlineCameraLoc_ = -1;
    int outlineColorLoc_ = -1;
    Material outlineMaterial_{};  // default maps + outlineShader_
};

}  // namespace llm_npc
