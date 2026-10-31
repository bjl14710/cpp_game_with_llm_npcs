#include "Assets.hpp"

#include <filesystem>
#include <iostream>

#include "FaceTexture.hpp"

namespace llm_npc {

namespace {

// Stable non-cryptographic hash so a filler building keeps the same look
// every run without City needing to know about models.
std::size_t stableHash(const std::string& s) {
    std::size_t h = 1469598103934665603ull;  // FNV-1a
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

}  // namespace

Assets::Assets(const std::string& assetsDir) {
    // Mood emotes are procedural — baked here regardless of downloads.
    for (int i = 0; i < 6; ++i) {
        faces_[i] = FaceTexture::bake(static_cast<NpcFace>(i));
    }

    cityDir_ = assetsDir + "/models/city";
    charDir_ = assetsDir + "/models/characters";
    if (!std::filesystem::exists(cityDir_ + "/citybits_texture.png")) {
        std::cerr << "[llm_npc] assets/models missing — run tools/fetch_assets.sh "
                     "for the full look (drawing primitive shapes meanwhile)\n";
        return;
    }

    // Landmarks: hand-picked silhouettes so each named spot reads distinctly.
    curated_ = {
        {"bakery", "building_A"},   {"police", "building_E"},
        {"coffee", "building_B"},   {"library", "building_D"},
        {"hardware", "building_C"}, {"office_a", "building_F"},
        {"tower", "building_H"},    {"taxi_cab", "car_taxi"},
        {"bench", "bench"},         {"cart", "box_B"},
        {"fountain", "base"},
    };
    genericBuildings_ = {"building_A", "building_B", "building_C", "building_D",
                         "building_E", "building_F", "building_G", "building_H"};

    // Everything drawCity can ask for, loaded up front.
    const char* stems[] = {
        "building_A", "building_B", "building_C", "building_D", "building_E",
        "building_F", "building_G", "building_H", "bench", "car_taxi",
        "car_police", "car_sedan", "car_hatchback", "box_B", "base", "bush",
        "watertower", "trafficlight_A", "dumpster", "trash_A",
        "road_straight", "road_straight_crossing",
    };
    bool any = false;
    for (const char* stem : stems) any = loadCityModel(stem) || any;

    // Rigged characters: five KayKit adventurers; the Knight doubles as the
    // police uniform. Missing files just shrink the variant pool.
    for (const char* stem : {"Barbarian", "Knight", "Mage", "Rogue", "Rogue_Hooded"}) {
        loadCharacter(stem);
    }

    loaded_ = any;
}

Assets::~Assets() {
    for (Texture2D& face : faces_) UnloadTexture(face);
    for (auto& [stem, model] : models_) UnloadModel(model);
    for (auto& character : characters_) {
        if (character.clips) UnloadModelAnimations(character.clips, character.clipCount);
        UnloadModel(character.model);
    }
}

void Assets::loadCharacter(const std::string& stem) {
    const std::string path = charDir_ + "/" + stem + ".glb";
    if (!std::filesystem::exists(path)) {
        std::cerr << "[llm_npc] missing character: " << path << "\n";
        return;
    }
    CharacterAsset character;
    character.model = LoadModel(path.c_str());
    if (character.model.meshCount == 0) {
        std::cerr << "[llm_npc] failed to load character: " << path << "\n";
        return;
    }

    // The packs mix skinned body meshes with unskinned attachment meshes
    // (held items). raylib 5.5's CPU skinning dereferences boneWeights
    // unconditionally and crashes on the unskinned ones, so compact them
    // out — v1 characters simply don't show hand props.
    int kept = 0;
    for (int i = 0; i < character.model.meshCount; ++i) {
        if (character.model.meshes[i].boneWeights != nullptr) {
            character.model.meshes[kept] = character.model.meshes[i];
            character.model.meshMaterial[kept] = character.model.meshMaterial[i];
            ++kept;
        } else {
            UnloadMesh(character.model.meshes[i]);
        }
    }
    character.model.meshCount = kept;
    if (kept == 0) {
        std::cerr << "[llm_npc] character had no skinned meshes: " << path << "\n";
        UnloadModel(character.model);
        return;
    }

    character.clips = LoadModelAnimations(path.c_str(), &character.clipCount);
    for (int i = 0; i < character.clipCount; ++i) {
        const std::string name = character.clips[i].name;
        if (name == "Idle") character.idle = i;
        else if (name == "Walking_A") character.walk = i;
        else if (name == "Cheer") character.gesture = i;
    }
    if (stem == "Knight") knightIndex_ = static_cast<int>(characters_.size());
    characters_.push_back(character);
}

const Assets::CharacterAsset* Assets::characterFor(int variantSeed, bool police) const {
    if (characters_.empty()) return nullptr;
    if (police && knightIndex_ >= 0) return &characters_[static_cast<std::size_t>(knightIndex_)];
    return &characters_[static_cast<std::size_t>(variantSeed) % characters_.size()];
}

bool Assets::loadCityModel(const std::string& stem) {
    const std::string path = cityDir_ + "/" + stem + ".gltf";
    if (!std::filesystem::exists(path)) {
        std::cerr << "[llm_npc] missing model: " << path << " (using fallback shape)\n";
        return false;
    }
    Model model = LoadModel(path.c_str());
    if (model.meshCount == 0) {
        std::cerr << "[llm_npc] failed to load model: " << path << "\n";
        return false;
    }
    models_.emplace(stem, model);
    return true;
}

const Model* Assets::modelForBuilding(const Building& building) const {
    if (!loaded_) return nullptr;
    const auto curated = curated_.find(building.id);
    const std::string stem =
        curated != curated_.end()
            ? curated->second
            : genericBuildings_[stableHash(building.id) % genericBuildings_.size()];
    const auto it = models_.find(stem);
    return it != models_.end() ? &it->second : nullptr;
}

const Model* Assets::prop(const std::string& stem) const {
    const auto it = models_.find(stem);
    return it != models_.end() ? &it->second : nullptr;
}

}  // namespace llm_npc
