#include "GroupSession.hpp"

#include <algorithm>
#include <cctype>

namespace llm_npc {

namespace {

// Case-insensitive prefix match at the START of the player's message —
// "Marge, what do you think?" addresses Marge. Only a leading name counts;
// mid-sentence mentions are conversation, not addressing.
bool startsWithName(const std::string& message, const std::string& name) {
    if (name.empty() || message.size() < name.size()) return false;
    for (std::size_t i = 0; i < name.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(message[i])) !=
            std::tolower(static_cast<unsigned char>(name[i]))) {
            return false;
        }
    }
    // The name must end at a word boundary (comma, space, end, punctuation).
    if (message.size() == name.size()) return true;
    const char next = message[name.size()];
    return !std::isalnum(static_cast<unsigned char>(next));
}

}  // namespace

void GroupSession::open(std::vector<int> npcIndices,
                        std::vector<std::string> names) {
    participants_ = std::move(npcIndices);
    names_ = std::move(names);
    transcript_.clear();
    consecutiveNpcTurns_ = 0;
    roundRobinCursor_ = 0;
    active_ = participants_.size() >= 2;
}

void GroupSession::close() {
    active_ = false;
    participants_.clear();
    names_.clear();
    transcript_.clear();
    consecutiveNpcTurns_ = 0;
    roundRobinCursor_ = 0;
}

bool GroupSession::active() const { return active_; }

const std::vector<int>& GroupSession::participants() const { return participants_; }

const std::string& GroupSession::nameOf(int npcIndex) const {
    static const std::string unknown = "?";
    for (std::size_t i = 0; i < participants_.size(); ++i) {
        if (participants_[i] == npcIndex) return names_[i];
    }
    return unknown;
}

void GroupSession::removeParticipant(int npcIndex) {
    for (std::size_t i = 0; i < participants_.size(); ++i) {
        if (participants_[i] != npcIndex) continue;
        participants_.erase(participants_.begin() + static_cast<long>(i));
        names_.erase(names_.begin() + static_cast<long>(i));
        break;
    }
    if (roundRobinCursor_ >= static_cast<int>(participants_.size())) {
        roundRobinCursor_ = 0;
    }
    if (participants_.empty()) active_ = false;
}

const std::vector<GroupLine>& GroupSession::transcript() const { return transcript_; }

void GroupSession::addLine(const std::string& speaker, const std::string& text) {
    transcript_.push_back({speaker, text});
}

std::string GroupSession::renderTranscript() const {
    std::string out;
    for (const GroupLine& line : transcript_) {
        out += line.speaker + ": " + line.text + "\n";
    }
    return out;
}

int GroupSession::resolveNextSpeaker(const std::string& playerMessage) {
    if (participants_.empty()) return -1;
    // Addressed-by-name wins; FULL name beats first name on ties, so check
    // longest names first.
    int addressed = -1;
    std::size_t addressedLen = 0;
    for (std::size_t i = 0; i < participants_.size(); ++i) {
        const std::string& full = names_[i];
        const std::string first = full.substr(0, full.find(' '));
        if (startsWithName(playerMessage, full) && full.size() > addressedLen) {
            addressed = participants_[i];
            addressedLen = full.size();
        } else if (addressed == -1 && startsWithName(playerMessage, first)) {
            addressed = participants_[i];
            addressedLen = first.size();
        }
    }
    if (addressed != -1) {
        // Keep the rotation continuing AFTER the addressed speaker.
        for (std::size_t i = 0; i < participants_.size(); ++i) {
            if (participants_[i] == addressed) {
                roundRobinCursor_ = static_cast<int>((i + 1) % participants_.size());
            }
        }
        return addressed;
    }
    const int speaker = participants_[static_cast<std::size_t>(roundRobinCursor_)];
    roundRobinCursor_ = (roundRobinCursor_ + 1) % static_cast<int>(participants_.size());
    return speaker;
}

bool GroupSession::npcMayTakeFloor() const {
    return active_ && participants_.size() >= 2 &&
           consecutiveNpcTurns_ < kMaxConsecutiveNpcTurns;
}

void GroupSession::noteNpcTurn() { ++consecutiveNpcTurns_; }

void GroupSession::notePlayerTurn() { consecutiveNpcTurns_ = 0; }

}  // namespace llm_npc
