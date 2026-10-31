#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NetFraming.hpp"
#include "NetMessage.hpp"
#include "NetSocket.hpp"

namespace llm_npc {

// Joining player's side of a multiplayer session. Connects to a host,
// streams the local player's input up, and hands decoded server messages
// (WorldSnapshots, chat deltas/replies, mood updates) to the game loop via
// poll() — the same drain-on-the-main-thread shape as LlmClient's
// drainDeltas()/drainReplies(), so main.cpp integrates it the familiar way.
// The client never simulates NPCs; it renders whatever the server says.
class NetClient {
   public:
    NetClient() = default;
    ~NetClient();

    NetClient(const NetClient&) = delete;
    NetClient& operator=(const NetClient&) = delete;

    // Connects and performs the JoinRequest/Welcome handshake (blocking,
    // ~4s connect timeout, 5s handshake read timeout). Returns false with
    // lastError() set on unreachable host, wrong join code, version
    // mismatch, or a full session — the Menu shows that text and returns to
    // the join page (plan edge cases).
    bool connect(const std::string& host, int port,
                 const std::string& playerName, const std::string& joinCode);

    // Sends Disconnect (best effort) and closes. Safe to call twice.
    void disconnect();

    // True after a successful handshake until the socket drops.
    bool connected() const { return connected_.load(); }

    // Player id the server assigned in Welcome; -1 before that.
    int playerId() const { return playerId_; }

    // Sends this frame's local movement to the server as PlayerInput.
    void sendInput(const Vec3& position, float facingDeg);

    // Sends ChatOpen / the player's typed line for the NPC being talked to.
    void sendChatOpen(int npcIndex);
    void sendChatLine(int npcIndex, const std::string& text);

    // Drains every message the reader thread decoded since the last call, in
    // arrival order. Non-blocking; called once per frame by the game loop.
    std::vector<NetMessage> poll();

    // Human-readable reason the last connect() failed or the link dropped.
    std::string lastError() const;

   private:
    // Receives into the framing buffer and pushes decoded messages to
    // inbox_; drops the link on a framing violation or socket error.
    void readerLoop();

    // Frames + writes one message; marks the link dead on failure.
    void send(MessageType type, nlohmann::json payload);

    void setError(std::string message);

    std::atomic<bool> connected_{false};
    int playerId_ = -1;

    socket_t socket_ = kInvalidSocket;
    FrameAssembler framing_;
    std::thread readerThread_;
    std::mutex sendMutex_;

    mutable std::mutex errorMutex_;
    std::string lastError_;

    std::mutex inboxMutex_;
    std::vector<NetMessage> inbox_;
};

}  // namespace llm_npc
