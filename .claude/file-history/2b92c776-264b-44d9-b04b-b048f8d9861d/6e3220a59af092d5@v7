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

// Parses one "HH-HH, x, z, activity" schedule value. Hours may be
// fractional; the activity is everything after the third comma (it may
// itself contain commas). Returns false on any malformed piece.
bool parseScheduleEntry(const std::string& s, ScheduleEntry& out) {
    const auto c1 = s.find(',');
    if (c1 == std::string::npos) return false;
    const auto c2 = s.find(',', c1 + 1);
    if (c2 == std::string::npos) return false;
    const auto c3 = s.find(',', c2 + 1);
    if (c3 == std::string::npos) return false;

    const std::string hours = trim(s.substr(0, c1));
    const auto dash = hours.find('-');
    if (dash == std::string::npos || dash == 0 || dash + 1 >= hours.size()) return false;
    try {
        out.startHour = std::stof(hours.substr(0, dash));
        out.endHour = std::stof(hours.substr(dash + 1));
        out.position.x = std::stof(trim(s.substr(c1 + 1, c2 - c1 - 1)));
        out.position.z = std::stof(trim(s.substr(c2 + 1, c3 - c2 - 1)));
        out.position.y = 0.f;
    } catch (const std::exception&) {
        return false;
    }
    if (out.startHour < 0.f || out.startHour >= 24.f || out.endHour < 0.f ||
        out.endHour >= 24.f) {
        return false;
    }
    out.activity = trim(s.substr(c3 + 1));
    return !out.activity.empty();
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
            } else if (key == "armed") {
                // Carries a weapon: retaliates (turns Hostile) when attacked
                // instead of fleeing. Same true/yes/1 convention.
                result.value.persona.armed = (val == "true" || val == "yes" || val == "1");
            } else if (key == "look") {
                // Appearance from the shared parts library. Structural
                // parse only; whether the ids exist in the catalog is
                // checked at spawn by lookForPersona. Six items since the
                // Mouth category (issue #104): body, head, eyes, hair,
                // mouth, palette. The pre-Mouth five-item form still
                // parses — the mouth defaults to the canonical smile so
                // old files keep their authored identity.
                const auto items = splitCsv(val);
                if (items.size() != 5 && items.size() != 6) {
                    result.error =
                        id + ": bad look '" + val +
                        "' (want: body, head, eyes, hair, mouth, palette)";
                    return result;
                }
                result.value.look.part(PartCategory::Body) = items[0];
                result.value.look.part(PartCategory::Head) = items[1];
                result.value.look.part(PartCategory::Eyes) = items[2];
                result.value.look.part(PartCategory::Hair) = items[3];
                result.value.look.part(PartCategory::Mouth) =
                    items.size() == 6 ? items[4] : "mouth_smile";
                result.value.look.paletteId = items.back();
                result.value.hasLook = true;
            } else if (key == "schedule") {
                // Repeated key: one daily-routine block per line, driven by
                // the shared world clock (never a private timer).
                ScheduleEntry entry;
                if (!parseScheduleEntry(val, entry)) {
                    result.error = id + ": bad schedule '" + val +
                                   "' (want: HH-HH, x, z, activity)";
                    return result;
                }
                result.value.schedule.push_back(std::move(entry));
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

std::string renderPersonaText(const LoadedPersona& loaded) {
    std::ostringstream out;
    const Persona& p = loaded.persona;
    out << "name = " << p.name << '\n';
    if (!p.role.empty()) out << "role = " << p.role << '\n';
    if (!p.traits.empty()) {
        out << "traits = ";
        for (std::size_t i = 0; i < p.traits.size(); ++i) {
            if (i) out << ", ";
            out << p.traits[i];
        }
        out << '\n';
    }
    if (!p.speakingStyle.empty()) out << "style = " << p.speakingStyle << '\n';
    if (!p.knowledgeBoundary.empty()) out << "knowledge = " << p.knowledgeBoundary << '\n';
    if (!loaded.spotId.empty()) out << "spot = " << loaded.spotId << '\n';
    out << "position = " << loaded.position.x << ", " << loaded.position.z << '\n';
    out << "facing = " << loaded.facingDeg << '\n';
    if (loaded.hasLook) {
        // Always the six-item form — parse accepts five for old files, but
        // everything written back is current-format.
        out << "look = ";
        for (int c = 0; c < kPartCategoryCount; ++c) {
            out << loaded.look.partIds[c] << ", ";
        }
        out << loaded.look.paletteId << '\n';
    }
    for (const ScheduleEntry& entry : loaded.schedule) {
        out << "schedule = " << entry.startHour << '-' << entry.endHour << ", "
            << entry.position.x << ", " << entry.position.z << ", "
            << entry.activity << '\n';
    }
    if (p.police) out << "police = true\n";
    if (p.armed) out << "armed = true\n";
    if (!p.extraDirectives.empty()) out << "---\n" << p.extraDirectives << '\n';
    return out.str();
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
