#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NetFraming.hpp"
#include "NetMessage.hpp"

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

    // Connects and performs the JoinRequest/Welcome handshake (blocking, with
    // a timeout). Returns false with lastError() set on unreachable host,
    // wrong join code, version mismatch, or a full session — the Menu shows
    // that text and returns to the join page (plan edge cases).
    // TODO(implement): raw TCP connect + handshake, then start readerLoop.
    bool connect(const std::string& host, int port,
                 const std::string& playerName, const std::string& joinCode);

    // Sends Disconnect (best effort) and closes. Safe to call twice.
    // TODO(implement)
    void disconnect();

    // True after a successful handshake until the socket drops.
    bool connected() const { return connected_.load(); }

    // Player id the server assigned in Welcome; -1 before that.
    int playerId() const { return playerId_; }

    // Sends this frame's local movement to the server as PlayerInput.
    // TODO(implement)
    void sendInput(const Vec3& position, float facingDeg);

    // Sends ChatOpen / the player's typed line for the NPC being talked to.
    // TODO(implement)
    void sendChatOpen(int npcIndex);
    void sendChatLine(int npcIndex, const std::string& text);

    // Drains every message the reader thread decoded since the last call, in
    // arrival order. Non-blocking; called once per frame by the game loop.
    // TODO(implement)
    std::vector<NetMessage> poll();

    // Human-readable reason the last connect() failed or the link dropped.
    const std::string& lastError() const { return lastError_; }

   private:
    // TODO(implement): readerLoop() — recv into FrameAssembler, decodeMessage,
    // push to inbox_ under inboxMutex_; drop the link on a framing violation.
    // TODO(implement): send(MessageType, payload) — encodeMessage + frame +
    // write, serialized by a send mutex.

    std::atomic<bool> connected_{false};
    int playerId_ = -1;
    std::string lastError_;

    FrameAssembler framing_;
    std::thread readerThread_;

    std::mutex inboxMutex_;
    std::vector<NetMessage> inbox_;
};

}  // namespace llm_npc
