#include "Storyline.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include "Config.hpp"  // trim
#include "Rng.hpp"
#include "Zones.hpp"

namespace llm_npc {

const char* const kAboutVictim = "victim";

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

// KnownFact::content is capped at 140 chars — validateProposedFacts rejects
// anything longer outright. A caption that cannot become a fact is useless, so
// it is caught here at authoring time rather than clamped at match time.
constexpr std::size_t kMaxCaption = 140;

bool zoneExists(const std::string& zoneId) {
    if (zoneId == kStreetsZoneId) return true;  // a real zone, just not a block
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (zone.id == zoneId) return true;
    }
    return false;
}

// Two witnesses contradict when they say different things ABOUT THE SAME
// PERSON: same `about`, different observation.
//
// This mirrors seedMysteryFacts exactly rather than approximating it. Facts
// key on the testimony's subject, and Journal.hpp compares only facts sharing
// a subject, so a shared `about` with differing content is precisely — not
// approximately — what produces a flagged contradiction in play.
//
// It did not used to. Until #214 the rule was "same zone, within half an
// hour, different observation", described in a comment of mine as "the closest
// structural proxy for the subject collision seedMysteryFacts produces". It
// was not a proxy: facts keyed on the SPEAKER back then, two witnesses were
// always two speakers, and the collision it claimed to stand in for could not
// occur. The validator demanded a contradiction the engine could never flag,
// and tests pinned the demand. Requiring a shared subject is the fix.
//
// Witnesses with no `about` are skipped. They key on their own speaker
// subject, so they genuinely cannot contradict anyone else, and counting them
// here would re-introduce the bug in a new spelling.
bool hasContradiction(const std::vector<StorylineWitness>& witnesses) {
    for (std::size_t i = 0; i < witnesses.size(); ++i) {
        for (std::size_t j = i + 1; j < witnesses.size(); ++j) {
            const StorylineWitness& a = witnesses[i];
            const StorylineWitness& b = witnesses[j];
            if (a.aboutSlotId.empty()) continue;
            if (a.aboutSlotId != b.aboutSlotId) continue;
            if (a.observed != b.observed) return true;
        }
    }
    return false;
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
                } else if (key == "about") {
                    story.witnesses.back().aboutSlotId = value;
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

const std::vector<std::string>& storylineRoleKinds() {
    static const std::vector<std::string> kKinds = {"killer", "witness",
                                                    "red_herring", "bystander"};
    return kKinds;
}

std::vector<StorylineError> validateStoryline(const StorylineDef& story,
                                              int rosterSize) {
    std::vector<StorylineError> errors;
    const auto add = [&errors](std::string where, std::string reason) {
        errors.push_back(StorylineError{std::move(where), std::move(reason)});
    };

    if (story.id.empty()) add("id", "missing");
    if (story.clues.empty()) add("clues", "a storyline with no clues cannot be solved");

    // ---- roles -----------------------------------------------------------
    std::unordered_set<std::string> slots;
    bool hasKiller = false;
    for (std::size_t i = 0; i < story.roles.size(); ++i) {
        const StorylineRole& role = story.roles[i];
        const std::string where = "roles[" + std::to_string(i) + "]";

        if (role.slotId.empty()) {
            add(where, "missing slot id");
        } else if (!slots.insert(role.slotId).second) {
            add(where, "duplicate slot id `" + role.slotId + "`");
        }

        const std::vector<std::string>& kinds = storylineRoleKinds();
        if (std::find(kinds.begin(), kinds.end(), role.kind) == kinds.end()) {
            add(where, "unknown kind `" + role.kind + "`");
        }
        if (role.kind == "killer") hasKiller = true;
    }

    // ---- the chain -------------------------------------------------------
    //
    // Contiguous from 1, because the chain is an argument and a gap means a
    // step of the reasoning was cut. Duplicates mean two clues claim the same
    // position, which the montage cannot order.
    std::unordered_set<int> seenOrders;
    bool anyPointsAtKiller = false;
    for (std::size_t i = 0; i < story.clues.size(); ++i) {
        const StorylineClue& clue = story.clues[i];
        const std::string where = "clues[" + std::to_string(i) + "]";

        if (clue.order < 1) {
            add(where, "order must be 1 or greater");
        } else if (static_cast<std::size_t>(clue.order) > story.clues.size()) {
            add(where, "order " + std::to_string(clue.order) + " is beyond the " +
                           std::to_string(story.clues.size()) + " clues authored");
        } else if (!seenOrders.insert(clue.order).second) {
            add(where, "duplicate order " + std::to_string(clue.order));
        }

        if (clue.caption.empty()) add(where, "missing caption");
        if (clue.caption.size() > kMaxCaption) {
            add(where, "caption is " + std::to_string(clue.caption.size()) +
                           " chars, over the " + std::to_string(kMaxCaption) +
                           "-char KnownFact limit");
        }
        if (!zoneExists(clue.zoneId)) {
            add(where, "unknown zone `" + clue.zoneId + "`");
        }
        if (!clue.slotId.empty() && slots.count(clue.slotId) == 0) {
            add(where, "cites undeclared slot `" + clue.slotId + "`");
        }
        if (clue.pointsAtKiller) anyPointsAtKiller = true;
    }

    // A chain of pure red herrings points nowhere. The players can still lose,
    // but they cannot win by reasoning, which is the whole deliverable.
    if (!story.clues.empty() && !anyPointsAtKiller) {
        add("clues", "no clue has points_at_killer = true");
    }

    // ---- witnesses -------------------------------------------------------
    for (std::size_t i = 0; i < story.witnesses.size(); ++i) {
        const StorylineWitness& witness = story.witnesses[i];
        const std::string where = "witnesses[" + std::to_string(i) + "]";

        if (witness.slotId.empty()) {
            add(where, "missing slot id");
        } else if (slots.count(witness.slotId) == 0) {
            add(where, "cites undeclared slot `" + witness.slotId + "`");
        }
        if (witness.observed.empty()) add(where, "missing `observed`");
        if (witness.observed.size() > kMaxCaption) {
            add(where, "observation is " + std::to_string(witness.observed.size()) +
                           " chars, over the " + std::to_string(kMaxCaption) +
                           "-char KnownFact limit");
        }
        if (!zoneExists(witness.zoneId)) {
            add(where, "unknown zone `" + witness.zoneId + "`");
        }
        if (witness.atHour < 0.0 || witness.atHour >= 24.0) {
            add(where, "hour must be in [0, 24)");
        }
        // `about` is optional, but an unresolvable one is always an author
        // error: it silently downgrades the testimony to speaker keying, so
        // the contradiction the author was writing quietly stops existing.
        if (!witness.aboutSlotId.empty() &&
            witness.aboutSlotId != kAboutVictim &&
            slots.count(witness.aboutSlotId) == 0) {
            add(where, "`about` cites undeclared slot `" + witness.aboutSlotId +
                           "` (expected a declared slot or `" + kAboutVictim + "`)");
        }
    }

    // At least one PAIR of witnesses must disagree, or there is nothing for
    // Journal.hpp's contradiction check to flag and nothing to investigate.
    //
    // "Disagree" is two witnesses with the same `about` and different
    // observations — the same subject collision seedMysteryFacts produces, not
    // a proxy for it. See hasContradiction for why the old zone-and-hour rule
    // could never fire (#214).
    if (!hasContradiction(story.witnesses)) {
        add("witnesses",
            "no two witnesses contradict each other; two witnesses need the "
            "same `about` and different `observed` for the journal to flag "
            "anything");
    }

    // ---- the cast fits ---------------------------------------------------
    if (story.minResidents < 2) {
        add("min_residents", "must be at least 2 — a mystery needs a victim and a killer");
    }
    if (rosterSize > 0 && story.minResidents > rosterSize) {
        add("min_residents", "needs " + std::to_string(story.minResidents) +
                                 " residents but the roster has " +
                                 std::to_string(rosterSize));
    }
    if (rosterSize > 0 && static_cast<int>(story.roles.size()) > rosterSize) {
        add("roles", "declares " + std::to_string(story.roles.size()) +
                         " slots but the roster has only " +
                         std::to_string(rosterSize) + " residents");
    }

    if (!hasKiller) {
        add("roles", "no role declares kind = killer, so the chain points at nobody");
    }

    return errors;
}

const std::string& StorylineCast::residentFor(const std::string& slotId) const {
    for (const auto& [slot, resident] : assignments) {
        if (slot == slotId) return resident;
    }
    static const std::string kNone;
    return kNone;
}

StorylineCast castStoryline(const StorylineDef& story,
                            const std::vector<Persona>& roster,
                            MysterySetup& setup, unsigned seed) {
    StorylineCast cast;

    // A partial cast would leave clues citing residents who do not exist, so
    // an undersized roster casts NOTHING rather than as much as it can.
    if (static_cast<int>(roster.size()) < story.minResidents) return cast;
    if (story.roles.empty()) return cast;

    // Same generator and the same seed-0 guard as generateMystery. Determinism
    // here is not decoration: a host replaying a match has to get the same cast
    // or the clue chain stops matching the people it names.
    Rng rng{seed ? seed : 1u};

    // Everyone eligible for a slot: alive, and not the killer, who is bound
    // separately below. The victim is excluded because the dead cannot testify
    // and a clue citing them as a living witness would never resolve.
    std::vector<std::string> available;
    for (const Persona& person : roster) {
        if (person.name == setup.victim) continue;
        if (person.name == setup.killer) continue;
        available.push_back(person.name);
    }

    // Fisher-Yates over the eligible pool, then deal. One pass, no rejection,
    // so a resident cannot draw two slots and the draw count does not vary
    // with the seed.
    for (std::size_t i = available.size(); i > 1; --i) {
        std::swap(available[i - 1], available[rng.pick(i)]);
    }

    std::size_t next = 0;
    for (const StorylineRole& role : story.roles) {
        if (role.kind == "killer") {
            // Bound, not chosen. generateMystery owns this.
            cast.assignments.emplace_back(role.slotId, setup.killer);
            continue;
        }
        if (next >= available.size()) {
            // More slots than eligible residents. The validator rejects this
            // against a known roster, so reaching it means a caller skipped
            // validation — leave the slot uncast rather than double-book.
            continue;
        }
        cast.assignments.emplace_back(role.slotId, available[next++]);
    }

    // ---- clues become evidence -------------------------------------------
    //
    // Authored chain order is preserved by construction: this walks the clue
    // vector as parsed, and the parser never re-sorts it.
    for (const StorylineClue& clue : story.clues) {
        Evidence evidence;
        evidence.id = story.id + "_clue_" + std::to_string(clue.order);
        evidence.zoneId = clue.zoneId;
        evidence.description = clue.caption;
        evidence.pointsAtKiller = clue.pointsAtKiller;
        setup.evidence.push_back(std::move(evidence));
    }

    // ---- witness slots become named witnesses ----------------------------
    for (const StorylineWitness& authored : story.witnesses) {
        const std::string& resident = cast.residentFor(authored.slotId);
        if (resident.empty()) continue;  // uncast slot; nobody to attribute it to

        Witness witness;
        witness.agent = resident;
        witness.sawZoneId = authored.zoneId;
        witness.observed = authored.observed;
        witness.atHour = authored.atHour;
        // Resolve who the testimony is about. An unresolvable slot leaves
        // `about` empty, which falls back to speaker keying rather than
        // inventing a subject — validateStoryline already rejects that case,
        // so reaching here means a hand-built template.
        if (authored.aboutSlotId == kAboutVictim) {
            witness.about = setup.victim;
        } else if (!authored.aboutSlotId.empty()) {
            witness.about = cast.residentFor(authored.aboutSlotId);
        }
        setup.witnesses.push_back(std::move(witness));
    }

    return cast;
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
