#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "City.hpp"
#include "LlmClient.hpp"
#include "Math.hpp"
#include "NpcAction.hpp"
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
    // toward +Z), and the id of the building/prop they belong to. The spot is
    // remembered as "home" so ReturnHome can walk back to it later.
    void setPlacement(const Vec3& position, float facingDeg, std::string spotId) {
        position_ = position;
        facingDeg_ = facingDeg;
        homePosition_ = position;
        homeFacingDeg_ = facingDeg;
        spotId_ = std::move(spotId);
    }

    // World-space feet position.
    const Vec3& position() const { return position_; }

    // Facing direction in degrees; 0 looks toward +Z.
    float facingDeg() const { return facingDeg_; }

    // Id of the building or prop this NPC stands at ("bakery", "bench", ...).
    const std::string& spotId() const { return spotId_; }

    // Turns immediately to face `target` (e.g. the player starting a chat)
    // without changing the current behavior.
    void lookAt(const Vec3& target) { faceToward(target); }

    // Advances world behavior by `dt` seconds: follows/chases the player and
    // turns to face them per the current behavior, sliding around buildings
    // via `city`, and counts down any active gesture pose. Pure game logic
    // (no LLM, no graphics) so it stays unit-testable.
    void update(float dt, const Vec3& playerPos, const City& city);

    // The persistent movement behavior set by the last obeyed instruction.
    NpcAction behavior() const { return behavior_; }

    // The transient gesture currently being shown (None when idle).
    NpcAction pose() const { return pose_; }

    // Seconds since the active gesture began; drives wave animation phase.
    float gesturePhase() const { return gesturePhase_; }

    // A stable per-NPC seed for the renderer's procedural appearance (clothing,
    // skin, hair, build). Derived from the persona name at construction.
    std::uint32_t appearanceSeed() const { return appearanceSeed_; }

    // Accumulated stride phase (radians) for the renderer's walk cycle, and
    // whether the NPC moved on the last update (drives the leg/arm swing).
    float locomotionPhase() const { return locomotionPhase_; }
    bool isMoving() const { return moving_; }

    // True once an Arrest behavior has reached the player. Latches until the
    // NPC is given a different instruction; lets the UI announce the catch.
    bool hasCaughtPlayer() const { return caughtPlayer_; }

    // The action parsed from the most recent reply (None if the NPC didn't act
    // this turn). Lets the UI show a stage direction even when the model spoke
    // no words alongside its action tag.
    NpcAction lastAction() const { return lastAction_; }

    // The charge parsed from the most recent reply's [[CHARGE: ...]] tag, when
    // an arresting officer named one (Charge::None otherwise). Meaningful only
    // for the turn it arrives, like lastAction().
    Charge lastCharge() const { return lastCharge_; }

    // The emotional state read from the latest reply; decays back to Neutral
    // after a while. Drives the rendered facial expression.
    NpcMood mood() const { return mood_; }

    // Orders this NPC (a summoned police officer) to chase the player down,
    // exactly as if its own reply had carried an arrest directive.
    void commandArrest() {
        behavior_ = NpcAction::Arrest;
        caughtPlayer_ = false;
    }

    // Sends the NPC walking back to its spawn spot (e.g. a cop returning to
    // post after an arrest). Clears the catch latch.
    void commandReturnHome() {
        behavior_ = NpcAction::ReturnHome;
        caughtPlayer_ = false;
    }

    // Starts the NPC strolling between random nearby spots: the ambient state
    // for street pedestrians. The first goal is chosen lazily on the next
    // update(), where the city is available for bounds checking.
    void commandWander() {
        behavior_ = NpcAction::Wander;
        caughtPlayer_ = false;
    }

    // Marks whether the player is mid-conversation with this NPC, so a wanderer
    // pauses and faces the player instead of strolling off while they talk.
    void setInConversation(bool talking) { inConversation_ = talking; }

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

    NpcAction behavior_ = NpcAction::None;    // persistent: follow/arrest/stop/face
    NpcAction pose_ = NpcAction::None;        // transient gesture overlay
    NpcAction lastAction_ = NpcAction::None;  // action from the latest reply
    Charge lastCharge_ = Charge::None;        // charge from the latest reply
    NpcMood mood_ = NpcMood::Neutral;         // current expression
    float moodTimer_ = 0.f;                   // seconds until mood relaxes
    float poseTimer_ = 0.f;                   // seconds of gesture remaining
    float gesturePhase_ = 0.f;                // seconds elapsed in current gesture
    float locomotionPhase_ = 0.f;             // accumulated stride phase (radians)
    bool moving_ = false;                     // moved on the last update tick
    std::uint32_t appearanceSeed_ = 1u;       // stable per-NPC appearance seed
    bool caughtPlayer_ = false;               // arrest reached the player
    Vec3 homePosition_{};                     // spawn spot for ReturnHome
    float homeFacingDeg_ = 0.f;               // spawn facing for ReturnHome
    Vec3 wanderTarget_{};                     // current stroll goal (Wander)
    float wanderPauseTimer_ = 0.f;            // idle seconds before a new goal
    std::uint32_t wanderRng_ = 1u;            // per-NPC RNG state for wandering
    bool wanderReady_ = false;                // first wander target chosen yet?
    bool inConversation_ = false;             // player is mid-chat with this NPC

    // Routes a freshly parsed action into behavior/gesture state.
    void applyAction(NpcAction action);
    void faceToward(const Vec3& target);
    void trimHistory();

    // Chooses a new random wander goal near the current position, clamped just
    // inside the walkable world.
    void pickWanderTarget(const City& city);
    // Next pseudo-random float in [0, 1) from wanderRng_ (no <random> needed).
    float nextRandUnit();
};

}  // namespace llm_npc
