#include "NetMessage.hpp"

#include <string>

namespace llm_npc {

namespace {

// One row per MessageType; order must match the enum so the enum value
// indexes its own name.
constexpr const char* kTypeNames[] = {
    "JoinRequest",   "Welcome",   "PlayerInput",   "WorldSnapshot",
    "ChatOpen",      "ChatLine",  "ChatDelta",     "ChatReply",
    "NpcMoodUpdate", "NpcSpeechBubble", "Disconnect",
    "VoteOpen",      "VoteNominate", "VoteState",  "VoteConfirm",
    "VoteResolved",  "MatchOver",
};
constexpr int kTypeCount = static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0]));

// messageTypeToString indexes this array by enum value with no bounds check,
// so a type added to the enum without a name here reads one past the end and
// corrupts the protocol quietly. Caught at COMPILE time rather than by a test
// somebody has to remember to run.
static_assert(kTypeCount == kMessageTypeCount,
              "kTypeNames is missing a row for a MessageType — add the name "
              "next to the enum entry, in the same position");

}  // namespace

bool readBool(const nlohmann::json& j, const char* key, bool fallback) {
    if (!j.is_object()) return fallback;
    const auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

int readInt(const nlohmann::json& j, const char* key, int fallback) {
    if (!j.is_object()) return fallback;
    const auto it = j.find(key);
    return (it != j.end() && it->is_number_integer()) ? it->get<int>() : fallback;
}

float readFloat(const nlohmann::json& j, const char* key, float fallback) {
    if (!j.is_object()) return fallback;
    const auto it = j.find(key);
    // is_number covers int and float: a peer sending 3 where 3.0 was expected
    // is well-formed, and rejecting it would be pedantry with a crash budget.
    return (it != j.end() && it->is_number()) ? it->get<float>() : fallback;
}

std::string readString(const nlohmann::json& j, const char* key,
                       const std::string& fallback) {
    if (!j.is_object()) return fallback;
    const auto it = j.find(key);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

const char* messageTypeToString(MessageType type) {
    return kTypeNames[static_cast<int>(type)];
}

std::optional<MessageType> messageTypeFromString(const std::string& name) {
    for (int i = 0; i < kTypeCount; ++i) {
        if (name == kTypeNames[i]) return static_cast<MessageType>(i);
    }
    return std::nullopt;
}

std::string encodeMessage(MessageType type, nlohmann::json payload) {
    payload["type"] = messageTypeToString(type);
    return payload.dump();
}

std::optional<NetMessage> decodeMessage(const std::string& bytes) {
    nlohmann::json parsed = nlohmann::json::parse(bytes, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object()) return std::nullopt;
    if (!parsed.contains("type") || !parsed["type"].is_string()) return std::nullopt;
    auto type = messageTypeFromString(parsed["type"].get<std::string>());
    if (!type) return std::nullopt;
    NetMessage msg;
    msg.type = *type;
    msg.payload = std::move(parsed);
    return msg;
}

nlohmann::json vec3ToJson(const Vec3& v) {
    return {{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

Vec3 vec3FromJson(const nlohmann::json& j) {
    Vec3 v;
    v.x = readFloat(j, "x", 0.f);
    v.y = readFloat(j, "y", 0.f);
    v.z = readFloat(j, "z", 0.f);
    return v;
}

nlohmann::json playerPoseToJson(const PlayerPose& p) {
    return {{"id", p.playerId},
            {"name", p.name},
            {"pos", vec3ToJson(p.position)},
            {"facing", p.facingDeg}};
}

PlayerPose playerPoseFromJson(const nlohmann::json& j) {
    PlayerPose p;
    p.playerId = readInt(j, "id", -1);
    p.name = readString(j, "name");
    if (j.is_object() && j.contains("pos")) p.position = vec3FromJson(j["pos"]);
    p.facingDeg = readFloat(j, "facing", 0.f);
    return p;
}

nlohmann::json netNpcPoseToJson(const NetNpcPose& n) {
    return {{"i", n.npcIndex},
            {"pos", vec3ToJson(n.position)},
            {"facing", n.facingDeg},
            {"mood", n.mood},
            {"behavior", n.behavior}};
}

NetNpcPose netNpcPoseFromJson(const nlohmann::json& j) {
    NetNpcPose n;
    n.npcIndex = readInt(j, "i", -1);
    if (j.is_object() && j.contains("pos")) n.position = vec3FromJson(j["pos"]);
    n.facingDeg = readFloat(j, "facing", 0.f);
    n.mood = readInt(j, "mood", 0);
    n.behavior = readInt(j, "behavior", 0);
    return n;
}

nlohmann::json voteStateToJson(const VoteStateMsg& v) {
    nlohmann::json confirmations = nlohmann::json::array();
    for (const auto& [playerId, agreed] : v.confirmations) {
        confirmations.push_back({{"id", playerId}, {"ok", agreed}});
    }
    return {{"nominee", v.nominee},
            {"nominator", v.nominator},
            {"confirmations", std::move(confirmations)}};
}

VoteStateMsg voteStateFromJson(const nlohmann::json& j) {
    VoteStateMsg v;
    v.nominee = readString(j, "nominee");
    v.nominator = readInt(j, "nominator", -1);
    // Every field is read totally, with a default — a truncated or hostile
    // payload must produce an empty ballot, not an exception in the network
    // thread. `.get<>()` on a missing key throws; `.value()` does not.
    if (j.contains("confirmations") && j["confirmations"].is_array()) {
        for (const auto& entry : j["confirmations"]) {
            if (!entry.is_object()) continue;
            v.confirmations.emplace_back(readInt(entry, "id", -1),
                                         readBool(entry, "ok", false));
        }
    }
    return v;
}

nlohmann::json voteResolvedToJson(const VoteResolvedMsg& v) {
    // Ints on the wire for the enum, the same convention NetNpcPose uses for
    // mood and behavior.
    return {{"outcome", static_cast<int>(v.outcome)},
            {"accused", v.accused},
            {"nominator", v.nominator},
            {"killed", v.playerKilled}};
}

VoteResolvedMsg voteResolvedFromJson(const nlohmann::json& j) {
    VoteResolvedMsg v;
    const int outcome =
        readInt(j, "outcome", static_cast<int>(VoteOutcome::NoAccusation));
    // An out-of-range outcome from a newer or hostile peer degrades to
    // NoAccusation, which is the only value that kills nobody. Casting a
    // garbage int straight into the enum and switching on it is how a bad
    // frame gets to execute a player.
    v.outcome = (outcome >= static_cast<int>(VoteOutcome::NoAccusation) &&
                 outcome <= static_cast<int>(VoteOutcome::Wrong))
                    ? static_cast<VoteOutcome>(outcome)
                    : VoteOutcome::NoAccusation;
    v.accused = readString(j, "accused");
    v.nominator = readInt(j, "nominator", -1);
    v.playerKilled = readInt(j, "killed", -1);
    return v;
}

nlohmann::json matchOverToJson(const MatchOverMsg& m) {
    return {{"killer", m.killer}, {"won", m.playersWon}};
}

MatchOverMsg matchOverFromJson(const nlohmann::json& j) {
    MatchOverMsg m;
    m.killer = readString(j, "killer");
    m.playersWon = readBool(j, "won", false);
    return m;
}

}  // namespace llm_npc
