// Tests for NetMessage encode/decode — the JSON wire protocol for
// multiplayer (plan: .claude/plans/multiplayer-and-aws-deploy.md).
#include <string>

#include "NetMessage.hpp"
#include "doctest.h"

using llm_npc::decodeMessage;
using llm_npc::encodeMessage;
using llm_npc::kNetProtocolVersion;
using llm_npc::MessageType;
using llm_npc::NetMessage;
using llm_npc::NetNpcPose;
using llm_npc::PlayerPose;
using llm_npc::Vec3;

TEST_CASE("encodeMessage/decodeMessage round-trips every message type") {
    const MessageType all[] = {
        MessageType::JoinRequest,   MessageType::Welcome,
        MessageType::PlayerInput,   MessageType::WorldSnapshot,
        MessageType::ChatOpen,      MessageType::ChatLine,
        MessageType::ChatDelta,     MessageType::ChatReply,
        MessageType::NpcMoodUpdate, MessageType::NpcSpeechBubble,
        MessageType::Disconnect,
    };
    for (MessageType type : all) {
        CAPTURE(llm_npc::messageTypeToString(type));
        auto decoded = decodeMessage(encodeMessage(type, {{"n", 7}, {"s", "abc"}}));
        REQUIRE(decoded.has_value());
        CHECK(decoded->type == type);
        CHECK(decoded->payload["n"] == 7);
        CHECK(decoded->payload["s"] == "abc");
    }
}

TEST_CASE("decodeMessage rejects invalid JSON") {
    CHECK_FALSE(decodeMessage("not json at all").has_value());
    CHECK_FALSE(decodeMessage("").has_value());
    CHECK_FALSE(decodeMessage("[1,2,3]").has_value());  // valid JSON, not an object
}

TEST_CASE("decodeMessage rejects an unknown or missing type tag") {
    CHECK_FALSE(decodeMessage(R"({"type":"FromTheFuture"})").has_value());
    CHECK_FALSE(decodeMessage(R"({"x":1})").has_value());
    CHECK_FALSE(decodeMessage(R"({"type":42})").has_value());  // non-string tag
}

TEST_CASE("vec3ToJson/vec3FromJson round-trips coordinates") {
    const Vec3 v{-1.5f, 0.f, 1234.25f};
    const Vec3 back = llm_npc::vec3FromJson(llm_npc::vec3ToJson(v));
    CHECK(back.x == doctest::Approx(v.x));
    CHECK(back.y == doctest::Approx(v.y));
    CHECK(back.z == doctest::Approx(v.z));
}

TEST_CASE("JoinRequest carries the protocol version for the handshake") {
    auto decoded = decodeMessage(encodeMessage(
        MessageType::JoinRequest,
        {{"version", kNetProtocolVersion}, {"name", "brandon"}, {"code", ""}}));
    REQUIRE(decoded.has_value());
    CHECK(decoded->payload["version"] == kNetProtocolVersion);
    CHECK(decoded->payload["name"] == "brandon");
}

TEST_CASE("WorldSnapshot round-trips player and NPC poses") {
    PlayerPose p0;
    p0.playerId = 0;
    p0.name = "host";
    p0.position = {1.f, 0.f, -2.f};
    p0.facingDeg = 90.f;
    PlayerPose p1;
    p1.playerId = 1;
    p1.name = "guest";
    p1.position = {-3.5f, 0.f, 4.f};
    p1.facingDeg = 270.f;

    nlohmann::json players = nlohmann::json::array();
    players.push_back(llm_npc::playerPoseToJson(p0));
    players.push_back(llm_npc::playerPoseToJson(p1));

    nlohmann::json npcs = nlohmann::json::array();
    for (int i = 0; i < 3; ++i) {
        NetNpcPose n;
        n.npcIndex = i;
        n.position = {static_cast<float>(i), 0.f, static_cast<float>(-i)};
        n.facingDeg = 45.f * static_cast<float>(i);
        n.mood = i;
        n.behavior = 2 - i;
        npcs.push_back(llm_npc::netNpcPoseToJson(n));
    }

    auto decoded = decodeMessage(encodeMessage(
        MessageType::WorldSnapshot, {{"tick", 42}, {"players", players}, {"npcs", npcs}}));
    REQUIRE(decoded.has_value());
    CHECK(decoded->payload["tick"] == 42);

    REQUIRE(decoded->payload["players"].size() == 2);
    const PlayerPose back1 = llm_npc::playerPoseFromJson(decoded->payload["players"][1]);
    CHECK(back1.playerId == 1);
    CHECK(back1.name == "guest");
    CHECK(back1.position.x == doctest::Approx(-3.5f));
    CHECK(back1.facingDeg == doctest::Approx(270.f));

    REQUIRE(decoded->payload["npcs"].size() == 3);
    const NetNpcPose backN = llm_npc::netNpcPoseFromJson(decoded->payload["npcs"][2]);
    CHECK(backN.npcIndex == 2);
    CHECK(backN.position.z == doctest::Approx(-2.f));
    CHECK(backN.mood == 2);
    CHECK(backN.behavior == 0);
}
