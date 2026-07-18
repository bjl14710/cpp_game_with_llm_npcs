#include "WorldGenValidate.hpp"

#include <algorithm>

#include "CharacterParts.hpp"
#include "PersonaLoader.hpp"

namespace llm_npc {

namespace {

// Small edit distance for nearest-valid-id suggestions in error messages —
// the whole point of the validator's errors is to be actionable retry
// feedback ("unknown part id 'hair_dreads' (nearest: hair_braids)").
std::size_t editDistance(const std::string& a, const std::string& b) {
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            const std::size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

std::string nearestPartId(const std::string& id) {
    std::string best;
    std::size_t bestD = static_cast<std::size_t>(-1);
    for (const PartDef& part : partCatalog()) {
        const std::size_t d = editDistance(id, part.id);
        if (d < bestD) {
            bestD = d;
            best = part.id;
        }
    }
    return best;
}

}  // namespace

std::vector<CastError> validateCast(const std::vector<GeneratedCharacter>& cast,
                                    const std::vector<TraitDef>& traitLibrary) {
    std::vector<CastError> errors;
    std::vector<std::string> seenNames;

    for (std::size_t i = 0; i < cast.size(); ++i) {
        const std::string where = "characters[" + std::to_string(i) + "]";
        // The existing parser IS the persona validator — errors verbatim.
        const PersonaParseResult parsed =
            parsePersonaText(cast[i].personaText, "generated");
        if (!parsed.ok) {
            errors.push_back({where, parsed.error});
            continue;
        }
        const std::string& name = parsed.value.persona.name;
        if (std::find(seenNames.begin(), seenNames.end(), name) != seenNames.end()) {
            errors.push_back({where, "duplicate character name '" + name + "'"});
        }
        seenNames.push_back(name);

        if (parsed.value.hasLook) {
            std::string why;
            if (!lookIsValid(parsed.value.look, &why)) {
                // Enrich unknown-part failures with the nearest valid id.
                std::string reason = why;
                const std::string marker = "unknown part: ";
                const auto at = why.find(marker);
                if (at != std::string::npos) {
                    const std::string bad = why.substr(at + marker.size());
                    reason += " (nearest valid id: " + nearestPartId(bad) + ")";
                }
                errors.push_back({where, reason});
            }
        } else {
            errors.push_back({where, "missing 'look = body, head, eyes, hair, "
                                     "mouth, palette' line"});
        }
        for (const std::string& traitId : parsed.value.persona.traitIds) {
            const bool known = std::any_of(
                traitLibrary.begin(), traitLibrary.end(),
                [&](const TraitDef& t) { return t.id == traitId; });
            if (!known) {
                std::string valid;
                for (const TraitDef& t : traitLibrary) {
                    if (!valid.empty()) valid += ", ";
                    valid += t.id;
                }
                errors.push_back({where, "unknown trait id '" + traitId +
                                             "' (valid: " + valid + ")"});
            }
        }
    }
    return errors;
}

bool extractJsonObject(const std::string& raw, std::string& jsonOut) {
    // Models wrap JSON in fences and prose; take the first balanced object
    // or array, respecting strings and escapes. No partial extraction.
    const auto start = raw.find_first_of("{[");
    if (start == std::string::npos) return false;
    const char open = raw[start];
    const char close = open == '{' ? '}' : ']';
    int depth = 0;
    bool inString = false;
    for (std::size_t i = start; i < raw.size(); ++i) {
        const char c = raw[i];
        if (inString) {
            if (c == '\\') {
                ++i;  // skip the escaped character
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == open) {
            ++depth;
        } else if (c == close) {
            --depth;
            if (depth == 0) {
                jsonOut = raw.substr(start, i - start + 1);
                return true;
            }
        }
    }
    return false;
}

std::string renderRetryFeedback(const std::vector<MapError>& mapErrors,
                                const std::vector<CastError>& castErrors) {
    std::string out = "Your previous output was INVALID. Problems found:\n";
    for (const MapError& e : mapErrors) {
        out += "- " + e.where + ": " + e.reason + "\n";
    }
    for (const CastError& e : castErrors) {
        out += "- " + e.where + ": " + e.reason + "\n";
    }
    out += "Fix every problem and reply with ONLY the corrected JSON.\n";
    return out;
}

}  // namespace llm_npc
