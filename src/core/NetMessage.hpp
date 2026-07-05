#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Math.hpp"
#include "json.hpp"

namespace llm_npc {

// Bumped whenever the wire format changes incompatibly. The join handshake
// compares versions and rejects mismatched builds instead of desyncing.
inline constexpr int kNetProtocolVersion = 1;

// Hard cap on players in one session (host counts as player 0).
inline constexpr int kMaxPlayers = 4;

// Every message on the wire is one JSON object: {"type": <MessageType as
// string>, ...fields}. Types cover the v1 feature set from
// .claude/plans/multiplayer-and-aws-deploy.md: join handshake, movement
// sync, and server-routed NPC dialogue.
enum class MessageType {
    JoinRequest,     // client -> server: version, player name, join code
    Welcome,         // server -> client: accepted + assigned player id, or refusal reason
    PlayerInput,     // client -> server: movement/facing for one tick
    WorldSnapshot,   // server -> all: authoritative player + NPC state
    ChatOpen,        // client -> server: player opened conversation with NPC i
    ChatLine,        // client -> server: player's typed line for NPC i
    ChatDelta,       // server -> one client: streamed reply fragment
    ChatReply,       // server -> one client: final reply or error text
    NpcMoodUpdate,   // server -> all: NPC i's mood changed (bystanders see expressions)
    NpcSpeechBubble, // server -> all: NPC i said something (bystanders see the bubble)
    Disconnect,      // either direction: goodbye with a reason
};

// Wire name of a message type ("JoinRequest", "WorldSnapshot", ...).
const char* messageTypeToString(MessageType type);

// Inverse of messageTypeToString; nullopt for unknown names so a newer
// peer's messages fail loudly at decode instead of desyncing.
std::optional<MessageType> messageTypeFromString(const std::string& name);

// One remote player's pose inside a WorldSnapshot.
struct PlayerPose {
    int playerId = -1;
    std::string name;
    Vec3 position{};
    float facingDeg = 0.f;
};

// One NPC's replicated state inside a WorldSnapshot. Mood/behavior mirror the
// fields the renderer reads from Npc (mood(), behavior(), position(),
// facingDeg()) so clients can draw NPCs without simulating them.
struct NpcPose {
    int npcIndex = -1;
    Vec3 position{};
    float facingDeg = 0.f;
    int mood = 0;      // static_cast of NpcMood; keep ints on the wire
    int behavior = 0;  // static_cast of NpcAction
};

// Decoded form of any incoming message: the type tag plus the raw JSON so
// each handler pulls the fields it needs. Typed helper structs (above) are
// used for the composite payloads.
struct NetMessage {
    MessageType type = MessageType::Disconnect;
    nlohmann::json payload;  // full message object including "type"
};

// Serializes a message object (already containing its fields) with the given
// type tag into the canonical wire JSON string.
std::string encodeMessage(MessageType type, nlohmann::json payload);

// Parses one frame's payload into a NetMessage. Returns nullopt when the
// bytes are not valid JSON or carry an unknown/missing "type" — callers drop
// the connection rather than guessing (see plan: malformed-frame edge case).
std::optional<NetMessage> decodeMessage(const std::string& bytes);

// Vec3 <-> JSON helpers shared by snapshot encoding: {"x":..,"y":..,"z":..}.
nlohmann::json vec3ToJson(const Vec3& v);
Vec3 vec3FromJson(const nlohmann::json& j);

// Pose <-> JSON helpers for WorldSnapshot's "players" and "npcs" arrays.
nlohmann::json playerPoseToJson(const PlayerPose& p);
PlayerPose playerPoseFromJson(const nlohmann::json& j);
nlohmann::json npcPoseToJson(const NpcPose& n);
NpcPose npcPoseFromJson(const nlohmann::json& j);

}  // namespace llm_npc
