#pragma once

#include <string>
#include <vector>

namespace llm_npc {

// Turn-based group conversation state (plan: group-conversations): a
// participant list over the SAME structured-JSON dialogue pipeline as solo
// talk. Solo DialogueSession is deliberately untouched — group is additive,
// and main.cpp dispatches to whichever fits the situation. All state here
// is renderer-free and unit-testable without a model.
struct GroupLine {
    // "Player" or the participant's persona name — transcripts are always
    // speaker-labeled, both for the UI and for per-speaker prompt assembly.
    std::string speaker;
    std::string text;
};

class GroupSession {
   public:
    // Opens with the npc indices present and their display names (the
    // player is implicit). Fewer than two NPCs is not a group.
    void open(std::vector<int> npcIndices, std::vector<std::string> names);
    void close();
    bool active() const;
    const std::vector<int>& participants() const;
    const std::string& nameOf(int npcIndex) const;

    // Drops a participant (death, arrest, dismissal); closes when empty.
    void removeParticipant(int npcIndex);

    // The labeled transcript, and its rendering for per-speaker prompts
    // ("Name: line\n" per entry — the last other-speaker line is the
    // user-turn input for whoever speaks next).
    const std::vector<GroupLine>& transcript() const;
    void addLine(const std::string& speaker, const std::string& text);
    std::string renderTranscript() const;

    // Turn resolution: a player message with a LEADING participant name
    // ("Marge, ..."; full-name beats first-name on ties) gives that NPC
    // the turn and the rotation continues after them; otherwise
    // round-robin. After an NPC reply, the floor may pass NPC-to-NPC at
    // most kMaxConsecutiveNpcTurns times before returning to the player.
    static constexpr int kMaxConsecutiveNpcTurns = 2;
    int resolveNextSpeaker(const std::string& playerMessage);
    bool npcMayTakeFloor() const;
    void noteNpcTurn();
    void notePlayerTurn();

   private:
    std::vector<int> participants_;
    std::vector<std::string> names_;  // parallel to participants_
    std::vector<GroupLine> transcript_;
    int consecutiveNpcTurns_ = 0;
    int roundRobinCursor_ = 0;
    bool active_ = false;
};

}  // namespace llm_npc
