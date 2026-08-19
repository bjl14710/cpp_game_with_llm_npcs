#include "LineBank.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "Config.hpp"  // trim

namespace llm_npc {
namespace {

// Splits "key = value" into its two halves. Returns false for a line that is
// blank, a comment, or has no '='.
//
// A '#' only starts a comment when it is the FIRST non-space character.
// Config.cpp's readKv strips from any '#', which is right for settings but
// wrong here: reply text is prose and "Room #3 is upstairs." must survive.
bool splitKeyValue(const std::string& raw, std::string& key, std::string& value) {
    const std::string line = trim(raw);
    if (line.empty() || line.front() == '#') return false;
    const auto eq = line.find('=');
    if (eq == std::string::npos) return false;
    key = trim(line.substr(0, eq));
    value = trim(line.substr(eq + 1));
    return !key.empty();
}

Familiarity familiarityFromText(const std::string& text, bool& known) {
    known = true;
    if (text == "any") return Familiarity::Any;
    if (text == "first") return Familiarity::First;
    if (text == "returning") return Familiarity::Returning;
    known = false;
    return Familiarity::Any;
}

// A topic declaring Any serves either caller. lookup() is only ever passed
// First or Returning, never Any.
bool servesFamiliarity(Familiarity topic, Familiarity asked) {
    return topic == Familiarity::Any || topic == asked;
}

std::string rotationKey(const std::string& speakerId, const std::string& topicId) {
    return speakerId + '\x1f' + topicId;  // unit separator: never in a name
}

}  // namespace

std::optional<PersonaBank> parseBankFile(const std::filesystem::path& path,
                                         std::vector<std::string>* errors) {
    const std::string name = path.filename().string();
    const auto fail = [&](const std::string& why) {
        if (errors) errors->push_back(name + ": " + why);
        return std::nullopt;
    };

    std::ifstream in(path);
    if (!in) return fail("cannot be read");

    PersonaBank bank;
    std::vector<std::string> localErrors;
    std::string raw, key, value;
    while (std::getline(in, raw)) {
        if (!splitKeyValue(raw, key, value)) continue;

        if (key == "persona") {
            bank.speakerId = value;
        } else if (key == "topic") {
            bank.topics.push_back(BankedTopic{value, Familiarity::Any, {}, {}});
        } else if (bank.topics.empty()) {
            localErrors.push_back(name + ": `" + key + "` before any `topic`");
        } else if (key == "familiarity") {
            bool known = false;
            bank.topics.back().familiarity = familiarityFromText(value, known);
            if (!known) {
                localErrors.push_back(name + ": topic `" + bank.topics.back().topicId +
                                      "` has unknown familiarity `" + value + "`");
            }
        } else if (key == "trigger") {
            bank.topics.back().triggers.push_back(value);
        } else if (key == "reply") {
            bank.topics.back().replies.push_back(value);
        } else {
            localErrors.push_back(name + ": unknown key `" + key + "`");
        }
    }

    // Checked before the per-topic pass so a wholly unusable file reports one
    // clear reason rather than a cascade.
    if (bank.speakerId.empty()) return fail("no `persona` key");

    // A topic with no triggers can never be reached; one with no replies has
    // nothing to serve. Either way it is dead weight, so drop it and say so.
    const auto dead = [](const BankedTopic& t) {
        return t.triggers.empty() || t.replies.empty();
    };
    for (const BankedTopic& topic : bank.topics) {
        if (dead(topic)) {
            localErrors.push_back(name + ": topic `" + topic.topicId +
                                  "` has no triggers or no replies");
        }
    }
    bank.topics.erase(std::remove_if(bank.topics.begin(), bank.topics.end(), dead),
                      bank.topics.end());

    if (bank.topics.empty()) {
        // Only add a reason if nothing else already explained it, so the error
        // count stays one-per-problem.
        if (localErrors.empty()) return fail("no topics");
        if (errors) errors->insert(errors->end(), localErrors.begin(), localErrors.end());
        return std::nullopt;
    }

    if (errors) errors->insert(errors->end(), localErrors.begin(), localErrors.end());
    return bank;
}

LineBank::LineBank(std::filesystem::path dir, float threshold)
    : dir_(std::move(dir)), threshold_(threshold) {
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (entry.path().extension() == ".bank") files.push_back(entry.path());
    }
    if (ec) {
        // One line, then behave as if there were no bank at all. Same contract
        // as ConversationStore and RatingLog: a broken bank must leave the game
        // no worse off than not having one.
        std::cerr << "[llm_npc] line bank: cannot read " << dir_
                  << " — banked replies disabled this session\n";
        return;
    }

    // directory_iterator order is unspecified; sort so load order, and any
    // error ordering, is reproducible across machines.
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        if (auto bank = parseBankFile(file, &errors_)) banks_.push_back(std::move(*bank));
    }
}

std::optional<std::string> LineBank::lookup(const std::string& speakerId,
                                            Familiarity familiarity,
                                            const std::string& playerLine) {
    const auto isSpeaker = [&](const PersonaBank& b) { return b.speakerId == speakerId; };
    const auto bank = std::find_if(banks_.begin(), banks_.end(), isSpeaker);
    if (bank == banks_.end()) {
        ++stats_.misses;
        return std::nullopt;
    }

    std::vector<TopicTriggers> candidates;
    candidates.reserve(bank->topics.size());
    for (const BankedTopic& topic : bank->topics) {
        if (servesFamiliarity(topic.familiarity, familiarity)) {
            candidates.push_back({topic.topicId, topic.triggers});
        }
    }

    const TopicMatch match = bestTopic(playerLine, candidates, threshold_);
    if (match.topicId.empty()) {
        ++stats_.misses;
        return std::nullopt;
    }

    const auto hit = std::find_if(
        bank->topics.begin(), bank->topics.end(),
        [&](const BankedTopic& t) { return t.topicId == match.topicId; });
    if (hit == bank->topics.end() || hit->replies.empty()) {
        ++stats_.misses;  // unreachable via parseBankFile, but never serve nothing
        return std::nullopt;
    }

    // Round-robin from a per-session cursor. An NPC that answers "hello"
    // identically twice in one conversation is worse than a slow one, so a
    // variant only repeats once every other variant has been used.
    std::size_t& cursor = nextVariant_[rotationKey(speakerId, match.topicId)];
    const std::string& reply = hit->replies[cursor % hit->replies.size()];
    cursor = (cursor + 1) % hit->replies.size();

    ++stats_.hits;
    return reply;
}

void LineBank::resetSession() {
    nextVariant_.clear();
}

}  // namespace llm_npc
