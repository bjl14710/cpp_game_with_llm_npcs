#include "Npc.hpp"

#include "AsciiText.hpp"

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
constexpr float kMoodSeconds = 25.f;    // how long an expression lingers
constexpr float kHomeSnapRadius = 0.6f; // close enough to be "back at post"
constexpr float kSchedulePause = 4.f;   // player this close pauses routine walking
}  // namespace

Npc::Npc(Persona persona, LlmClient& client, int maxHistoryTurns)
    : persona_(std::move(persona)), client_(client), maxHistoryTurns_(maxHistoryTurns) {}

std::uint64_t Npc::ask(const std::string& playerLine) {
    // Send a snapshot of the current history; the player line is appended to
    // history only once a successful reply arrives. That way a failed request
    // can be retried without polluting context with unanswered user turns.
    pendingUserLine_ = playerLine;
    // A persisted summary means this player and this NPC have met before, so a
    // banked greeting must not treat them as a stranger. That is the only place
    // an authored line can contradict remembered history; see banks/README.md.
    const Familiarity familiarity =
        memory_.empty() ? Familiarity::First : Familiarity::Returning;
    pendingId_ =
        client_.submit(persona_.renderSystemPrompt(memory_, gossip_, resolvedTraits(),
                                                   renderRoleBlock(resolvedRole(), secret_)),
                       history_, playerLine, persona_.name, familiarity);
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

    // Pull every directive tag out of the reply before it is shown or
    // remembered, so tags never appear in the transcript or future context,
    // and route them into behavior/gesture/mood state.
    // Fold typographic punctuation the moment model text enters the game.
    // A model writing natural dialogue reaches for a curly apostrophe and the
    // built-in font has no glyph for it, so "don't" reaches the screen as
    // "don?t" — see AsciiText.hpp for why that is a font substitution and not
    // the encoding bug it looks like.
    //
    // Done HERE, at the boundary, rather than at each draw call: the same
    // string goes to the dialogue box, the transcript, the speech bubble, the
    // network and SQLite, and folding it once means those cannot disagree.
    std::string content = toRenderableAscii(reply.content);
    const Directives directives = parseDirectives(content);
    applyAction(directives.action);
    if (directives.hasMood) {
        mood_ = directives.mood;
        moodTimer_ = kMoodSeconds;
    }

    history_.push_back({"user", std::move(pendingUserLine_)});
    history_.push_back({"assistant", content});
    pendingUserLine_.clear();
    trimHistory();
    return content;
}

void Npc::applyAction(NpcAction action) {
    // Only police can actually arrest; when the model has a civilian try
    // anyway, treat it as calling for the police instead.
    if (action == NpcAction::Arrest && !persona_.police) action = NpcAction::CallPolice;

    // Mood/combat gate on following (issue #120): an NPC who is hostile,
    // fleeing, or plain angry at the player does not fall into step with
    // them, whatever the model emitted — the VALIDATE step of
    // propose-validate-commit, same as the civilian-arrest rule above.
    if (action == NpcAction::Follow &&
        (state_ == NpcState::Hostile || state_ == NpcState::Fleeing ||
         mood_ == NpcMood::Angry)) {
        action = NpcAction::None;
    }

    lastAction_ = action;  // remembered for one turn so the UI can narrate it
    switch (action) {
        case NpcAction::None:
        case NpcAction::ReturnHome:  // internal-only; never arrives from a reply
            // Leave existing behavior intact.
            break;
        case NpcAction::Follow:
        case NpcAction::Stop:
        case NpcAction::Face:
        case NpcAction::Arrest:
            behavior_ = action;
            caughtPlayer_ = false;  // a new instruction clears the latch
            break;
        case NpcAction::CallPolice:
            // The world (main loop) routes the summons to police NPCs; the
            // caller just flags someone down, which reads as a wave.
            pose_ = NpcAction::Wave;
            poseTimer_ = kGestureSeconds;
            gesturePhase_ = 0.f;
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

void Npc::update(float dt, const Vec3& playerPos, const City& city,
                 float timeOfDayHours) {
    // Tick down any gesture overlay independently of movement.
    if (poseTimer_ > 0.f) {
        gesturePhase_ += dt;
        poseTimer_ -= dt;
        if (poseTimer_ <= 0.f) {
            pose_ = NpcAction::None;
            poseTimer_ = 0.f;
        }
    }

    // Expressions relax back to neutral when the conversation moves on.
    if (moodTimer_ > 0.f) {
        moodTimer_ -= dt;
        if (moodTimer_ <= 0.f) {
            mood_ = NpcMood::Neutral;
            moodTimer_ = 0.f;
        }
    }

    const float dist = distanceXZ(position_, playerPos);

    switch (behavior_) {
        case NpcAction::Follow: {
            faceToward(playerPos);
            if (dist > kFollowStop) {
                const Vec3 step = steerXZ(position_, playerPos) * (kNpcWalk * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            }
            break;
        }
        case NpcAction::Arrest: {
            faceToward(playerPos);
            if (dist > kCatchRadius) {
                const Vec3 step = steerXZ(position_, playerPos) * (kNpcRun * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            } else {
                behavior_ = NpcAction::Stop;  // caught: settle and hold
                caughtPlayer_ = true;
            }
            break;
        }
        case NpcAction::ReturnHome: {
            const float homeDist = distanceXZ(position_, homePosition_);
            if (homeDist > kHomeSnapRadius) {
                faceToward(homePosition_);
                const Vec3 step = steerXZ(position_, homePosition_) * (kNpcWalk * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            } else {
                behavior_ = NpcAction::None;  // back at post
                facingDeg_ = homeFacingDeg_;
            }
            break;
        }
        case NpcAction::Stop:
        case NpcAction::Face:
            faceToward(playerPos);
            break;
        case NpcAction::None: {
            // Daily routine: walk to the active schedule entry, read from
            // the SHARED clock injected by the caller (negative = no clock,
            // schedule dormant). Combat states own movement instead, and a
            // player within conversation range politely pauses the walk.
            if (timeOfDayHours < 0.f || schedule_.empty() ||
                state_ != NpcState::Idle) {
                break;
            }
            const int active = activeScheduleIndex(schedule_, timeOfDayHours);
            activity_ = active >= 0 ? schedule_[static_cast<std::size_t>(active)].activity
                                    : std::string();
            if (active < 0 || dist < kSchedulePause) break;
            const Vec3& target =
                schedule_[static_cast<std::size_t>(active)].position;
            if (distanceXZ(position_, target) > kHomeSnapRadius) {
                faceToward(target);
                const Vec3 step = steerXZ(position_, target) * (kNpcWalk * dt);
                position_ = city.resolveMovement(position_, position_ + step, kNpcRadius);
            }
            break;
        }
        case NpcAction::RaiseHand:  // gesture-only kinds never reach behavior_
        case NpcAction::Wave:
        case NpcAction::CallPolice:
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

void Npc::deriveFacingFromMotion(const Vec3& prevPosition, float dt) {
    if (state_ == NpcState::Dead || dt <= 0.f) return;  // corpses hold their pose
    const float dx = position_.x - prevPosition.x;
    const float dz = position_.z - prevPosition.z;
    // Below walking pace, keep the current facing (idle jitter, tiny nudges,
    // and conversation turns must not fight the derivation).
    constexpr float kFacingSpeedThreshold = 0.5f;  // units per second
    if ((dx * dx + dz * dz) < (kFacingSpeedThreshold * dt) * (kFacingSpeedThreshold * dt)) {
        return;
    }
    facingDeg_ = std::atan2(dx, dz) * 180.f / 3.14159265358979323846f;
}

void Npc::takeDamage(int amount) {
    if (state_ == NpcState::Dead) return;
    hp_ -= amount;
    if (hp_ <= 0) {
        hp_ = 0;
        state_ = NpcState::Dead;
        return;
    }
    state_ = persona_.armed ? NpcState::Hostile : NpcState::Fleeing;
}

const RoleDef* Npc::resolvedRole() const {
    if (roleId_.empty() || roleRegistry_ == nullptr) return nullptr;
    // findRole returns nullptr for an unknown id; the spawn site logs it, the
    // same division of labour resolvedTraits uses. Rendering with a nullptr
    // role yields a block carrying only the secret, so an unknown id costs the
    // directives and nothing else — it never invents a part.
    return findRole(*roleRegistry_, roleId_);
}

std::vector<const TraitDef*> Npc::resolvedTraits() const {
    std::vector<const TraitDef*> out;
    if (!traitRegistry_) return out;
    for (const std::string& id : persona_.traitIds) {
        for (const TraitDef& trait : *traitRegistry_) {
            if (trait.id == id) {
                out.push_back(&trait);
                break;
            }
        }
        // Unknown ids were logged at spawn; render just skips them.
    }
    return out;
}

}  // namespace llm_npc
