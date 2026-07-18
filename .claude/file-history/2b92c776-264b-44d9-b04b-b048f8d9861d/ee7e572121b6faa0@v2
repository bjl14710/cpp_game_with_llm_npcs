#include "Npc.hpp"

#include <cmath>
#include <utility>

namespace llm_npc {

namespace {
// Movement tuning, in world units (~meters). NPCs are deliberately a touch
// slower than the player (kWalkSpeed = 7 in main.cpp) so following feels like
// trailing rather than shoving, while an arrest can nearly keep pace.
constexpr float kNpcWalk = 3.8f;       // follow speed
constexpr float kNpcRun = 6.0f;        // arrest speed
constexpr float kNpcRadius = 0.45f;    // collision circle, matches the player
constexpr float kFollowStop = 2.5f;    // stop this close when following
constexpr float kCatchRadius = 1.6f;   // arrest succeeds within this range
// Long enough that the gesture is still playing after the player reads the
// reply and leaves the dialog to look at the NPC (the timer starts the moment
// the reply lands, while the dialog may still be open).
constexpr float kGestureSeconds = 15.f; // how long raise_hand / wave holds
}  // namespace

Npc::Npc(Persona persona, LlmClient& client, int maxHistoryTurns)
    : persona_(std::move(persona)), client_(client), maxHistoryTurns_(maxHistoryTurns) {}

std::uint64_t Npc::ask(const std::string& playerLine) {
    // Send a snapshot of the current history; the player line is appended to
    // history only once a successful reply arrives. That way a failed request
    // can be retried without polluting context with unanswered user turns.
    pendingUserLine_ = playerLine;
    pendingId_ = client_.submit(persona_.renderSystemPrompt(), history_, playerLine);
    return pendingId_;
}

std::optional<std::string> Npc::onReplyArrived(const ChatReply& reply) {
    if (reply.id != pendingId_) return std::nullopt;
    pendingId_ = 0;

    if (!reply.ok) {
        // Don't append anything on failure — let the UI report the error and
        // let the player retry. Clear the pending user line too.
        pendingUserLine_.clear();
        return std::nullopt;
    }

    // Pull any action directive out of the reply before it is shown or
    // remembered, so the tag never appears in the transcript or future
    // context, and route it into behavior/gesture state.
    std::string content = reply.content;
    applyAction(parseActionTag(content));

    history_.push_back({"user", std::move(pendingUserLine_)});
    history_.push_back({"assistant", content});
    pendingUserLine_.clear();
    trimHistory();
    return content;
}

void Npc::applyAction(NpcAction action) {
    lastAction_ = action;  // remembered for one turn so the UI can narrate it
    switch (action) {
        case NpcAction::None:
            // The reply carried no directive; leave existing behavior intact.
            break;
        case NpcAction::Follow:
        case NpcAction::Stop:
        case NpcAction::Face:
        case NpcAction::Arrest:
            behavior_ = action;
            caughtPlayer_ = false;  // a new instruction clears the latch
            break;
        case NpcAction::RaiseHand:
        case NpcAction::Wave:
            pose_ = action;
            poseTimer_ = kGestureSeconds;
            gesturePhase_ = 0.f;
            break;
    }
}

void Npc::faceToward(const Vec3& target) {
    const float dx = target.x - position_.x;
    const float dz = target.z - position_.z;
    if (dx * dx + dz * dz < 1e-6f) return;  // on top of target; keep facing
    // Yaw convention matches flatForward/forwardFromAngles: 0 looks toward +Z,
    // increasing toward +X, hence atan2(dx, dz).
    facingDeg_ = std::atan2(dx, dz) * 180.f / 3.14159265358979323846f;
}

void Npc::update(float dt, const Vec3& playerPos, const City& city) {
    // Tick down any gesture overlay independently of movement.
    if (poseTimer_ > 0.f) {
        gesturePhase_ += dt;
        poseTimer_ -= dt;
        if (poseTimer_ <= 0.f) {
            pose_ = NpcAction::None;
            poseTimer_ = 0.f;
        }
    }

    const float dist = distanceXZ(position_, playerPos);

    switch (behavior_) {
        case NpcAction::Follow: {
            faceToward(playerPos);
            if (dist > kFollowStop) {
                const Vec3 step = normalize(playerPos - position_) * (kNpcWalk * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            }
            break;
        }
        case NpcAction::Arrest: {
            faceToward(playerPos);
            if (dist > kCatchRadius) {
                const Vec3 step = normalize(playerPos - position_) * (kNpcRun * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            } else {
                behavior_ = NpcAction::Stop;  // caught: settle and hold
                caughtPlayer_ = true;
            }
            break;
        }
        case NpcAction::Stop:
        case NpcAction::Face:
            faceToward(playerPos);
            break;
        case NpcAction::None:
        case NpcAction::RaiseHand:  // gesture-only kinds never reach behavior_
        case NpcAction::Wave:
            break;
    }
}

void Npc::trimHistory() {
    // History is a flat list of role/content turns. Two entries (user +
    // assistant) make one round-trip turn, hence the *2 cap.
    const int maxEntries = maxHistoryTurns_ * 2;
    if (static_cast<int>(history_.size()) <= maxEntries) return;
    const int drop = static_cast<int>(history_.size()) - maxEntries;
    history_.erase(history_.begin(), history_.begin() + drop);
}

}  // namespace llm_npc
