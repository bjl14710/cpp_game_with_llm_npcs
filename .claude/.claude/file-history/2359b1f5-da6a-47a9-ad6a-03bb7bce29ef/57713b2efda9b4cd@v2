#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "LlmClient.hpp"
#include "Math.hpp"
#include "Persona.hpp"

namespace llm_npc {

// Combat state machine for an NPC. Civilians flee when damaged; armed NPCs
// (cops) turn Hostile and fight back.
enum class NpcState {
    Idle,     // standing at spawn point, willing to chat
    Fleeing,  // moving away from the player at flee speed
    Hostile,  // armed NPC: moving to preferred range and firing
    Dead,     // hp reached 0; rendered as collapsed, no further interaction
};

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

    // --- Combat interface ------------------------------------------------

    // Reduces hp by `amount`, clamps to [0, hpMax]. Transitions state:
    //   - Armed NPC  → Hostile
    //   - Unarmed NPC → Fleeing
    //   - hp == 0    → Dead (regardless of armed status)
    // No-op if already Dead.
    // TODO(combat): implement in Npc.cpp
    void takeDamage(int amount);

    // Seconds remaining before this NPC can fire again (armed only).
    // Decremented by World::updateCombat each frame.
    // TODO(combat): implement fire cooldown tick in World::updateCombat
    float fireCooldown = 0.f;

    // Combat state accessors.
    NpcState    combatState()  const { return state_; }
    int         hp()           const { return hp_; }
    bool        isArmed()      const { return persona_.armed; }

    // World-space feet position (writable so combat can move fleeing NPCs).
    Vec3& position() { return position_; }

    // ---- Read-only accessors --------------------------------------------

    const Vec3& position() const { return position_; }
    float facingDeg() const { return facingDeg_; }
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

    // Combat state — all zero/Idle by default.
    int      hp_    = 100;
    int      hpMax_ = 100;
    NpcState state_ = NpcState::Idle;

    void trimHistory();
};

}  // namespace llm_npc
