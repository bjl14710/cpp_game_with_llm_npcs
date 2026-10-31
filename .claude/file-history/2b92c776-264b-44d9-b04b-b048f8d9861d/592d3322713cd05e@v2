// Tests for NetMessage encode/decode — the JSON wire protocol for
// multiplayer. All stubs are * doctest::skip() until the feature is
// implemented (plan: .claude/plans/multiplayer-and-aws-deploy.md).
#include <string>

#include "NetMessage.hpp"
#include "doctest.h"

using llm_npc::decodeMessage;
using llm_npc::encodeMessage;
using llm_npc::MessageType;
using llm_npc::NetMessage;

TEST_CASE("encodeMessage/decodeMessage round-trips every message type" * doctest::skip()) {
    // For each MessageType, encode a payload with representative fields and
    // decode it back: type tag and fields survive unchanged.
}

TEST_CASE("decodeMessage rejects invalid JSON" * doctest::skip()) {
    // "not json at all" -> nullopt, no throw (parse with
    // allow_exceptions=false, same as parseOllamaChunk).
}

TEST_CASE("decodeMessage rejects an unknown or missing type tag" * doctest::skip()) {
    // {"type":"FromTheFuture"} and {"x":1} both -> nullopt, so a newer peer's
    // messages fail loudly at the handshake instead of desyncing.
}

TEST_CASE("vec3ToJson/vec3FromJson round-trips coordinates" * doctest::skip()) {
    // Including negatives and fractional values.
}

TEST_CASE("JoinRequest carries the protocol version for the handshake" * doctest::skip()) {
    // Encoded JoinRequest includes kNetProtocolVersion; the server-side
    // version check is what rejects mismatched builds (plan edge case:
    // host/client running different game builds).
}

TEST_CASE("WorldSnapshot round-trips player and NPC poses" * doctest::skip()) {
    // A snapshot with 2 PlayerPoses and 3 NpcPoses (position, facing, mood,
    // behavior ints) decodes to identical values.
}
