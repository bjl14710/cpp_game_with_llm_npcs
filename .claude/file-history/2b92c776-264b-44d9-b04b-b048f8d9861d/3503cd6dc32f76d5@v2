#include "NetServer.hpp"

namespace llm_npc {

// TODO(implement): struct NetServer::Connection { socket fd, playerId, name,
// FrameAssembler, reader thread, per-request routing for ChatDelta/ChatReply }.
struct NetServer::Connection {};

NetServer::NetServer(Settings settings, World& world, LlmClient& client)
    : settings_(std::move(settings)), world_(world), client_(client) {}

NetServer::~NetServer() { stop(); }

bool NetServer::start() {
    // TODO(implement): create/bind/listen a TCP socket on settings_.port
    // (INADDR_ANY), record the bound port in port_, start acceptThread_ and
    // tickThread_. On failure set lastError_ and return false.
    lastError_ = "NetServer::start not implemented yet";
    return false;
}

void NetServer::stop() {
    // TODO(implement): set running_=false, close the listen socket and every
    // connection (send Disconnect first), join acceptThread_/tickThread_.
}

int NetServer::playerCount() const {
    // TODO(implement): size of connections_ under connectionsMutex_.
    return 0;
}

void NetServer::setHostPose(const Vec3& position, float facingDeg) {
    // TODO(implement): stash for the next WorldSnapshot broadcast.
    (void)position;
    (void)facingDeg;
}

}  // namespace llm_npc
