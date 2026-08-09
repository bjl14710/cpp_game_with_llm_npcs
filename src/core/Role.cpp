#include "Role.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "Config.hpp"  // trim

namespace llm_npc {

namespace {

// Parses one .role file's text. Deliberately the same contract as
// parseTraitText: key=value lines, unknown keys are named errors, examples
// arrive as example_user/example_npc PAIRS in order.
bool parseRoleText(const std::string& text, const std::string& id, RoleDef& out,
                   std::string& error) {
    RoleDef role;
    role.id = id;
    std::istringstream in(text);
    std::string line;
    std::string pendingUser;
    bool hasPendingUser = false;
    // Tracked separately from `role.demeanour.empty()` so `demeanour =` with a
    // blank value reports the same failure as omitting the key. Both are the
    // same authoring mistake; only one of them looks like an answer.
    bool sawDemeanour = false;

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
            role.name = val;
        } else if (key == "directive") {
            if (val.empty()) {
                error = id + ": empty directive";
                return false;
            }
            role.directives.push_back(val);
        } else if (key == "demeanour") {
            sawDemeanour = true;
            role.demeanour = val;
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
            role.examples.push_back({pendingUser, val});
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
    if (role.name.empty()) {
        error = id + ": missing required 'name'";
        return false;
    }
    if (role.directives.empty()) {
        error = id + ": a role needs at least one directive";
        return false;
    }

    // THE RULE THAT IS NOT IN Trait.cpp, and the reason it is a hard failure:
    // demeanour is the anti-tell line. Without an explicit instruction to hold
    // baseline warmth, the model was MEASURED to default to hostile — guilty
    // Marge goes [[MOOD: angry]] on turn one where innocent Marge laughs — and
    // mood drives the rendered face. A role missing this passes every unit test
    // and breaks the mystery in play, which is precisely why it cannot be a
    // warning.
    if (!sawDemeanour) {
        error = id + ": missing required 'demeanour' — the anti-tell line; "
                     "without it the model defaults to hostile and the mood tag "
                     "identifies the killer";
        return false;
    }
    if (role.demeanour.empty()) {
        error = id + ": empty 'demeanour' — a blank value is not an anti-tell "
                     "line";
        return false;
    }

    out = std::move(role);
    return true;
}

}  // namespace

std::vector<RoleDef> loadAllRoles(const std::filesystem::path& dir,
                                  std::vector<std::string>* errors) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".role") {
            files.push_back(entry.path());
        }
    }
    // directory_iterator order is unspecified; sort so load order and any error
    // ordering are reproducible across machines.
    std::sort(files.begin(), files.end());

    std::vector<RoleDef> out;
    for (const auto& file : files) {
        std::ifstream in(file);
        if (!in) {
            if (errors) errors->push_back(file.stem().string() + ": cannot open");
            continue;
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        RoleDef role;
        std::string error;
        // One bad file costs that file, not the whole cast — the same contract
        // loadAllTraits and loadAllPersonas hold.
        if (parseRoleText(buf.str(), file.stem().string(), role, error)) {
            out.push_back(std::move(role));
        } else if (errors) {
            errors->push_back(error);
        }
    }
    return out;
}

std::string renderRoleBlock(const RoleDef* role, const std::string& secret) {
    // Every persona in the game today is role-less, so this early return is
    // what makes the feature invisible until it is used. A test compares the
    // rendered prompt byte for byte.
    if (role == nullptr) return "";

    std::ostringstream out;
    // Shaped on renderTraitBlock so the two sections read as one voice rather
    // than as two systems talking past each other.
    out << "In this story you are " << role->name << ":\n";
    for (const std::string& directive : role->directives) {
        out << "- " << directive << '\n';
    }

    // The secret goes in as prose, unlabelled. Writing `secret: <text>` would
    // hand the model a token to echo back, and an acceptance criterion says
    // neither the role id nor the raw secret may appear verbatim in a
    // transcript.
    if (!secret.empty()) {
        out << "- " << secret << '\n';
    }

    // Last inside the block, because it is the instruction most at risk of
    // being ignored and the file's own convention is that trailing lines carry
    // most weight with small models.
    out << role->demeanour << '\n';

    for (const TraitExchange& example : role->examples) {
        out << "Example:\n  Someone: " << example.userLine << "\n  You: "
            << example.npcLine << '\n';
    }
    return out.str();
}

const RoleDef* findRole(const std::vector<RoleDef>& roles, const std::string& id) {
    // Real, not a stub: a linear scan over four roles is not a search problem,
    // and having it work from the first commit means step 4 can wire an NPC's
    // assigned role without waiting on the parser.
    for (const RoleDef& role : roles) {
        if (role.id == id) return &role;
    }
    return nullptr;
}

}  // namespace llm_npc
