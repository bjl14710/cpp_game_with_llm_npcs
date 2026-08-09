// Pushing a host-built map to everyone already connected (plan:
// retire-worldgen-sandbox-mode, steps 4-5).
//
// The name-mapping cases are LIVE — adding an enum value without its
// kTypeNames row is a silent, total protocol break, and that is worth catching
// from the first commit rather than after the payload exists. Everything about
// applying a received map is skipped until step 5.
#include <string>

#include "NetMessage.hpp"
#include "SandboxMap.hpp"
#include "doctest.h"

using namespace llm_npc;

// ---- the message type itself (LIVE) --------------------------------------

TEST_CASE("SandboxMapSync round-trips through its wire name") {
    // kTypeNames indexes by enum value. A new enum value without a matching
    // row does not fail to compile — it reads whatever is at that index, or
    // past the end of the array. This is the case that catches it.
    CHECK(std::string(messageTypeToString(MessageType::SandboxMapSync)) ==
          "SandboxMapSync");

    const auto parsed = messageTypeFromString("SandboxMapSync");
    REQUIRE(parsed.has_value());
    CHECK(*parsed == MessageType::SandboxMapSync);
}

TEST_CASE("every message type has a distinct wire name") {
    // The general form of the bug above: two types sharing a name, or a name
    // shifted by one, makes messageTypeFromString return the wrong type and
    // two builds disagree about what a frame means.
    for (int i = 0; i <= static_cast<int>(MessageType::SandboxMapSync); ++i) {
        const auto type = static_cast<MessageType>(i);
        const std::string name = messageTypeToString(type);
        CAPTURE(name);
        CHECK_FALSE(name.empty());
        const auto back = messageTypeFromString(name);
        REQUIRE(back.has_value());
        CHECK(*back == type);
    }
}

TEST_CASE("an unknown type name is rejected rather than guessed") {
    // A newer peer's message must fail loudly at decode instead of desyncing.
    CHECK_FALSE(messageTypeFromString("SandboxMapSyncV2").has_value());
    CHECK_FALSE(messageTypeFromString("").has_value());
}

// ---- carrying a map (step 4) ---------------------------------------------

TEST_CASE("a map survives encode and decode unchanged" * doctest::skip()) {
    // TODO(worldgen step 4): build a SandboxMap, encode it into a
    // SandboxMapSync payload, decode, and compare the rebuilt map field for
    // field. SandboxMap::toJson/fromJson already exist and are tested — this
    // is about the envelope, not the map format.
}

TEST_CASE("a malformed map payload is rejected, not partially applied" *
          doctest::skip()) {
    // TODO(worldgen step 4): truncated JSON, a missing "map" field, and a map
    // whose "version" is newer than this build. fromJson already rejects a
    // newer version; this pins that the message layer surfaces that as a
    // refusal rather than an empty world.
}

TEST_CASE("the client validates a received map before building it" *
          doctest::skip()) {
    // TODO(worldgen step 5): VALIDATE ON THE CLIENT TOO. Never trust a peer's
    // payload to be well-formed just because the host sent it — run
    // validateMap and keep the current world when it reports errors.
}

TEST_CASE("an oversized map payload is refused at validation" *
          doctest::skip()) {
    // TODO(worldgen step 4): NetFraming is length-prefixed and handles large
    // frames, so the cap is a policy decision rather than a protocol limit.
    // Cap it at validation and reject beyond it.
}

// ---- applying it to a live session (step 5) ------------------------------

TEST_CASE("players respawn at the new map's spawn point on swap" *
          doctest::skip()) {
    // TODO(worldgen step 5): THE HARD PART, and it is behaviour rather than an
    // edge case. Every connected player is standing at a position that may now
    // be inside a wall, so a swap respawns them.
}

TEST_CASE("a client that rejects the map keeps its current world" *
          doctest::skip()) {
    // TODO(worldgen step 5): the failure path of the case above. A refused
    // sync must leave the client playable, not empty.
}

TEST_CASE("a map swap closes any open conversation" * doctest::skip()) {
    // TODO(worldgen step 5): the world changed underneath it. Listed in the
    // plan's edge cases.
}
