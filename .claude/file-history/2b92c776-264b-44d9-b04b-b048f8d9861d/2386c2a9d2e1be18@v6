#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NetMessage.hpp"
#include "NetSocket.hpp"

namespace llm_npc {

// Authoritative multiplayer host: accepts up to kMaxPlayers-1 remote players,
// applies their PlayerInput, and broadcasts WorldSnapshots at a fixed tick
// rate (plan: .claude/plans/multiplayer-and-aws-deploy.md).
//
// Thread contract: the server's own threads (accept, per-connection readers,
// tick) touch only sockets and internal queues. The game simulation stays on
// the host's main loop, which feeds state in and pulls events out each frame:
//
//   server.setHostPose(playerPos, yaw);          // host avatar for snapshots
//   server.publishNpcPoses(posesFromWorld);      // NPC state for snapshots
//   for (auto& ev : server.drainChatEvents()) {  // remote NPC chat (issue 4)
//       ... route through Npc/LlmClient, then sendToPlayer()/broadcast() ...
//   }
//
// This keeps World and LlmClient single-threaded exactly as in solo play —
// no locks leak into game logic. rendering-free so the loopback tests can drive
// it headless.
class NetServer {
   public:
    struct Settings {
        int port = 0;             // 0 = pick any free port (tests); host UI sets a real one
        std::string joinCode;     // empty = no code required
        float snapshotHz = 12.f;  // WorldSnapshot broadcast rate
        std::string hostName = "host";  // shown as player 0's name
    };

    explicit NetServer(Settings settings);
    ~NetServer();

    NetServer(const NetServer&) = delete;
    NetServer& operator=(const NetServer&) = delete;

    // Binds, listens, and starts the accept + tick threads. Returns false
    // with lastError() set when the port cannot be bound.
    bool start();

    // Closes every connection (best-effort Disconnect first) and joins all
    // threads. Safe to call twice; the destructor calls it.
    void stop();

    // The port actually bound (differs from Settings::port when it was 0).
    int port() const { return port_; }

    // Connected remote players (excludes the local host player).
    int playerCount() const;

    // Human-readable reason start() failed.
    const std::string& lastError() const { return lastError_; }

    // Called by the host's game loop each frame with the host player's own
    // pose, so it appears in broadcast snapshots like everyone else's.
    void setHostPose(const Vec3& position, float facingDeg);

    // Latest NPC state for snapshots; the host loop samples its World and
    // publishes here (the tick thread never touches World directly).
    void publishNpcPoses(std::vector<NetNpcPose> poses);

    // Current poses of every connected remote player, for the host's own
    // renderer (remote players are drawn from here, not from snapshots —
    // the host never receives its own broadcast).
    std::vector<PlayerPose> remotePlayerPoses() const;

    // One remote player's NPC-chat action, drained on the host loop.
    struct ChatEvent {
        int playerId = -1;
        bool open = false;  // true: opened conversation; false: typed a line
        int npcIndex = -1;
        std::string text;   // the typed line when !open
    };

    // Pops all chat events received since the last call (arrival order).
    std::vector<ChatEvent> drainChatEvents();

    // Sends one message to one player. False when that player is gone —
    // callers just drop the payload (their conversation died with them).
    bool sendToPlayer(int playerId, MessageType type, nlohmann::json payload);

    // Sends one message to every connected player.
    void broadcast(MessageType type, nlohmann::json payload);

   private:
    // One connected remote player. Owned by connections_; the reader thread
    // marks itself dead and the tick thread reaps.
    struct Connection;

    void acceptLoop();
    void readerLoop(Connection& conn);
    void tickLoop();

    // Reads the JoinRequest and answers Welcome (or a refusal). True when
    // the player was admitted and conn is fully initialized.
    bool handshake(Connection& conn);

    // Builds the WorldSnapshot payload from the latest published state.
    nlohmann::json buildSnapshot();

    void handleMessage(Connection& conn, const NetMessage& msg);

    // Frames + sends on one connection; marks it dead on failure.
    bool sendOn(Connection& conn, MessageType type, nlohmann::json payload);

    Settings settings_;

    socket_t listenSocket_;  // see NetSocket.hpp
    int port_ = 0;
    std::string lastError_;
    std::atomic<bool> running_{false};

    mutable std::mutex connectionsMutex_;
    std::vector<std::unique_ptr<Connection>> connections_;

    std::mutex stateMutex_;  // host pose + published NPC poses + tick counter
    PlayerPose hostPose_;
    std::vector<NetNpcPose> npcPoses_;
    std::uint64_t tick_ = 0;

    std::mutex chatMutex_;
    std::vector<ChatEvent> chatEvents_;

    std::condition_variable tickCv_;  // wakes tickLoop early on stop()
    std::mutex tickMutex_;

    std::thread acceptThread_;
    std::thread tickThread_;
};

}  // namespace llm_npc
