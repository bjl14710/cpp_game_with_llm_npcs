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
            v += part->id;
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
    v += "\nStyle rule: 'round' and 'blocky' family parts never mix; 'any' "
         "parts fit both.\n";
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
           "STRICT JSON, no prose, no code fences:\n"
           "{\"characters\": [\"<persona file text>\", ...]}\n"
           "Each persona file text uses key = value lines separated by \\n:\n"
           "name = <unique full name> (required)\n"
           "role = <one-line occupation>\n"
           "traits = <2-4 comma-separated adjectives>\n"
           "style = <how they speak, one line>\n"
           "knowledge = <what they know and don't, one line>\n"
           "position = <x>, <z>\n"
           "trait = <ONE trait id from the vocabulary> (0-2 such lines)\n"
           "look = <body>, <head>, <eyes>, <hair>, <mouth>, <palette> "
           "(required, ids from the vocabulary, style-consistent)\n" +
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
           "\"characters\": [\"<persona file text>\", ...]}\n"
           "name_slug = the character's name lowercased with spaces as '_'.\n"
           "Every npc source must match one generated character.\n"
           "Persona file text format:\n"
           "name = <unique full name> (required)\nrole = <occupation>\n"
           "traits = <adjectives>\nstyle = <speech>\nknowledge = <bounds>\n"
           "position = <x>, <z>\ntrait = <trait id> (0-2 lines)\n"
           "look = <body>, <head>, <eyes>, <hair>, <mouth>, <palette> (required)\n" +
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
        if (!e.is_string()) return false;
        cast.push_back({e.get<std::string>()});
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
