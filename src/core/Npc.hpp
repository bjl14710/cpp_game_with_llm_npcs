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
#include "Role.hpp"
#include "Schedule.hpp"

namespace llm_npc {

// Combat state machine for an NPC. Civilians flee when damaged; armed NPCs
// (cops) turn Hostile and fight back. Orthogonal to the conversational
// behavior_ system — a Fleeing NPC still has a mood, but combat movement
// overrides behavior movement while active (World::updateCombat).
enum class NpcState {
    Idle,     // default; conversation and behaviors work normally
    Fleeing,  // moving away from the player at flee speed
    Hostile,  // armed NPC: moving to preferred range and firing
    Dead,     // hp reached 0; rendered collapsed, no further interaction
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
    //
    // `timeOfDayHours` is the SHARED world clock (World::state()) injected
    // per tick — the NPC keeps no clock of its own. Pass a negative value
    // (the default) to disable schedule behavior (tests, callers without a
    // clock); everything else behaves as before.
    void update(float dt, const Vec3& playerPos, const City& city,
                float timeOfDayHours = -1.f);

    // Daily routine (from the persona file). Empty = stand at home.
    void setSchedule(std::vector<ScheduleEntry> schedule) {
        schedule_ = std::move(schedule);
    }

    // A TEMPORARY destination that outranks the schedule (issue #156).
    //
    // Set while the town assembles in the plaza for the vote; cleared on the
    // Resolution -> Investigation rollover, which puts everyone straight back
    // on their authored day. It composes ON TOP of the schedule rather than
    // replacing it: personas/*.persona is never rewritten, `activity()` still
    // reports whatever the clock says the resident would otherwise be doing,
    // and clearing the override needs no memory of what was there before.
    //
    // It moves the NPC and nothing else. The vote itself must never depend on
    // the walk succeeding — a resident who cannot reach the plaza votes from
    // where they stand — so no other system reads this.
    void setGatherTarget(const Vec3& target) { gatherTarget_ = target; }
    void clearGatherTarget() { gatherTarget_.reset(); }
    const std::optional<Vec3>& gatherTarget() const { return gatherTarget_; }

    // The active schedule entry's activity label at the last update
    // ("baking bread"); empty when nothing is scheduled. Shown in the
    // nameplate.
    const std::string& activity() const { return activity_; }

    // The persistent movement behavior set by the last obeyed instruction.
    NpcAction behavior() const { return behavior_; }

    // The transient gesture currently being shown (None when idle).
    NpcAction pose() const { return pose_; }

    // Seconds since the active gesture began; drives wave animation phase.
    float gesturePhase() const { return gesturePhase_; }

    // True once an Arrest behavior has reached the player. Latches until the
    // NPC is given a different instruction; lets the UI announce the catch.
    bool hasCaughtPlayer() const { return caughtPlayer_; }

    // The action parsed from the most recent reply (None if the NPC didn't act
    // this turn). Lets the UI show a stage direction even when the model spoke
    // no words alongside its action tag.
    NpcAction lastAction() const { return lastAction_; }

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

    const Persona& persona() const { return persona_; }
    const std::vector<ChatTurn>& history() const { return history_; }
    bool waiting() const { return pendingId_ != 0; }

    // Persisted memory from earlier sessions, injected into every system
    // prompt this NPC sends (Persona::renderSystemPrompt(memory)). Loaded
    // from the ConversationStore at startup and refreshed after each
    // summarization.
    void setMemory(std::string summary) { memory_ = std::move(summary); }
    const std::string& memory() const { return memory_; }

    // Town gossip THIS character has heard, rendered by main from the
    // shared fact bus whenever this NPC's knowledge set changes; injected
    // into every system prompt alongside the memory.
    void setGossip(std::string gossip) { gossip_ = std::move(gossip); }

    // Installs the shared trait library (issue #116); persona().traitIds
    // resolve against it at prompt time.
    void setTraitRegistry(const std::vector<TraitDef>* registry) {
        traitRegistry_ = registry;
    }

    // persona().traitIds resolved against the registry (unknown ids skip;
    // they were logged at spawn).
    std::vector<const TraitDef*> resolvedTraits() const;
    const std::string& gossip() const { return gossip_; }

    // --- Role layer (issue #199) -------------------------------------------
    //
    // The per-match part this NPC plays and the one thing they are hiding.
    // ASSIGNED, never chosen here: generateMystery picks the killer (#178) and
    // castStoryline deals every other slot (#188). This carries what it is
    // handed and resolves the id, nothing more.
    //
    // NEVER SHOWN TO A PLAYER. No UI, no journal line, no debug overlay reads
    // these — the whole mechanic is that the town looks the same whoever did
    // it. The secret reaches the model inside the system prompt; keeping it
    // out of the transcript is the prompt's job, and test coverage says so.
    void setRole(std::string roleId, std::string secret) {
        roleId_ = std::move(roleId);
        secret_ = std::move(secret);
    }
    const std::string& roleId() const { return roleId_; }
    const std::string& secret() const { return secret_; }

    // Installs the shared role library. Mirrors setTraitRegistry exactly,
    // including the lifetime rule: the vector must outlive the NPC.
    void setRoleRegistry(const std::vector<RoleDef>* registry) {
        roleRegistry_ = registry;
    }

    // roleId_ resolved against the registry, or nullptr.
    //
    // DEMOTE AND LOG, the same contract unknown traitIds have. Never a crash,
    // and never a silent substitution — substituting for an unknown killer
    // role would produce a match with two killers, or none, and nothing
    // downstream would notice.
    const RoleDef* resolvedRole() const;

    // --- Combat interface --------------------------------------------------

    // Reduces hp by `amount` (clamped at 0). Transitions state: armed NPCs
    // turn Hostile, unarmed ones Fleeing; hp reaching 0 means Dead either
    // way. No-op when already Dead.
    void takeDamage(int amount);

    // Enters the Dead state directly, without going through takeDamage.
    //
    // For the opening murder (plan: opening-murder): the victim STARTS dead,
    // they are not killed during play. Two reasons this is its own entry point
    // rather than takeDamage(hp()):
    //
    //   - Intent. There is no attacker, no damage amount and no hit; going
    //     through the damage path to express "was already dead when the match
    //     began" reads as a killing that happens to have no source.
    //   - Blast radius. Damage is meant to be observed. World::updateCombat is
    //     what emits NpcDamagedEvent, and main.cpp turns that into a floating
    //     "<name> collapses!" callout. takeDamage alone emits nothing today, so
    //     this is not a bug being dodged — it is a path that stays quiet even
    //     if the damage path grows a hook later.
    //
    // Idempotent, and a no-op on an already-dead NPC.
    void markDeadAtStart() {
        hp_ = 0;
        state_ = NpcState::Dead;
    }

    NpcState combatState() const { return state_; }
    int hp() const { return hp_; }
    int hpMax() const { return hpMax_; }
    bool isArmed() const { return persona_.armed; }

    // Stands a surviving NPC down after the player respawns (Fleeing and
    // Hostile return to Idle; the dead stay dead).
    void calmDown() {
        if (state_ != NpcState::Dead) state_ = NpcState::Idle;
    }

    // Plants this NPC on the ground. THE ONE AUTHORITY over an NPC's
    // vertical position.
    //
    // Movement code drives horizontal motion only (see steerXZ) and this owns
    // the rest. Splitting those two is what let an NPC following a jumping
    // player climb into the air: the step carried the vertical gap,
    // City::resolveMovement wrote it through with `pos.y = to.y`, and nothing
    // put it back.
    //
    // Ground is 0 because there is nowhere else an NPC can legally be.
    // Buildings are solid axis-aligned boxes with no interiors and
    // resolveMovement refuses to let anyone inside one, so no NPC can stand on
    // a surface above the plane. If they ever climb, this is the single line
    // that changes — which is the point of it being one line in one place.
    void snapToGround() { position_.y = 0.f; }

    // Derives facing from the actual displacement since `prevPosition` —
    // the ONE source of truth for "which way am I moving", called once per
    // frame after every mover (behaviors AND combat) has run. Reorients
    // only above a small speed threshold, so standing still never flips
    // facing and deliberate standing turns (lookAt during chat) hold.
    // Kills the moonwalk class of bugs at the source: any future system
    // that moves an NPC gets correct facing for free.
    void deriveFacingFromMotion(const Vec3& prevPosition, float dt);

    // World-space feet position, writable so World::updateCombat can move
    // fleeing/hostile NPCs (conversational movement stays in update()).
    Vec3& position() { return position_; }

    // Seconds before this NPC can fire again (armed only); World's combat
    // tick decrements it and rolls the next shot.
    float fireCooldown = 0.f;

   private:
    Persona persona_;
    LlmClient& client_;
    int maxHistoryTurns_;
    std::vector<ChatTurn> history_;
    std::uint64_t pendingId_ = 0;
    std::string pendingUserLine_;
    std::string memory_;  // persisted cross-session summary (may be empty)
    // The loaded trait library (owned by main, outlives every NPC); null
    // when no traits are in play. Resolution happens per prompt so the
    // registry can be shared by every NPC without copies.
    const std::vector<TraitDef>* traitRegistry_ = nullptr;
    const std::vector<RoleDef>* roleRegistry_ = nullptr;
    std::string roleId_;
    std::string secret_;
    std::string gossip_;  // facts heard around town (may be empty)

    Vec3 position_{};
    float facingDeg_ = 0.f;
    std::string spotId_;

    NpcAction behavior_ = NpcAction::None;    // persistent: follow/arrest/stop/face
    NpcAction pose_ = NpcAction::None;        // transient gesture overlay
    NpcAction lastAction_ = NpcAction::None;  // action from the latest reply
    NpcMood mood_ = NpcMood::Neutral;         // current expression
    float moodTimer_ = 0.f;                   // seconds until mood relaxes
    float poseTimer_ = 0.f;                   // seconds of gesture remaining
    float gesturePhase_ = 0.f;                // seconds elapsed in current gesture
    bool caughtPlayer_ = false;               // arrest reached the player
    Vec3 homePosition_{};                     // spawn spot for ReturnHome
    float homeFacingDeg_ = 0.f;               // spawn facing for ReturnHome
    std::vector<ScheduleEntry> schedule_;     // daily routine (may be empty)
    std::optional<Vec3> gatherTarget_;        // #156 override; outranks schedule_
    std::string activity_;                    // active entry's label

    // Combat state — all zero/Idle until something hurts this NPC.
    int hp_ = 100;
    int hpMax_ = 100;
    NpcState state_ = NpcState::Idle;

    // Routes a freshly parsed action into behavior/gesture state.
    void applyAction(NpcAction action);
    void faceToward(const Vec3& target);
    void trimHistory();
};

}  // namespace llm_npc
