#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "LlmClient.hpp"
#include "Persona.hpp"

namespace llm_npc {

// An NPC that delegates dialogue to the shared LlmClient. Holds its own persona
// and a bounded conversation history. Many NPCs can share one LlmClient.
class Npc {
   public:
    Npc(Persona persona, LlmClient& client, int maxHistoryTurns = 10);

    // Sends the player's line to the LLM. Returns the in-flight request id.
    // The reply will appear in client.drainReplies() with that id; pass it back
    // into onReplyArrived() to update this NPC's history and surface text.
    std::uint64_t ask(const std::string& playerLine);

    // Notify this NPC of a reply. Returns the assistant text if the id matches
    // a pending request from this NPC; nullopt otherwise. On success, the turn
    // is appended to history (capped to maxHistoryTurns_).
    std::optional<std::string> onReplyArrived(const ChatReply& reply);

    const Persona& persona() const { return persona_; }
    const std::vector<ChatTurn>& history() const { return history_; }
    bool waiting() const { return pendingId_ != 0; }

   private:
    Persona persona_;
    LlmClient& client_;
    int maxHistoryTurns_;
    std::vector<ChatTurn> history_;
    std::uint64_t pendingId_ = 0;
    std::string pendingUserLine_;

    void trimHistory();
};

}  // namespace llm_npc
