// Tests for NetMessage encode/decode — the JSON wire protocol for
// multiplayer (plan: .claude/plans/multiplayer-and-aws-deploy.md).
#include <string>
#include <vector>

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

// ---- the trust boundary (issue #245) ---------------------------------------
//
// Every other test in this file sends well-formed JSON, which is exactly why
// the throwing path survived: it was never entered. These cases send payloads
// a hostile — or merely mismatched — peer would send.

TEST_CASE("a wrong-typed field never throws out of a decoder") {
    // The reported crash, at the layer that caused it:
    //     {"type": "PlayerInput", "facing": "north"}
    // NetServer read `facing` with value(), which throws when the key is
    // present with the wrong type, and there is no try/catch in the whole
    // connection path — so this terminated the host.
    nlohmann::json hostile;
    hostile["facing"] = "north";
    hostile["id"] = "one";
    hostile["name"] = 17;
    hostile["mood"] = nlohmann::json::array();
    hostile["pos"] = "somewhere";

    CHECK(llm_npc::readFloat(hostile, "facing", 1.5f) == doctest::Approx(1.5f));
    CHECK(llm_npc::readInt(hostile, "id", -1) == -1);
    CHECK(llm_npc::readString(hostile, "name") == "");
    CHECK(llm_npc::readInt(hostile, "mood", 0) == 0);

    const PlayerPose pose = llm_npc::playerPoseFromJson(hostile);
    CHECK(pose.playerId == -1);
    CHECK(pose.name.empty());
    CHECK(pose.facingDeg == doctest::Approx(0.f));

    const NetNpcPose npc = llm_npc::netNpcPoseFromJson(hostile);
    CHECK(npc.npcIndex == -1);
    CHECK(npc.mood == 0);
}

TEST_CASE("a non-object payload is absorbed rather than indexed") {
    // decodeMessage rejects these at the top level, but the helpers are called
    // from places already holding a sub-object, so they have to stand alone.
    for (const nlohmann::json junk :
         {nlohmann::json(nullptr), nlohmann::json(3), nlohmann::json("text"),
          nlohmann::json::array({1, 2, 3})}) {
        CHECK(llm_npc::readInt(junk, "id", -7) == -7);
        CHECK(llm_npc::readString(junk, "name", "fallback") == "fallback");
        CHECK(llm_npc::readBool(junk, "ok", true));
        CHECK(llm_npc::readFloat(junk, "facing", 2.5f) == doctest::Approx(2.5f));
        CHECK(llm_npc::playerPoseFromJson(junk).playerId == -1);
        CHECK(llm_npc::netNpcPoseFromJson(junk).npcIndex == -1);
        CHECK(llm_npc::vec3FromJson(junk).x == doctest::Approx(0.f));
    }
}

TEST_CASE("null fields fall back rather than converting") {
    // JSON null is its own type, and get<std::string>() on it throws too.
    nlohmann::json nulls;
    for (const char* key : {"id", "name", "facing", "ok", "pos"}) {
        nulls[key] = nullptr;
    }
    CHECK(llm_npc::readInt(nulls, "id", -1) == -1);
    CHECK(llm_npc::readString(nulls, "name", "x") == "x");
    CHECK(llm_npc::readFloat(nulls, "facing", 9.f) == doctest::Approx(9.f));
    CHECK(llm_npc::readBool(nulls, "ok", true));
    CHECK(llm_npc::playerPoseFromJson(nulls).name.empty());
}

TEST_CASE("an integer where a float was expected is accepted, not rejected") {
    // A peer sending 3 where 3.0 was expected is well-formed. Rejecting it
    // would be pedantry paid for with a crash budget.
    nlohmann::json j;
    j["facing"] = 90;  // integer
    CHECK(llm_npc::readFloat(j, "facing", 0.f) == doctest::Approx(90.f));
}

TEST_CASE("every message type survives a hostile payload of every shape") {
    // The sweep: four payload shapes a well-behaved peer would never send,
    // against every type. The requirement is only that nothing throws — the
    // connection may well be dropped, but the process must not die.
    const std::vector<nlohmann::json> shapes = {
        nlohmann::json::object(),
        nlohmann::json::object({{"facing", "north"}, {"id", "one"}, {"name", 4}}),
        nlohmann::json::object({{"pos", 7}, {"mood", "angry"}, {"i", 1.5}}),
        nlohmann::json::object({{"reason", nullptr}, {"accepted", "yes"}, {"npc", 1.5}}),
    };
    const MessageType all[] = {
        MessageType::JoinRequest,   MessageType::Welcome,
        MessageType::PlayerInput,   MessageType::WorldSnapshot,
        MessageType::ChatOpen,      MessageType::ChatLine,
        MessageType::ChatDelta,     MessageType::ChatReply,
        MessageType::NpcMoodUpdate, MessageType::NpcSpeechBubble,
        MessageType::Disconnect,
    };
    for (const MessageType type : all) {
        for (const nlohmann::json& shape : shapes) {
            CAPTURE(shape.dump());
            const auto decoded = decodeMessage(encodeMessage(type, shape));
            REQUIRE(decoded.has_value());
            // Everything a handler would pull off this payload.
            CHECK_NOTHROW(llm_npc::playerPoseFromJson(decoded->payload));
            CHECK_NOTHROW(llm_npc::netNpcPoseFromJson(decoded->payload));
            CHECK_NOTHROW(llm_npc::vec3FromJson(decoded->payload));
            CHECK_NOTHROW(llm_npc::readInt(decoded->payload, "version", -1));
            CHECK_NOTHROW(llm_npc::readString(decoded->payload, "code"));
            CHECK_NOTHROW(llm_npc::readBool(decoded->payload, "accepted", false));
        }
    }
}
