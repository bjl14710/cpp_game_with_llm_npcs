#include "NetMessage.hpp"

namespace llm_npc {

namespace {

// One row per MessageType; order must match the enum so the enum value
// indexes its own name.
constexpr const char* kTypeNames[] = {
    "JoinRequest",   "Welcome",   "PlayerInput",   "WorldSnapshot",
    "ChatOpen",      "ChatLine",  "ChatDelta",     "ChatReply",
    "NpcMoodUpdate", "NpcSpeechBubble", "Disconnect",
    "SandboxMapSync",
};
constexpr int kTypeCount = static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0]));

}  // namespace

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
    v.x = j.value("x", 0.f);
    v.y = j.value("y", 0.f);
    v.z = j.value("z", 0.f);
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
    p.playerId = j.value("id", -1);
    p.name = j.value("name", std::string{});
    if (j.contains("pos")) p.position = vec3FromJson(j["pos"]);
    p.facingDeg = j.value("facing", 0.f);
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
    n.npcIndex = j.value("i", -1);
    if (j.contains("pos")) n.position = vec3FromJson(j["pos"]);
    n.facingDeg = j.value("facing", 0.f);
    n.mood = j.value("mood", 0);
    n.behavior = j.value("behavior", 0);
    return n;
}

}  // namespace llm_npc
