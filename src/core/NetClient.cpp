#include "NetClient.hpp"

namespace llm_npc {

NetClient::~NetClient() { disconnect(); }

bool NetClient::connect(const std::string& host, int port,
                        const std::string& playerName, const std::string& joinCode) {
    // TODO(implement): TCP connect with timeout; send JoinRequest
    // {kNetProtocolVersion, playerName, joinCode}; block for Welcome; on
    // accept store playerId_ and start readerThread_, else set lastError_
    // from the refusal reason.
    (void)host;
    (void)port;
    (void)playerName;
    (void)joinCode;
    lastError_ = "NetClient::connect not implemented yet";
    return false;
}

void NetClient::disconnect() {
    // TODO(implement): best-effort Disconnect message, close the socket,
    // join readerThread_, connected_=false.
}

void NetClient::sendInput(const Vec3& position, float facingDeg) {
    // TODO(implement)
    (void)position;
    (void)facingDeg;
}

void NetClient::sendChatOpen(int npcIndex) {
    // TODO(implement)
    (void)npcIndex;
}

void NetClient::sendChatLine(int npcIndex, const std::string& text) {
    // TODO(implement)
    (void)npcIndex;
    (void)text;
}

std::vector<NetMessage> NetClient::poll() {
    // TODO(implement): swap inbox_ out under inboxMutex_ (same pattern as
    // LlmClient::drainReplies).
    return {};
}

}  // namespace llm_npc
