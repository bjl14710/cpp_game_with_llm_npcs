#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "LlmClient.hpp"
#include "NetMessage.hpp"
#include "World.hpp"

namespace llm_npc {

// Authoritative multiplayer host. Owns the one true copy of the World and the
// single LlmClient: remote players send PlayerInput, the server steps the
// simulation and broadcasts WorldSnapshots at a fixed tick rate, and all NPC
// dialogue is routed through here so every player sees the same NPC mood and
// memory (plan: .claude/plans/multiplayer-and-aws-deploy.md).
//
// Runs inside the hosting player's game process; the host is player 0 and
// feeds its input directly rather than over a socket. SFML-free like the rest
// of src/core so the loopback integration test can drive it headless.
class NetServer {
   public:
    struct Settings {
        int port = 0;               // 0 = pick any free port (tests); host UI sets a real one
        std::string joinCode;       // empty = no code required
        float snapshotHz = 12.f;    // WorldSnapshot broadcast rate
    };

    // `world` and `client` outlive the server; main.cpp constructs them
    // exactly as single-player does and hands them over when hosting.
    NetServer(Settings settings, World& world, LlmClient& client);
    ~NetServer();

    NetServer(const NetServer&) = delete;
    NetServer& operator=(const NetServer&) = delete;

    // Binds, listens, and starts the accept + tick threads. Returns false
    // with lastError() set when the port cannot be bound.
    // TODO(implement): raw BSD/Winsock listen socket (pattern: what
    // external/httplib.h does for cross-platform sockets — zero new deps).
    bool start();

    // Stops all threads and closes every connection with a Disconnect
    // message. Safe to call twice.
    // TODO(implement)
    void stop();

    // The port actually bound (differs from Settings::port when it was 0).
    int port() const { return port_; }

    // Connected remote players (excludes the local host player).
    // TODO(implement)
    int playerCount() const;

    // Human-readable reason start() or a client handshake failed.
    const std::string& lastError() const { return lastError_; }

    // Called by the host's game loop each frame with the host player's own
    // pose, so it appears in broadcast snapshots like everyone else's.
    // TODO(implement)
    void setHostPose(const Vec3& position, float facingDeg);

   private:
    // One connected remote player: socket, identity, and their NPC chat
    // in-flight state (which LlmClient request id is theirs, so ChatDelta /
    // ChatReply route back to the right client only — see plan edge cases).
    struct Connection;  // TODO(implement): defined in NetServer.cpp

    // TODO(implement): acceptLoop() — accept, JoinRequest/Welcome handshake
    //   (protocol version + join code + kMaxPlayers check), spawn a reader.
    // TODO(implement): readerLoop(Connection&) — FrameAssembler::feed ->
    //   decodeMessage -> apply PlayerInput / route ChatOpen + ChatLine.
    // TODO(implement): tickLoop() — step world_.npcs() update, drain
    //   client_.drainDeltas()/drainReplies() to the owning connection,
    //   broadcast WorldSnapshot + NpcMoodUpdate/NpcSpeechBubble at snapshotHz.
    // TODO(implement): serialize concurrent ChatLine to one NPC: LlmClient's
    //   single worker already queues; track per-NPC busy state and answer
    //   the second player with an "NPC is busy" indicator (plan edge case).

    Settings settings_;
    World& world_;
    LlmClient& client_;

    int port_ = 0;
    std::string lastError_;
    std::atomic<bool> running_{false};

    std::mutex connectionsMutex_;
    std::vector<std::unique_ptr<Connection>> connections_;

    std::thread acceptThread_;
    std::thread tickThread_;
};

}  // namespace llm_npc
