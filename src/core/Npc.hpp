#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "LlmClient.hpp"
#include "Math.hpp"
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

    // Places the NPC in the world: feet position, facing in degrees (0 looks
    // toward +Z), and the id of the building/prop they belong to.
    void setPlacement(const Vec3& position, float facingDeg, std::string spotId) {
        position_ = position;
        facingDeg_ = facingDeg;
        spotId_ = std::move(spotId);
    }

    // World-space feet position.
    const Vec3& position() const { return position_; }

    // Facing direction in degrees; 0 looks toward +Z.
    float facingDeg() const { return facingDeg_; }

    // Id of the building or prop this NPC stands at ("bakery", "bench", ...).
    const std::string& spotId() const { return spotId_; }

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

    Vec3 position_{};
    float facingDeg_ = 0.f;
    std::string spotId_;

    void trimHistory();
};

}  // namespace llm_npc
