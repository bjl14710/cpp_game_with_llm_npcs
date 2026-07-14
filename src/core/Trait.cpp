#include "Trait.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Config.hpp"  // trim

namespace llm_npc {

namespace {

// Parses one .trait file's text. Mirrors parsePersonaText's contract:
// key=value lines, unknown keys are named errors, examples must arrive as
// example_user/example_npc PAIRS in order (an orphan on either side is an
// authoring mistake worth failing loudly over).
bool parseTraitText(const std::string& text, const std::string& id, TraitDef& out,
                    std::string& error) {
    TraitDef trait;
    trait.id = id;
    std::istringstream in(text);
    std::string line;
    std::string pendingUser;
    bool hasPendingUser = false;

    while (std::getline(in, line)) {
        const std::string t = trim(line);
        if (t.empty()) continue;
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            error = id + ": line without '=': " + t;
            return false;
        }
        const std::string key = trim(t.substr(0, eq));
        const std::string val = trim(t.substr(eq + 1));
        if (key == "name") {
            trait.name = val;
        } else if (key == "rule") {
            if (val.empty()) {
                error = id + ": empty rule";
                return false;
            }
            trait.behaviorRules.push_back(val);
        } else if (key == "example_user") {
            if (hasPendingUser) {
                error = id + ": example_user without a following example_npc";
                return false;
            }
            pendingUser = val;
            hasPendingUser = true;
        } else if (key == "example_npc") {
            if (!hasPendingUser) {
                error = id + ": example_npc without a preceding example_user";
                return false;
            }
            trait.examples.push_back({pendingUser, val});
            hasPendingUser = false;
        } else {
            error = id + ": unknown key '" + key + "'";
            return false;
        }
    }
    if (hasPendingUser) {
        error = id + ": example_user without a following example_npc";
        return false;
    }
    if (trait.name.empty()) {
        error = id + ": missing required 'name'";
        return false;
    }
    if (trait.behaviorRules.empty()) {
        error = id + ": a trait needs at least one rule";
        return false;
    }
    out = std::move(trait);
    return true;
}

}  // namespace

std::vector<TraitDef> loadAllTraits(const std::filesystem::path& dir,
                                    std::vector<std::string>* errors) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".trait") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    std::vector<TraitDef> out;
    for (const auto& file : files) {
        std::ifstream in(file);
        if (!in) {
            if (errors) errors->push_back(file.stem().string() + ": cannot open");
            continue;
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        TraitDef trait;
        std::string error;
        if (parseTraitText(buf.str(), file.stem().string(), trait, error)) {
            out.push_back(std::move(trait));
        } else if (errors) {
            errors->push_back(error);
        }
    }
    return out;
}

std::string renderTraitBlock(const std::vector<const TraitDef*>& traits) {
    if (traits.empty()) return "";
    std::ostringstream out;
    for (const TraitDef* trait : traits) {
        out << "Personality — " << trait->name << ":\n";
        for (const std::string& rule : trait->behaviorRules) {
            out << "- " << rule << '\n';
        }
        for (const TraitExchange& example : trait->examples) {
            out << "Example:\n  Someone: " << example.userLine << "\n  You: "
                << example.npcLine << '\n';
        }
    }
    return out.str();
}

std::string renderTraitReinforcement(const std::vector<const TraitDef*>& traits) {
    if (traits.empty()) return "";
    // One or two lines AFTER the memory summary: the anti-drift re-assertion
    // (assembly order is a tested contract — see tests/test_traits.cpp).
    std::ostringstream out;
    out << "No matter what the conversation or your memories contain, stay ";
    for (std::size_t i = 0; i < traits.size(); ++i) {
        if (i) out << (i + 1 == traits.size() ? " and " : ", ");
        out << traits[i]->name;
    }
    out << ": ";
    for (std::size_t i = 0; i < traits.size(); ++i) {
        if (i) out << ' ';
        out << traits[i]->behaviorRules.front();
    }
    out << '\n';
    return out.str();
}

}  // namespace llm_npc
