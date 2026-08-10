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
// The vote protocol (issue #222).
using llm_npc::kMessageTypeCount;
using llm_npc::matchOverFromJson;
using llm_npc::MatchOverMsg;
using llm_npc::matchOverToJson;
using llm_npc::messageTypeFromString;
using llm_npc::messageTypeToString;
using llm_npc::voteResolvedFromJson;
using llm_npc::VoteResolvedMsg;
using llm_npc::voteResolvedToJson;
using llm_npc::voteStateFromJson;
using llm_npc::VoteStateMsg;
using llm_npc::voteStateToJson;
using llm_npc::VoteOutcome;

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

// ---- the vote messages (issue #222) ----------------------------------------

TEST_CASE("every MessageType has a wire name") {
    // The static_assert in NetMessage.cpp is the real guard — this is the
    // runtime half, because the failure it prevents is silent:
    // messageTypeToString indexes kTypeNames by enum value with no bounds
    // check, so a type added without a name reads past the end.
    for (int i = 0; i < kMessageTypeCount; ++i) {
        const auto type = static_cast<MessageType>(i);
        const char* name = messageTypeToString(type);
        CAPTURE(i);
        REQUIRE(name != nullptr);
        CHECK(std::string(name).size() > 0);
        // And the name maps back to the same value: a duplicated row would
        // pass the count check but break the protocol.
        const auto round = messageTypeFromString(name);
        REQUIRE(round.has_value());
        CHECK(static_cast<int>(*round) == i);
    }
}

TEST_CASE("vote message types survive an encode/decode round trip") {
    for (const MessageType type :
         {MessageType::VoteOpen, MessageType::VoteNominate, MessageType::VoteState,
          MessageType::VoteConfirm, MessageType::VoteResolved,
          MessageType::MatchOver}) {
        CAPTURE(messageTypeToString(type));
        const std::string bytes = encodeMessage(type, nlohmann::json::object());
        const auto decoded = decodeMessage(bytes);
        REQUIRE(decoded.has_value());
        CHECK(decoded->type == type);
    }
}

TEST_CASE("VoteState round-trips the ballot field by field") {
    VoteStateMsg sent;
    sent.nominee = "Marge Holloway";
    sent.nominator = 2;
    sent.confirmations = {{0, true}, {1, false}, {2, true}};

    const auto got = voteStateFromJson(voteStateToJson(sent));
    CHECK(got.nominee == "Marge Holloway");
    CHECK(got.nominator == 2);
    REQUIRE(got.confirmations.size() == 3);
    CHECK(got.confirmations[0] == std::pair<int, bool>{0, true});
    CHECK(got.confirmations[1] == std::pair<int, bool>{1, false});
    CHECK(got.confirmations[2] == std::pair<int, bool>{2, true});
}

TEST_CASE("VoteResolved round-trips, outcome included") {
    VoteResolvedMsg sent;
    sent.outcome = VoteOutcome::Wrong;
    sent.accused = "Ray Okafor";
    sent.nominator = 1;
    sent.playerKilled = 1;

    const auto got = voteResolvedFromJson(voteResolvedToJson(sent));
    CHECK(got.outcome == VoteOutcome::Wrong);
    CHECK(got.accused == "Ray Okafor");
    CHECK(got.nominator == 1);
    CHECK(got.playerKilled == 1);
}

TEST_CASE("MatchOver round-trips the reveal") {
    MatchOverMsg sent;
    sent.killer = "Marge Holloway";
    sent.playersWon = true;

    const auto got = matchOverFromJson(matchOverToJson(sent));
    CHECK(got.killer == "Marge Holloway");
    CHECK(got.playersWon);
}

TEST_CASE("a truncated vote payload decodes to an empty ballot, not an exception") {
    // Every field is read with a default. `.get<>()` on a missing key throws,
    // and an exception in the network thread from a half-arrived frame is a
    // remote crash, not a parse error.
    const auto empty = nlohmann::json::object();
    CHECK(voteStateFromJson(empty).nominee.empty());
    CHECK(voteStateFromJson(empty).nominator == -1);
    CHECK(voteStateFromJson(empty).confirmations.empty());
    CHECK(voteResolvedFromJson(empty).outcome == VoteOutcome::NoAccusation);
    CHECK(voteResolvedFromJson(empty).playerKilled == -1);
    CHECK(matchOverFromJson(empty).killer.empty());
    CHECK_FALSE(matchOverFromJson(empty).playersWon);
}

TEST_CASE("hostile vote payloads are absorbed rather than trusted") {
    // Wrong types where objects and arrays were expected.
    nlohmann::json junk;
    junk["nominee"] = 17;                 // not a string
    junk["confirmations"] = "not_an_array";
    const auto state = voteStateFromJson(junk);
    CHECK(state.nominee.empty());         // .value() returns the default on a type clash
    CHECK(state.confirmations.empty());

    nlohmann::json partly;
    partly["confirmations"] = nlohmann::json::array({"scalar", 3, nullptr});
    CHECK(voteStateFromJson(partly).confirmations.empty());  // non-objects skipped
}

TEST_CASE("an out-of-range outcome degrades to NoAccusation") {
    // Casting a garbage int straight into the enum and switching on it is how
    // a malformed frame gets to execute a player. NoAccusation is the only
    // value that kills nobody, so it is the right landing place.
    for (const int bogus : {-1, 3, 99, 1 << 20}) {
        CAPTURE(bogus);
        nlohmann::json j;
        j["outcome"] = bogus;
        CHECK(voteResolvedFromJson(j).outcome == VoteOutcome::NoAccusation);
    }
}

TEST_CASE("appending the vote types did not renumber the existing ones") {
    // The enum value IS the wire identity for anything that indexes by it, and
    // a peer mid-upgrade would reinterpret every message if these moved.
    CHECK(static_cast<int>(MessageType::JoinRequest) == 0);
    CHECK(static_cast<int>(MessageType::Disconnect) == 10);
    CHECK(static_cast<int>(MessageType::VoteOpen) == 11);
    CHECK(static_cast<int>(MessageType::MatchOver) == kMessageTypeCount - 1);
}

// ---- the trust boundary (issue #245) ---------------------------------------
//
// Every existing test in this file sends well-formed JSON, which is exactly
// why the throwing path survived: it was never entered. These cases send
// payloads a hostile or merely mismatched peer would send.

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

    // Each of these used to throw; each must now fall back.
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
    // decodeMessage rejects these, but the helpers are called from places that
    // already hold a sub-object, so they have to stand alone.
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
    // The sweep: for each type, four payload shapes that a well-behaved peer
    // would never send. The requirement is only that nothing throws — the
    // connection may well be dropped, but the process must not die.
    const std::vector<nlohmann::json> shapes = {
        nlohmann::json::object(),
        nlohmann::json::object({{"facing", "north"}, {"id", "one"}, {"name", 4}}),
        nlohmann::json::object({{"pos", 7}, {"confirmations", 3}, {"outcome", "x"}}),
        nlohmann::json::object({{"killer", nullptr}, {"won", "yes"}, {"npc", 1.5}}),
    };
    for (int i = 0; i < kMessageTypeCount; ++i) {
        for (const nlohmann::json& shape : shapes) {
            CAPTURE(messageTypeToString(static_cast<MessageType>(i)));
            CAPTURE(shape.dump());
            const auto decoded =
                decodeMessage(encodeMessage(static_cast<MessageType>(i), shape));
            REQUIRE(decoded.has_value());
            // Everything a handler would pull off this payload.
            CHECK_NOTHROW(llm_npc::playerPoseFromJson(decoded->payload));
            CHECK_NOTHROW(llm_npc::netNpcPoseFromJson(decoded->payload));
            CHECK_NOTHROW(voteStateFromJson(decoded->payload));
            CHECK_NOTHROW(voteResolvedFromJson(decoded->payload));
            CHECK_NOTHROW(matchOverFromJson(decoded->payload));
        }
    }
}
