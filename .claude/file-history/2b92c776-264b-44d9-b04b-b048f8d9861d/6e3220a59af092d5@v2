#include "PersonaLoader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Config.hpp"

namespace llm_npc {

namespace {

// Split a comma-separated list into trimmed, non-empty items.
std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream in(s);
    while (std::getline(in, item, ',')) {
        item = trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

// Parse "x, z" into a ground-plane position. Returns false on bad input.
bool parsePosition(const std::string& s, Vec3& out) {
    auto parts = splitCsv(s);
    if (parts.size() != 2) return false;
    try {
        out.x = std::stof(parts[0]);
        out.z = std::stof(parts[1]);
        out.y = 0.f;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

}  // namespace

PersonaParseResult parsePersonaText(const std::string& text, const std::string& id) {
    PersonaParseResult result;
    result.value.id = id;

    std::istringstream in(text);
    std::string line;
    std::ostringstream extra;
    bool inExtra = false;

    while (std::getline(in, line)) {
        if (!inExtra) {
            const std::string t = trim(line);
            if (t == "---") {
                inExtra = true;
                continue;
            }
            if (t.empty()) continue;
            const auto eq = t.find('=');
            if (eq == std::string::npos) {
                result.error = id + ": header line without '=': " + t;
                return result;
            }
            const std::string key = trim(t.substr(0, eq));
            const std::string val = trim(t.substr(eq + 1));
            if (key == "name") {
                result.value.persona.name = val;
            } else if (key == "role") {
                result.value.persona.role = val;
            } else if (key == "traits") {
                result.value.persona.traits = splitCsv(val);
            } else if (key == "style") {
                result.value.persona.speakingStyle = val;
            } else if (key == "knowledge") {
                result.value.persona.knowledgeBoundary = val;
            } else if (key == "spot") {
                result.value.spotId = val;
            } else if (key == "position") {
                if (!parsePosition(val, result.value.position)) {
                    result.error = id + ": bad position '" + val + "' (want: x, z)";
                    return result;
                }
            } else if (key == "facing") {
                try {
                    result.value.facingDeg = std::stof(val);
                } catch (const std::exception&) {
                    result.error = id + ": bad facing '" + val + "'";
                    return result;
                }
            } else if (key == "police") {
                // Grants the arrest action; everyone else can only summon
                // the police. Accepts true/yes/1 (anything else is false).
                result.value.persona.police = (val == "true" || val == "yes" || val == "1");
            } else {
                result.error = id + ": unknown header key '" + key + "'";
                return result;
            }
        } else {
            extra << line << '\n';
        }
    }

    if (result.value.persona.name.empty()) {
        result.error = id + ": missing required 'name'";
        return result;
    }
    result.value.persona.extraDirectives = trim(extra.str());
    result.ok = true;
    return result;
}

PersonaParseResult parsePersonaFile(const std::filesystem::path& path) {
    const std::string id = path.stem().string();
    std::ifstream in(path);
    if (!in) {
        PersonaParseResult result;
        result.error = id + ": cannot open " + path.string();
        return result;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return parsePersonaText(buf.str(), id);
}

std::vector<LoadedPersona> loadAllPersonas(const std::filesystem::path& dir,
                                           std::vector<std::string>* errors) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".persona") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::vector<LoadedPersona> out;
    for (const auto& f : files) {
        PersonaParseResult r = parsePersonaFile(f);
        if (r.ok) {
            out.push_back(std::move(r.value));
        } else if (errors) {
            errors->push_back(r.error);
        }
    }
    return out;
}

}  // namespace llm_npc
