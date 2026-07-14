#include "Assets.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "CharacterParts.hpp"
#include "FaceTexture.hpp"
#include "json.hpp"

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

// Atmosphere shader: distance fog toward the sky color plus a subtle warm
// tint, over the standard texture * diffuse * vertex-color pipeline. The
// vertex stage passes world-space position (matModel is identity for rlgl
// batch primitives, whose vertices are already world-space, so the same
// shader serves models AND primitives). GLSL 330 — the core profile raylib
// uses on desktop.
constexpr const char* kFogVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragPosition;
void main() {
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

constexpr const char* kFogFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 cameraPos;
uniform vec4 fogColor;
uniform float fogDensity;
uniform float lightLevel;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 warm = texel.rgb * vec3(1.06, 1.0, 0.92) * lightLevel;
    float d = length(cameraPos - fragPosition) * fogDensity;
    float fog = 1.0 - exp(-d * d);
    finalColor = vec4(mix(warm, fogColor.rgb, fog), texel.a);
}
)";

// Character cel shader (issue #138): 3-band toon diffuse composed with the
// SAME warm-tint + distance-fog stage as kFogFragmentShader, in one program.
// The face normal comes from screen-space derivatives of the world position
// (flat-faceted banding) rather than vertex normals, because the rlgl batch
// only fills normals for cubes — spheres/cylinders (most composite parts)
// push none. Derivative normals band EVERY character surface identically:
// batch primitives, skinned meshes, and the #139 part meshes. Same vertex
// stage as fog; only the fragment differs.
constexpr const char* kCelFragmentShader = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 cameraPos;
uniform vec4 fogColor;
uniform float fogDensity;
uniform float lightLevel;
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    // Flat face normal; fixed sun direction. No normalize(cross) on
    // degenerate fragments: guard keeps billboards/decals fully lit.
    vec3 dx = dFdx(fragPosition);
    vec3 dy = dFdy(fragPosition);
    vec3 n = cross(dx, dy);
    float ndl = 1.0;
    if (dot(n, n) > 1e-12) {
        ndl = max(dot(normalize(n), normalize(vec3(0.4, 0.8, 0.45))), 0.0);
    }
    // Quantize into 3 bands: shadow floor, mid tone, full light. The floor
    // stays readable (0.70) — the pack's baked outfit colors are already
    // dark, and a character facing away from the sun must not go murky.
    float band = ndl < 0.25 ? 0.70 : (ndl < 0.65 ? 0.87 : 1.0);
    vec3 warm = texel.rgb * band * vec3(1.06, 1.0, 0.92) * lightLevel;
    float d = length(cameraPos - fragPosition) * fogDensity;
    float fog = 1.0 - exp(-d * d);
    finalColor = vec4(mix(warm, fogColor.rgb, fog), texel.a);
}
)";

// Inverted-hull outline for MESH characters (the model counterpart of the
// #103 primitive hull): the vertex stage pushes each vertex out along its
// world-space normal by a fixed world width; drawn with front faces culled
// so only the rim survives. Meshes have real normals (unlike the batch),
// which is exactly why this pass is mesh-only.
constexpr const char* kOutlineVertexShader = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 matView;
uniform mat4 matProjection;
uniform float outlineWidth;
out vec3 fragPosition;
void main() {
    vec3 wp = vec3(matModel * vec4(vertexPosition, 1.0));
    vec3 wn = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragPosition = wp + wn * outlineWidth;
    gl_Position = matProjection * matView * vec4(fragPosition, 1.0);
}
)";

// Solid rim color, fading into fog with distance so far-off outlines melt
// into the horizon along with the geometry they wrap.
constexpr const char* kOutlineFragmentShader = R"(
#version 330
in vec3 fragPosition;
uniform vec3 cameraPos;
uniform vec4 fogColor;
uniform float fogDensity;
uniform vec4 outlineColor;
out vec4 finalColor;
void main() {
    float d = length(cameraPos - fragPosition) * fogDensity;
    float fog = 1.0 - exp(-d * d);
    finalColor = vec4(mix(outlineColor.rgb, fogColor.rgb, fog), 1.0);
}
)";

}  // namespace

Assets::Assets(const std::string& assetsDir) {
    // Mood emotes are procedural — baked here regardless of downloads.
    for (int i = 0; i < 6; ++i) {
        faces_[i] = FaceTexture::bake(static_cast<NpcFace>(i));
    }
    // The stylized decal set too (issue #140): every creator face pick x
    // every mood, a few rect draws each — negligible at boot.
    for (int e = 0; e < FaceTexture::kEyeStyleCount; ++e) {
        for (int m = 0; m < FaceTexture::kMouthStyleCount; ++m) {
            for (int f = 0; f < 6; ++f) {
                stylizedFaces_[e][m][f] =
                    FaceTexture::bakeStylized(e, m, static_cast<NpcFace>(f));
            }
        }
    }

    // Atmosphere shader first, so every model loaded below can adopt it.
    // Failure (no GL context, GLSL mismatch) degrades to the plain look.
    fogShader_ = LoadShaderFromMemory(kFogVertexShader, kFogFragmentShader);
    fogLoaded_ = IsShaderValid(fogShader_);
    if (fogLoaded_) {
        fogCameraLoc_ = GetShaderLocation(fogShader_, "cameraPos");
        fogColorLoc_ = GetShaderLocation(fogShader_, "fogColor");
        fogLightLoc_ = GetShaderLocation(fogShader_, "lightLevel");
        // Daylight defaults; the renderer re-drives fogColor and lightLevel
        // each frame from the shared world clock's day/night curves. The
        // density is tuned so the plaza (~40 units) stays crisp and the far
        // city edge (~150+) visibly hazes.
        const float skyColor[4] = {135.f / 255.f, 190.f / 255.f, 235.f / 255.f, 1.f};
        const float density = 0.006f;
        const float light = 1.f;
        SetShaderValue(fogShader_, fogColorLoc_, skyColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(fogShader_, GetShaderLocation(fogShader_, "fogDensity"),
                       &density, SHADER_UNIFORM_FLOAT);
        SetShaderValue(fogShader_, fogLightLoc_, &light, SHADER_UNIFORM_FLOAT);
    } else {
        std::cerr << "[llm_npc] fog shader failed to compile — plain look\n";
    }

    // Character cel shader (issue #138): shares the fog vertex stage and
    // uniform names, so the renderer drives fog + cel identically. Failure
    // degrades characters to the fog shader — never the default material.
    celShader_ = LoadShaderFromMemory(kFogVertexShader, kCelFragmentShader);
    celLoaded_ = IsShaderValid(celShader_);
    if (celLoaded_) {
        celCameraLoc_ = GetShaderLocation(celShader_, "cameraPos");
        celColorLoc_ = GetShaderLocation(celShader_, "fogColor");
        celLightLoc_ = GetShaderLocation(celShader_, "lightLevel");
        const float skyColor[4] = {135.f / 255.f, 190.f / 255.f, 235.f / 255.f, 1.f};
        const float density = 0.006f;
        const float light = 1.f;
        SetShaderValue(celShader_, celColorLoc_, skyColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(celShader_, GetShaderLocation(celShader_, "fogDensity"),
                       &density, SHADER_UNIFORM_FLOAT);
        SetShaderValue(celShader_, celLightLoc_, &light, SHADER_UNIFORM_FLOAT);
    } else {
        std::cerr << "[llm_npc] cel shader failed to compile — characters keep "
                     "the fog look\n";
    }

    // Mesh outline shader: fixed world-space rim width (relative to the
    // 1.8u character contract) and the #103 rim color, set once — only
    // cameraPos and fogColor change per frame.
    outlineShader_ =
        LoadShaderFromMemory(kOutlineVertexShader, kOutlineFragmentShader);
    outlineLoaded_ = IsShaderValid(outlineShader_);
    if (outlineLoaded_) {
        outlineCameraLoc_ = GetShaderLocation(outlineShader_, "cameraPos");
        outlineColorLoc_ = GetShaderLocation(outlineShader_, "fogColor");
        const float width = 0.022f;
        const float rim[4] = {32.f / 255.f, 30.f / 255.f, 38.f / 255.f, 1.f};
        const float skyColor[4] = {135.f / 255.f, 190.f / 255.f, 235.f / 255.f, 1.f};
        const float density = 0.006f;
        SetShaderValue(outlineShader_, GetShaderLocation(outlineShader_, "outlineWidth"),
                       &width, SHADER_UNIFORM_FLOAT);
        SetShaderValue(outlineShader_, GetShaderLocation(outlineShader_, "outlineColor"),
                       rim, SHADER_UNIFORM_VEC4);
        SetShaderValue(outlineShader_, outlineColorLoc_, skyColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(outlineShader_, GetShaderLocation(outlineShader_, "fogDensity"),
                       &density, SHADER_UNIFORM_FLOAT);
        outlineMaterial_ = LoadMaterialDefault();
        outlineMaterial_.shader = outlineShader_;
    } else {
        std::cerr << "[llm_npc] outline shader failed to compile — mesh "
                     "characters render rimless\n";
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
        // No "fountain" entry: the pack has no fountain model, so the
        // renderer composes one from primitives (drawFountain), taking its
        // height from the SizeSpec below. An entry here would draw a model
        // instead and skip the composite.
        {"trafficlight_a", "trafficlight_A"}, {"trafficlight_b", "trafficlight_A"},
        {"trafficlight_c", "trafficlight_A"}, {"trafficlight_d", "trafficlight_A"},
        {"bush_a", "bush"}, {"bush_b", "bush"},
        {"bush_c", "bush"}, {"bush_d", "bush"},
        {"police_car", "car_police"},
        {"sedan_a", "car_sedan"}, {"sedan_b", "car_sedan"},
        {"hatchback_a", "car_hatchback"}, {"hatchback_b", "car_hatchback"},
        {"dumpster_a", "dumpster"},
        {"trash_a", "trash_A"}, {"trash_b", "trash_B"},
    };
    // Size contracts for everything that must not stretch to its collision
    // box. Heights are world meters, chosen against the 1.8-unit characters.
    sizeSpecs_ = {
        {"taxi_cab", {SizeSpec::Mode::Uniform, 1.5f}},
        {"cart", {SizeSpec::Mode::Uniform, 1.9f}},
        {"bench", {SizeSpec::Mode::Uniform, 0.9f}},
        // Composite fountain (drawFountain): total height of basin + column
        // + finial. 1.2 (the old base-tile pancake height) reads flat on the
        // 8-unit footprint; 2.6 gives a recognizable centerpiece.
        {"fountain", {SizeSpec::Mode::Uniform, 2.6f}},
        {"trafficlight_a", {SizeSpec::Mode::Uniform, 4.5f}},
        {"trafficlight_b", {SizeSpec::Mode::Uniform, 4.5f}},
        {"trafficlight_c", {SizeSpec::Mode::Uniform, 4.5f}},
        {"trafficlight_d", {SizeSpec::Mode::Uniform, 4.5f}},
        {"bush_a", {SizeSpec::Mode::Uniform, 1.1f}},
        {"bush_b", {SizeSpec::Mode::Uniform, 1.1f}},
        {"bush_c", {SizeSpec::Mode::Uniform, 1.1f}},
        {"bush_d", {SizeSpec::Mode::Uniform, 1.1f}},
        {"police_car", {SizeSpec::Mode::Uniform, 1.5f}},
        {"sedan_a", {SizeSpec::Mode::Uniform, 1.35f}},
        {"sedan_b", {SizeSpec::Mode::Uniform, 1.35f}},
        {"hatchback_a", {SizeSpec::Mode::Uniform, 1.35f}},
        {"hatchback_b", {SizeSpec::Mode::Uniform, 1.35f}},
        {"dumpster_a", {SizeSpec::Mode::Uniform, 1.4f}},
        {"trash_a", {SizeSpec::Mode::Uniform, 0.9f}},
        {"trash_b", {SizeSpec::Mode::Uniform, 0.7f}},
    };

    genericBuildings_ = {"building_A", "building_B", "building_C", "building_D",
                         "building_E", "building_F", "building_G", "building_H"};

    // Everything drawCity can ask for, loaded up front.
    const char* stems[] = {
        "building_A", "building_B", "building_C", "building_D", "building_E",
        "building_F", "building_G", "building_H", "bench", "car_taxi",
        "car_police", "car_sedan", "car_hatchback", "box_B", "base", "bush",
        "watertower", "trafficlight_A", "dumpster", "trash_A", "trash_B",
        "road_straight", "road_straight_crossing", "road_junction",
    };
    bool any = false;
    for (const char* stem : stems) any = loadCityModel(stem) || any;

    // Rigged characters: five KayKit adventurers; the Knight doubles as the
    // police uniform. Missing files just shrink the variant pool.
    for (const char* stem : {"Barbarian", "Knight", "Mage", "Rogue", "Rogue_Hooded"}) {
        loadCharacter(stem);
    }

    // Quaternius modular part meshes (issue #139), resolved from whatever
    // the part catalog references.
    modularDir_ = assetsDir + "/models/characters_modular";
    loadModularParts();

    loaded_ = any;
}

Assets::~Assets() {
    if (fogLoaded_) UnloadShader(fogShader_);
    if (celLoaded_) UnloadShader(celShader_);
    if (outlineLoaded_) {
        // Not UnloadMaterial: that would also unload outlineShader_ (freed
        // on the next line) — only the default maps array is ours to free.
        MemFree(outlineMaterial_.maps);
        UnloadShader(outlineShader_);
    }
    for (Texture2D& face : faces_) UnloadTexture(face);
    for (auto& perEye : stylizedFaces_) {
        for (auto& perMouth : perEye) {
            for (Texture2D& face : perMouth) UnloadTexture(face);
        }
    }
    for (auto& [stem, model] : models_) UnloadModel(model);
    for (auto& [stem, model] : modularModels_) UnloadModel(model);
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

    // Characters render through the cel shader (#138) — banded toon light
    // composed with the same fog stage. CPU skinning is unaffected: it
    // writes animated vertices into the mesh before drawing, and the shader
    // consumes only the standard position/texcoord/color attributes. Cel
    // compile failure falls back to fog; a character NEVER keeps the
    // default lit material (the routing rule this block enforces).
    if (celLoaded_ || fogLoaded_) {
        for (int i = 0; i < character.model.materialCount; ++i) {
            character.model.materials[i].shader = celLoaded_ ? celShader_ : fogShader_;
        }
    }

    character.clips = LoadModelAnimations(path.c_str(), &character.clipCount);
    for (int i = 0; i < character.clipCount; ++i) {
        const std::string name = character.clips[i].name;
        if (name == "Idle") character.idle = i;
        else if (name == "Walking_A") character.walk = i;
        else if (name == "Cheer") character.gesture = i;
        else if (name == "Death_A_Pose") character.death = i;
    }
    if (stem == "Knight") knightIndex_ = static_cast<int>(characters_.size());
    characters_.push_back(character);
}

void Assets::loadModularParts() {
    // glTF node names -> raylib mesh index ranges, per file stem. raylib
    // flattens glTF meshes PER PRIMITIVE in file order, so glTF mesh i's
    // primitives occupy a contiguous raylib range starting at the sum of
    // the primitive counts before it — read straight from the glTF JSON
    // (it's the same file LoadModel parses).
    struct NodeRange {
        int first = 0;
        int count = 0;
    };
    std::unordered_map<std::string, std::unordered_map<std::string, NodeRange>>
        nodeRanges;

    for (const PartDef& part : partCatalog()) {
        if (part.meshName.empty()) continue;
        const auto colon = part.meshName.find(':');
        if (colon == std::string::npos) {
            std::cerr << "[llm_npc] bad meshName (no ':'): " << part.meshName << "\n";
            continue;
        }
        const std::string stem = part.meshName.substr(0, colon);
        const std::string path = modularDir_ + "/" + stem + ".gltf";

        // Load the model + its node map once per file.
        auto modelIt = modularModels_.find(stem);
        if (modelIt == modularModels_.end()) {
            if (!std::filesystem::exists(path)) {
                std::cerr << "[llm_npc] missing modular pack file: " << path
                          << " (run tools/fetch_assets.sh; mesh parts draw "
                             "fallback boxes)\n";
                continue;
            }
            Model model = LoadModel(path.c_str());
            if (model.meshCount == 0) {
                std::cerr << "[llm_npc] failed to load modular file: " << path << "\n";
                continue;
            }
            // Compact out unskinned prop meshes (the SciFi pistol) exactly
            // like loadCharacter does — raylib's CPU skinning dereferences
            // boneWeights unconditionally — but KEEP the old->new index
            // remap: node resolution below still speaks pre-compaction
            // (glTF file order) indices.
            std::vector<int> remap(static_cast<std::size_t>(model.meshCount), -1);
            int kept = 0;
            for (int i = 0; i < model.meshCount; ++i) {
                if (model.meshes[i].boneWeights != nullptr) {
                    remap[static_cast<std::size_t>(i)] = kept;
                    model.meshes[kept] = model.meshes[i];
                    model.meshMaterial[kept] = model.meshMaterial[i];
                    ++kept;
                } else {
                    UnloadMesh(model.meshes[i]);
                }
            }
            model.meshCount = kept;
            // Same routing rule as every character surface (issue #138).
            if (celLoaded_ || fogLoaded_) {
                for (int i = 0; i < model.materialCount; ++i) {
                    model.materials[i].shader = celLoaded_ ? celShader_ : fogShader_;
                }
            }
            // Tier A stance (plan: static assembly, no per-frame animation):
            // pose the model ONCE at load to frame 0 of its Idle clip, so
            // static part meshes stand naturally instead of in the glTF
            // bind pose (T-pose arms). All four files share one skeleton
            // and animation library, so mixed heads/bodies pose in
            // agreement. Anchoring keeps using the bind-pose bounds — feet
            // stay at y=0 and the neck line doesn't move at frame 0.
            int clipCount = 0;
            ModelAnimation* clips = LoadModelAnimations(path.c_str(), &clipCount);
            for (int i = 0; i < clipCount; ++i) {
                if (std::string(clips[i].name) == "Idle") {
                    UpdateModelAnimation(model, clips[i], 0);
                    break;
                }
            }
            if (clips) UnloadModelAnimations(clips, clipCount);
            modelIt = modularModels_.emplace(stem, model).first;
            meshRemaps_.emplace(stem, std::move(remap));

            std::ifstream in(path);
            const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            auto& ranges = nodeRanges[stem];
            if (j.is_object() && j.contains("meshes") && j.contains("nodes")) {
                std::vector<int> base;
                int running = 0;
                for (const auto& mesh : j["meshes"]) {
                    base.push_back(running);
                    running += static_cast<int>(mesh.value("primitives",
                                                           nlohmann::json::array())
                                                    .size());
                }
                for (const auto& node : j["nodes"]) {
                    if (!node.contains("mesh") || !node.contains("name")) continue;
                    const int mi = node["mesh"].get<int>();
                    if (mi < 0 || mi >= static_cast<int>(base.size())) continue;
                    ranges[node["name"].get<std::string>()] = {
                        base[static_cast<std::size_t>(mi)],
                        static_cast<int>(j["meshes"][static_cast<std::size_t>(mi)]
                                             .value("primitives",
                                                    nlohmann::json::array())
                                             .size())};
                }
            }
        }

        // Resolve this part's '+'-separated node list against the map.
        PartMeshes resolved;
        resolved.model = &modelIt->second;
        const auto& ranges = nodeRanges[stem];
        bool ok = true;
        std::string nodeList = part.meshName.substr(colon + 1);
        while (ok && !nodeList.empty()) {
            const auto plus = nodeList.find('+');
            const std::string token = nodeList.substr(0, plus);
            nodeList = plus == std::string::npos ? "" : nodeList.substr(plus + 1);
            const auto range = ranges.find(stem + "_" + token);
            if (range == ranges.end()) {
                std::cerr << "[llm_npc] modular node not found: " << stem << "_"
                          << token << " (part " << part.id << ")\n";
                ok = false;
                break;
            }
            const std::vector<int>& remap = meshRemaps_[stem];
            for (int i = 0; i < range->second.count; ++i) {
                const int original = range->second.first + i;
                if (original >= static_cast<int>(remap.size())) continue;
                const int mapped = remap[static_cast<std::size_t>(original)];
                if (mapped >= 0) resolved.meshes.push_back(mapped);
            }
        }
        if (!ok || resolved.meshes.empty()) continue;

        // Measured union bounds (bind pose) — what the renderer anchors by.
        BoundingBox box =
            GetMeshBoundingBox(modelIt->second.meshes[resolved.meshes.front()]);
        for (const int idx : resolved.meshes) {
            const BoundingBox b = GetMeshBoundingBox(modelIt->second.meshes[idx]);
            box.min = Vector3{std::min(box.min.x, b.min.x), std::min(box.min.y, b.min.y),
                              std::min(box.min.z, b.min.z)};
            box.max = Vector3{std::max(box.max.x, b.max.x), std::max(box.max.y, b.max.y),
                              std::max(box.max.z, b.max.z)};
        }
        resolved.boundsMin = box.min;
        resolved.boundsMax = box.max;
        partMeshes_.emplace(part.meshName, std::move(resolved));
    }
}

const Assets::PartMeshes* Assets::partMeshes(const std::string& meshName) const {
    const auto it = partMeshes_.find(meshName);
    return it != partMeshes_.end() ? &it->second : nullptr;
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
    // Atmosphere: every material renders through the shared fog shader.
    // (UnloadModel leaves material shaders alone — raylib treats them as
    // user-owned precisely so they can be shared; the destructor unloads
    // fogShader_ exactly once.)
    if (fogLoaded_) {
        for (int i = 0; i < model.materialCount; ++i) {
            model.materials[i].shader = fogShader_;
        }
    }
    models_.emplace(stem, model);
    return true;
}

const Assets::SizeSpec& Assets::sizeSpecFor(const Building& building) const {
    static const SizeSpec fill{};  // Mode::Fill — the default contract
    // Same instance-suffix rule as modelForBuilding.
    const auto it = sizeSpecs_.find(building.id.substr(0, building.id.find('#')));
    return it != sizeSpecs_.end() ? it->second : fill;
}

const Model* Assets::modelForBuilding(const Building& building) const {
    if (!loaded_) return nullptr;
    // Sandbox pieces carry an instance suffix ("bakery#2") so Building ids
    // stay unique; every instance keys assets by the base id.
    const std::string baseId = building.id.substr(0, building.id.find('#'));
    const auto curated = curated_.find(baseId);
    const std::string stem =
        curated != curated_.end()
            ? curated->second
            : genericBuildings_[stableHash(baseId) % genericBuildings_.size()];
    const auto it = models_.find(stem);
    return it != models_.end() ? &it->second : nullptr;
}

const Model* Assets::prop(const std::string& stem) const {
    const auto it = models_.find(stem);
    return it != models_.end() ? &it->second : nullptr;
}

}  // namespace llm_npc
