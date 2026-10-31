// Loopback integration tests: NetServer + NetClient over 127.0.0.1 with the
// LLM served by FakeOllama (tests/FakeOllama.hpp), so no real Ollama or
// network is needed. All stubs are * doctest::skip() until the feature is
// implemented (plan: .claude/plans/multiplayer-and-aws-deploy.md).
//
// Each stub maps to an acceptance criterion in the plan.
#include <string>

#include "NetClient.hpp"
#include "NetServer.hpp"
#include "doctest.h"

// TODO(implement): shared fixture — build a small World (City + 2-3 Npcs with
// test personas), an LlmClient pointed at a FakeOllama, and a NetServer on
// port 0; helper to connect N NetClients and pump poll() until a predicate
// holds or a deadline passes (no sleeps-and-hope).

TEST_CASE("client joins and both sides see two players in the snapshot" * doctest::skip()) {
    // Acceptance: given a host on port P, a client joining 127.0.0.1:P
    // completes the handshake, gets a player id, and the next WorldSnapshot
    // lists both players' poses.
}

TEST_CASE("player movement propagates through server snapshots" * doctest::skip()) {
    // Client sends sendInput() with a new position; the host's subsequent
    // broadcast shows that player moved (server is authoritative).
}

TEST_CASE("shared NPC chat updates mood for the bystander too" * doctest::skip()) {
    // Acceptance: client A chats with NPC 0 (FakeOllama streams a reply with
    // a [[MOOD:...]] directive); A receives ChatDelta/ChatReply, and client B
    // — who never opened the chat — receives NpcMoodUpdate/NpcSpeechBubble
    // for NPC 0.
}

TEST_CASE("connect to an unreachable address fails cleanly" * doctest::skip()) {
    // Acceptance: NetClient::connect to a closed port returns false with a
    // non-empty lastError() and no crash; connected() stays false.
}

TEST_CASE("LLM failure is reported only to the requesting client" * doctest::skip()) {
    // Acceptance: FakeOllama in Http500 mode — client A's ChatReply carries
    // the error text, client B receives nothing chat-related and stays
    // connected.
}

TEST_CASE("two clients messaging the same NPC both get answered in order" * doctest::skip()) {
    // Acceptance: concurrent ChatLines to NPC 0 serialize through the single
    // LlmClient worker; each client gets exactly its own reply, no
    // interleaving or crash.
}

TEST_CASE("a fifth join attempt is rejected with session full" * doctest::skip()) {
    // Plan edge case: with kMaxPlayers already in (host + 3), the next
    // connect() fails with a "session full" reason.
}

TEST_CASE("protocol version mismatch is rejected at the handshake" * doctest::skip()) {
    // Plan edge case: a JoinRequest carrying a different version gets a
    // refusal Welcome, not a silent desync.
}

TEST_CASE("client disconnect mid-conversation leaves the server healthy" * doctest::skip()) {
    // Plan edge case: A disconnects while its reply streams; the server drops
    // A's in-flight routing, B can immediately chat with the same NPC.
}
