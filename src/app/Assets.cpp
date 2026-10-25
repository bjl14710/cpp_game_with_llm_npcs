#include "Assets.hpp"

#include <filesystem>
#include <iostream>
#include <string>

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

// Atmosphere shader: distance fog toward the sky color plus a subtle warm
// tint, over the standard texture * diffuse * vertex-color pipeline. The
// vertex stage passes world-space position (matModel is identity for rlgl
// batch primitives, whose vertices are already world-space, so the same
// shader serves models AND primitives). GLSL 330 — the core profile raylib
// uses on desktop. The version line is prepended at load time (see
// shaderSource) so the fragment stage can be built with or without NOIR.
constexpr const char* kFogVertexShader = R"(
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

// Without NOIR this is exactly the original fog shader. With NOIR it adds
// the night terms, every one gated on uniforms the renderer zeroes in
// daylight (lampCount = 0), so the day path computes the same expression:
//   - lamp pools: warm point lights with a smooth windowed falloff, using a
//     flat normal from screen-space derivatives (works for models and the
//     normal-less primitive batch alike), plus warm glow mixed into the fog
//     around each lamp so light reads through the haze.
constexpr const char* kFogFragmentShader = R"(
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 cameraPos;
uniform vec4 fogColor;
uniform float fogDensity;
uniform float lightLevel;
#ifdef NOIR
#define MAX_LAMPS 16
uniform vec3 lampPos[MAX_LAMPS];
uniform int lampCount;
uniform vec3 lampColor;
uniform float lampRadius;
uniform float wetness;
#endif
out vec4 finalColor;
void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 warm = texel.rgb * vec3(1.06, 1.0, 0.92) * lightLevel;
    float d = length(cameraPos - fragPosition) * fogDensity;
    float fog = 1.0 - exp(-d * d);
    vec3 fogRgb = fogColor.rgb;
#ifdef NOIR
    if (lampCount > 0) {
        vec3 v = normalize(cameraPos - fragPosition);
        // Lines (rain) have no area: their derivative normal can vanish,
        // so fall back to facing the viewer instead of normalizing zero.
        vec3 nRaw = cross(dFdx(fragPosition), dFdy(fragPosition));
        vec3 n = dot(nRaw, nRaw) > 1e-12 ? normalize(nRaw) : v;
        if (dot(n, v) < 0.0) n = -n;
        bool ground = abs(n.y) > 0.9 && fragPosition.y < 0.15;
        bool grass = texel.g > texel.r * 1.12 && texel.g > texel.b;
        float wet = (ground && !grass) ? wetness : 0.0;
        vec3 fwd = vec3(v.x, 0.0, v.z);
        fwd = dot(fwd, fwd) > 1e-6 ? normalize(fwd) : vec3(0.0, 0.0, 1.0);
        vec3 side = vec3(fwd.z, 0.0, -fwd.x);
        vec3 pool = vec3(0.0);
        vec3 haze = vec3(0.0);
        vec3 spec = vec3(0.0);
        for (int i = 0; i < lampCount; ++i) {
            vec3 toLamp = lampPos[i] - fragPosition;
            float dist = length(toLamp);
            vec3 l = toLamp / max(dist, 1e-3);
            float k = clamp(1.0 - (dist * dist) / (lampRadius * lampRadius), 0.0, 1.0);
            float atten = k * k;
            float ndl = max(dot(n, l), 0.0);
            pool += lampColor * atten * (0.25 + 0.75 * ndl);
            haze += lampColor * atten;
            if (wet > 0.0) {
                vec3 h = normalize(l + v);
                float hs = dot(h, side);
                float hf = dot(h, fwd);
                float streak = exp(-(hs * hs) / 0.0016 - (hf * hf) / 0.035);
                float reach = clamp(1.0 - dist / (lampRadius * 2.5), 0.0, 1.0);
                spec += lampColor * streak * reach;
            }
        }
        warm += texel.rgb * pool;
        fogRgb += haze * 0.35;
        if (wet > 0.0) {
            float fres = pow(1.0 - max(dot(n, v), 0.0), 4.0);
            warm = warm * (1.0 - 0.3 * wet) + (spec + fogRgb * fres * 0.25) * wet;
        }
    }
#endif
    finalColor = vec4(mix(warm, fogRgb, fog), texel.a);
}
)";

// Prepends the GLSL version (and the NOIR switch) to a shader body.
std::string shaderSource(const char* body, bool noir) {
    return std::string("#version 330\n") + (noir ? "#define NOIR\n" : "") + body;
}

}  // namespace

Assets::Assets(const std::string& assetsDir) {
    // Mood emotes are procedural — baked here regardless of downloads.
    for (int i = 0; i < 6; ++i) {
        faces_[i] = FaceTexture::bake(static_cast<NpcFace>(i));
    }

    // Atmosphere shader first, so every model loaded below can adopt it.
    // The noir build is tried first; if it fails to compile the original
    // fog shader is used (today's look, no night extras), and if that
    // fails too (no GL context, GLSL mismatch) the plain look.
    const std::string vs = shaderSource(kFogVertexShader, false);
    for (const bool noir : {true, false}) {
        const std::string fs = shaderSource(kFogFragmentShader, noir);
        fogShader_ = LoadShaderFromMemory(vs.c_str(), fs.c_str());
        fogLoaded_ = IsShaderValid(fogShader_);
        if (fogLoaded_) {
            noirLoaded_ = noir;
            break;
        }
        std::cerr << "[llm_npc] " << (noir ? "noir" : "fog")
                  << " shader failed to compile — falling back\n";
    }
    if (fogLoaded_) {
        fogCameraLoc_ = GetShaderLocation(fogShader_, "cameraPos");
        fogColorLoc_ = GetShaderLocation(fogShader_, "fogColor");
        fogLightLoc_ = GetShaderLocation(fogShader_, "lightLevel");
        noirLocs_.fogDensity = GetShaderLocation(fogShader_, "fogDensity");
        if (noirLoaded_) {
            noirLocs_.lampPos = GetShaderLocation(fogShader_, "lampPos");
            noirLocs_.lampCount = GetShaderLocation(fogShader_, "lampCount");
            noirLocs_.lampColor = GetShaderLocation(fogShader_, "lampColor");
            noirLocs_.lampRadius = GetShaderLocation(fogShader_, "lampRadius");
            noirLocs_.wetness = GetShaderLocation(fogShader_, "wetness");
            const int noLamps = 0;
            SetShaderValue(fogShader_, noirLocs_.lampCount, &noLamps, SHADER_UNIFORM_INT);
        }
        // Daylight defaults; the renderer re-drives fogColor, lightLevel and
        // fogDensity each frame from the shared world clock's day/night
        // curves. The density is tuned so the plaza (~40 units) stays crisp
        // and the far city edge (~150+) visibly hazes.
        const float skyColor[4] = {135.f / 255.f, 190.f / 255.f, 235.f / 255.f, 1.f};
        const float light = 1.f;
        SetShaderValue(fogShader_, fogColorLoc_, skyColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(fogShader_, noirLocs_.fogDensity, &kFogDensity,
                       SHADER_UNIFORM_FLOAT);
        SetShaderValue(fogShader_, fogLightLoc_, &light, SHADER_UNIFORM_FLOAT);
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
        "streetlight",
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
    if (fogLoaded_) UnloadShader(fogShader_);
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

    // Characters fog too. CPU skinning is unaffected: it writes animated
    // vertices into the mesh before drawing, and the fog shader consumes
    // only the standard position/texcoord/color attributes.
    if (fogLoaded_) {
        for (int i = 0; i < character.model.materialCount; ++i) {
            character.model.materials[i].shader = fogShader_;
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
