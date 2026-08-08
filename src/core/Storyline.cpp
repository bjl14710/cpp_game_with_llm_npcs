#include "Storyline.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "Config.hpp"  // trim

namespace llm_npc {
namespace {

// Splits "key = value". Returns false for a blank line, a comment, or a line
// with no '='.
//
// A '#' only starts a comment when it is the FIRST non-space character — the
// same departure from Config.cpp's readKv that LineBank.cpp makes, and for the
// same reason. Clue captions are prose: "Two cups on the counter, table #3"
// has to survive.
bool splitKeyValue(const std::string& raw, std::string& key, std::string& value) {
    const std::string line = trim(raw);
    if (line.empty() || line.front() == '#') return false;
    const auto eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    value = trim(line.substr(eq + 1));
    return !key.empty();
}

// Which section the following indented keys belong to. The format is flat text
// with section-opening keys, exactly like banks/*.bank's `topic =`.
enum class Section { None, Role, Clue, Witness };

bool parseBool(const std::string& text) {
    return text == "true" || text == "yes" || text == "1";
}

}  // namespace

std::optional<StorylineDef> parseStorylineFile(const std::filesystem::path& path,
                                               std::vector<std::string>* errors) {
    const std::string name = path.filename().string();
    const auto fail = [&](const std::string& why) -> std::optional<StorylineDef> {
        if (errors) errors->push_back(name + ": " + why);
        return std::nullopt;
    };

    std::ifstream in(path);
    if (!in) return fail("cannot be read");

    StorylineDef story;
    std::vector<std::string> localErrors;
    Section section = Section::None;

    std::string raw, key, value;
    while (std::getline(in, raw)) {
        if (!splitKeyValue(raw, key, value)) continue;

        // --- section openers ------------------------------------------------
        if (key == "role") {
            story.roles.push_back(StorylineRole{value, "", ""});
            section = Section::Role;
            continue;
        }
        if (key == "clue") {
            // std::atoi rather than std::stoi: a non-numeric order yields 0,
            // which the validator reports as "order must be 1 or greater".
            // stoi would throw, and a parser that throws on a typo violates
            // the degrade-to-inert contract this file is built around.
            StorylineClue clue;
            clue.order = std::atoi(value.c_str());
            story.clues.push_back(clue);
            section = Section::Clue;
            continue;
        }
        if (key == "witness") {
            StorylineWitness witness;
            witness.slotId = value;
            story.witnesses.push_back(witness);
            section = Section::Witness;
            continue;
        }

        // --- file-scope keys --------------------------------------------------
        if (key == "id") {
            story.id = value;
            section = Section::None;
            continue;
        }
        if (key == "title") {
            story.title = value;
            section = Section::None;
            continue;
        }
        if (key == "min_residents") {
            story.minResidents = std::atoi(value.c_str());
            section = Section::None;
            continue;
        }

        // --- section-scoped keys ----------------------------------------------
        switch (section) {
            case Section::Role:
                if (key == "kind") {
                    story.roles.back().kind = value;
                } else if (key == "note") {
                    story.roles.back().note = value;
                } else {
                    localErrors.push_back(name + ": unknown role key `" + key + "`");
                }
                break;

            case Section::Clue:
                if (key == "zone") {
                    story.clues.back().zoneId = value;
                } else if (key == "caption") {
                    story.clues.back().caption = value;
                } else if (key == "slot") {
                    story.clues.back().slotId = value;
                } else if (key == "points_at_killer") {
                    story.clues.back().pointsAtKiller = parseBool(value);
                } else {
                    localErrors.push_back(name + ": unknown clue key `" + key + "`");
                }
                break;

            case Section::Witness:
                if (key == "zone") {
                    story.witnesses.back().zoneId = value;
                } else if (key == "observed") {
                    story.witnesses.back().observed = value;
                } else if (key == "hour") {
                    story.witnesses.back().atHour = std::atof(value.c_str());
                } else {
                    localErrors.push_back(name + ": unknown witness key `" + key + "`");
                }
                break;

            case Section::None:
                localErrors.push_back(name + ": `" + key +
                                      "` before any `role`, `clue` or `witness`");
                break;
        }
    }

    // Checked before anything else so a wholly unusable file reports one clear
    // reason rather than a cascade — same ordering LineBank uses.
    if (story.id.empty()) return fail("no `id` key");
    if (story.clues.empty()) return fail("no clues");

    if (errors) errors->insert(errors->end(), localErrors.begin(), localErrors.end());
    return story;
}

std::vector<StorylineDef> loadStorylines(const std::filesystem::path& dir,
                                         std::vector<std::string>* errors) {
    std::vector<StorylineDef> stories;

    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.path().extension() == ".storyline") files.push_back(entry.path());
    }
    if (ec) {
        // One line, then behave as if there were no storylines at all. Same
        // contract as ConversationStore, RatingLog and LineBank: a broken
        // content directory must leave the game no worse off than an absent
        // one.
        std::cerr << "[llm_npc] storylines: cannot read " << dir
                  << " — authored mysteries disabled this session\n";
        return stories;
    }

    // directory_iterator order is unspecified; sort so load order and any error
    // ordering are reproducible across machines.
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path& file : files) {
        // One bad template must not take the others with it — a typo in a
        // fixture should cost that fixture, not the whole mode.
        if (std::optional<StorylineDef> story = parseStorylineFile(file, errors)) {
            stories.push_back(std::move(*story));
        }
    }
    return stories;
}

}  // namespace llm_npc
