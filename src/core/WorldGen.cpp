#include "WorldGen.hpp"

#include <cctype>

#include "CharacterParts.hpp"
#include "PersonaLoader.hpp"
#include "json.hpp"

namespace llm_npc {

namespace {

const char* categoryKey(PartCategory c) {
    switch (c) {
        case PartCategory::Body: return "body";
        case PartCategory::Head: return "head";
        case PartCategory::Eyes: return "eyes";
        case PartCategory::Hair: return "hair";
        case PartCategory::Mouth: return "mouth";
    }
    return "?";
}

}  // namespace

std::string castVocabulary(const std::vector<TraitDef>& traitLibrary) {
    std::string v = "VOCABULARY (use ONLY these ids):\n";
    for (int c = 0; c < kPartCategoryCount; ++c) {
        v += std::string(categoryKey(static_cast<PartCategory>(c))) + " parts: ";
        bool first = true;
        for (const PartDef* part :
             partsForCategory(static_cast<PartCategory>(c), "any")) {
            if (!first) v += ", ";
            v += part->id + "(" + part->styleTag + ")";
            first = false;
        }
        v += "\n";
    }
    v += "palettes: ";
    for (std::size_t i = 0; i < paletteCatalog().size(); ++i) {
        if (i) v += ", ";
        v += paletteCatalog()[i].id;
    }
    v += "\ntraits: ";
    for (std::size_t i = 0; i < traitLibrary.size(); ++i) {
        if (i) v += ", ";
        v += traitLibrary[i].id;
    }
    v += "\nStyle rule: within one look, never mix a (round) part with a "
         "(blocky) part; (any) parts fit both families.\n";
    return v;
}

std::string mapVocabulary() {
    std::string v = "PIECES (id WxD tiles, one tile = 8 world units, world is "
                    "-110..110 in x and z, tile coords are integers, SW anchor):\n";
    for (const PieceDef& piece : pieceCatalog()) {
        v += piece.id + " " + std::to_string(piece.tilesW) + "x" +
             std::to_string(piece.tilesD) + "\n";
    }
    v += "Solid footprints must NOT overlap. NPC positions are world units, "
         "outside every solid footprint.\n";
    return v;
}

std::string buildCastPrompt(const std::vector<TraitDef>& traitLibrary) {
    return "You create characters for a small-town life game by emitting "
           "STRICT JSON, no prose, no code fences. Shape:\n"
           "{\"characters\": [{\"name\": \"<unique full name>\", "
           "\"role\": \"<occupation>\", \"traits\": \"<2-4 adjectives>\", "
           "\"style\": \"<how they speak>\", \"knowledge\": \"<what they "
           "know and don't>\", \"position\": \"<x>, <z>\", "
           "\"trait\": [<0-2 ids>], \"look\": \"<body>, <head>, <eyes>, "
           "<hair>, <mouth>, <palette>\"}]}\n"
           "\"trait\" ids must be copied VERBATIM from the traits list, or use "
           "[] — adjectives from the request (like 'curious') are NOT trait "
           "ids; they belong in \"traits\".\n"
           "Example look value (bare ids, six of them, style-consistent):\n"
           "\"body_round, head_round, eyes_happy, hair_bun, mouth_smile, warm\"\n"
           "The palette is the bare id ('warm', NOT 'palette_warm'). trait "
           "lines are OPTIONAL — use ONLY ids from the traits list below or "
           "omit the line entirely. Keep every look inside ONE style family "
           "('round' or 'blocky'; 'any' parts fit both).\n" +
           castVocabulary(traitLibrary) +
           "At most " + std::to_string(kWorldGenMaxCharacters) +
           " characters. Reply with ONLY the JSON object.";
}

std::string buildMapPrompt() {
    return "You lay out maps for a small-town life game by emitting STRICT "
           "JSON in this exact shape, no prose, no code fences:\n"
           "{\"version\": 1, \"name\": \"<short name>\", \"tile\": 8, "
           "\"pieces\": [{\"piece\": \"<piece id>\", \"x\": <tileX>, \"z\": "
           "<tileZ>}], \"npcs\": []}\n" +
           mapVocabulary() + "Reply with ONLY the JSON object.";
}

std::string buildVillagePrompt(const std::vector<TraitDef>& traitLibrary) {
    return "You create a populated map for a small-town life game by "
           "emitting STRICT JSON, no prose, no code fences:\n"
           "{\"map\": {\"version\": 1, \"name\": \"<short name>\", \"tile\": 8, "
           "\"pieces\": [{\"piece\": \"<piece id>\", \"x\": <tileX>, \"z\": "
           "<tileZ>}], \"npcs\": [{\"source\": \"character:gen_<name_slug>\", "
           "\"x\": <world x>, \"z\": <world z>, \"facing\": <deg>}]}, "
           "\"characters\": [{\"name\": \"...\", \"role\": \"...\", "
           "\"traits\": \"...\", \"style\": \"...\", \"knowledge\": "
           "\"...\", \"position\": \"<x>, <z>\", \"trait\": [], "
           "\"look\": \"<body>, <head>, <eyes>, <hair>, <mouth>, "
           "<palette>\"}]}\n"
           "name_slug = the character's name lowercased with spaces as '_'. "
           "Every npc source must match one generated character. \"trait\" "
           "ids come VERBATIM from the traits list or use []. The look is "
           "six bare ids, style-consistent.\n" +
           castVocabulary(traitLibrary) + mapVocabulary() +
           "At most " + std::to_string(kWorldGenMaxCharacters) +
           " characters. Reply with ONLY the JSON object.";
}

std::string generatedCharacterId(const std::string& name) {
    std::string slug = "gen_";
    for (const char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (c == ' ' || c == '-' || c == '\'') {
            if (!slug.empty() && slug.back() != '_') slug += '_';
        }
    }
    return slug;
}

bool parseGeneratedCast(const std::string& json,
                        std::vector<GeneratedCharacter>& out) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object() || !j.contains("characters") || !j["characters"].is_array()) {
        return false;
    }
    std::vector<GeneratedCharacter> cast;
    for (const auto& e : j["characters"]) {
        if (e.is_string()) {  // raw persona text also accepted
            cast.push_back({e.get<std::string>()});
            continue;
        }
        // The preferred contract: one JSON OBJECT per character (models
        // handle nested objects far better than newline-embedded strings —
        // measured during #126's live smoke, where all three local models
        // failed the string form identically). Converted here to the
        // persona text the EXISTING parser validates, so the one-loader
        // rule holds.
        if (!e.is_object()) return false;
        std::string text;
        for (const char* key : {"name", "role", "traits", "style", "knowledge"}) {
            if (e.contains(key) && e[key].is_string()) {
                text += std::string(key) + " = " + e[key].get<std::string>() + "\n";
            }
        }
        if (e.contains("position") && e["position"].is_string()) {
            text += "position = " + e["position"].get<std::string>() + "\n";
        }
        if (e.contains("trait")) {
            // Wrapper normalization only (never content repair): models
            // sometimes emit the array AS a string — "[grumpy]".
            const auto stripWrap = [](std::string s) {
                while (!s.empty() && (s.front() == '[' || s.front() == '"' ||
                                      s.front() == ' ')) {
                    s.erase(s.begin());
                }
                while (!s.empty() && (s.back() == ']' || s.back() == '"' ||
                                      s.back() == ' ')) {
                    s.pop_back();
                }
                return s;
            };
            if (e["trait"].is_string()) {
                const std::string id = stripWrap(e["trait"].get<std::string>());
                if (!id.empty()) text += "trait = " + id + "\n";
            } else if (e["trait"].is_array()) {
                for (const auto& id : e["trait"]) {
                    if (id.is_string()) {
                        text += "trait = " + id.get<std::string>() + "\n";
                    }
                }
            }
        }
        if (e.contains("look") && e["look"].is_string()) {
            text += "look = " + e["look"].get<std::string>() + "\n";
        }
        cast.push_back({std::move(text)});
    }
    if (cast.empty() || cast.size() > kWorldGenMaxCharacters) return false;
    out = std::move(cast);
    return true;
}

bool parseGeneratedVillage(const std::string& json, SandboxMap& mapOut,
                           std::vector<GeneratedCharacter>& castOut) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object() || !j.contains("map") || !j.contains("characters")) {
        return false;
    }
    if (!SandboxMap::fromJson(j["map"].dump(), mapOut)) return false;
    return parseGeneratedCast(
        nlohmann::json{{"characters", j["characters"]}}.dump(), castOut);
}

std::vector<MapError> validateVillageLinks(
    const SandboxMap& map, const std::vector<GeneratedCharacter>& cast) {
    std::vector<std::string> ids;
    for (const GeneratedCharacter& character : cast) {
        const PersonaParseResult parsed =
            parsePersonaText(character.personaText, "generated");
        if (parsed.ok) ids.push_back(generatedCharacterId(parsed.value.persona.name));
    }
    std::vector<MapError> errors;
    for (std::size_t i = 0; i < map.npcs.size(); ++i) {
        const std::string& source = map.npcs[i].source;
        if (source.rfind("character:gen_", 0) != 0) continue;  // roster refs ok
        const std::string key = source.substr(std::string("character:").size());
        if (std::find(ids.begin(), ids.end(), key) == ids.end()) {
            std::string valid;
            for (const std::string& id : ids) {
                if (!valid.empty()) valid += ", ";
                valid += id;
            }
            errors.push_back({"npcs[" + std::to_string(i) + "]",
                              "source '" + source +
                                  "' matches no generated character (valid: " +
                                  valid + ")"});
        }
    }
    return errors;
}

}  // namespace llm_npc
