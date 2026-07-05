#include "NetServer.hpp"

#include <chrono>
#include <cstring>

#include "NetFraming.hpp"

namespace llm_npc {

namespace {
// A joiner gets this long to send its JoinRequest before we give up on it.
constexpr int kHandshakeTimeoutMs = 5000;
}  // namespace

struct NetServer::Connection {
    socket_t socket = kInvalidSocket;
    int playerId = -1;
    std::string name;
    std::atomic<bool> alive{true};

    std::mutex sendMutex;   // frames from tick/host threads interleave here
    std::mutex poseMutex;   // reader writes PlayerInput, tick reads
    PlayerPose pose;

    FrameAssembler framing;
    std::thread reader;
};

NetServer::NetServer(Settings settings)
    : settings_(std::move(settings)), listenSocket_(kInvalidSocket) {
    hostPose_.playerId = 0;
    hostPose_.name = settings_.hostName;
}

NetServer::~NetServer() { stop(); }

bool NetServer::start() {
    ensureSocketsInit();

    listenSocket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket_ == kInvalidSocket) {
        lastError_ = "could not create listen socket";
        return false;
    }
    configureSocket(listenSocket_);
    int one = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one),
               sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(settings_.port));
    if (::bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        lastError_ = "could not bind port " + std::to_string(settings_.port);
        closeSocket(listenSocket_);
        listenSocket_ = kInvalidSocket;
        return false;
    }
    if (::listen(listenSocket_, kMaxPlayers) != 0) {
        lastError_ = "could not listen on port " + std::to_string(settings_.port);
        closeSocket(listenSocket_);
        listenSocket_ = kInvalidSocket;
        return false;
    }

    // Learn the real port when 0 asked the OS to pick one.
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(listenSocket_, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
        port_ = static_cast<int>(ntohs(bound.sin_port));
    } else {
        port_ = settings_.port;
    }

    running_ = true;
    acceptThread_ = std::thread([this] { acceptLoop(); });
    tickThread_ = std::thread([this] { tickLoop(); });
    return true;
}

void NetServer::stop() {
    if (!running_.exchange(false)) {
        // start() may have failed after creating the socket; still close it.
        if (listenSocket_ != kInvalidSocket) {
            closeSocket(listenSocket_);
            listenSocket_ = kInvalidSocket;
        }
        return;
    }

    // Unblock accept(), then the tick sleeper.
    shutdownSocket(listenSocket_);
    closeSocket(listenSocket_);
    listenSocket_ = kInvalidSocket;
    tickCv_.notify_all();

    if (acceptThread_.joinable()) acceptThread_.join();
    if (tickThread_.joinable()) tickThread_.join();

    // Now no thread is creating connections; tell everyone goodbye and
    // unblock their readers.
    std::vector<std::unique_ptr<Connection>> doomed;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        doomed.swap(connections_);
    }
    for (auto& conn : doomed) {
        sendOn(*conn, MessageType::Disconnect, {{"reason", "server closed"}});
        conn->alive = false;
        shutdownSocket(conn->socket);
        if (conn->reader.joinable()) conn->reader.join();
        closeSocket(conn->socket);
    }
}

int NetServer::playerCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    int count = 0;
    for (const auto& conn : connections_) {
        if (conn->alive && conn->playerId >= 0) ++count;
    }
    return count;
}

void NetServer::setHostPose(const Vec3& position, float facingDeg) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    hostPose_.position = position;
    hostPose_.facingDeg = facingDeg;
}

void NetServer::publishNpcPoses(std::vector<NpcPose> poses) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    npcPoses_ = std::move(poses);
}

std::vector<NetServer::ChatEvent> NetServer::drainChatEvents() {
    std::vector<ChatEvent> out;
    std::lock_guard<std::mutex> lock(chatMutex_);
    out.swap(chatEvents_);
    return out;
}

bool NetServer::sendToPlayer(int playerId, MessageType type, nlohmann::json payload) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto& conn : connections_) {
        if (conn->playerId == playerId && conn->alive) {
            return sendOn(*conn, type, std::move(payload));
        }
    }
    return false;
}

void NetServer::broadcast(MessageType type, nlohmann::json payload) {
    const std::string wire = FrameAssembler::encode(encodeMessage(type, std::move(payload)));
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (auto& conn : connections_) {
        if (!conn->alive || conn->playerId < 0) continue;
        std::lock_guard<std::mutex> sendLock(conn->sendMutex);
        if (!sendAll(conn->socket, wire)) conn->alive = false;
    }
}

bool NetServer::sendOn(Connection& conn, MessageType type, nlohmann::json payload) {
    const std::string wire = FrameAssembler::encode(encodeMessage(type, std::move(payload)));
    std::lock_guard<std::mutex> lock(conn.sendMutex);
    if (!sendAll(conn.socket, wire)) {
        conn.alive = false;
        return false;
    }
    return true;
}

void NetServer::acceptLoop() {
    while (running_) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const socket_t accepted =
            ::accept(listenSocket_, reinterpret_cast<sockaddr*>(&peer), &len);
        if (accepted == kInvalidSocket) {
            if (!running_) return;  // stop() closed the listen socket
            continue;               // transient error; keep serving
        }
        configureSocket(accepted);

        auto conn = std::make_unique<Connection>();
        conn->socket = accepted;
        Connection* raw = conn.get();
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_.push_back(std::move(conn));
        }
        // Handshake runs on the connection's own reader thread so a slow
        // joiner can't stall the accept loop.
        raw->reader = std::thread([this, raw] { readerLoop(*raw); });
    }
}

bool NetServer::handshake(Connection& conn) {
    setRecvTimeoutMs(conn.socket, kHandshakeTimeoutMs);

    // Collect frames until the first complete message (or timeout/violation).
    std::string joinPayload;
    while (joinPayload.empty()) {
        char buf[1024];
        const auto n = ::recv(conn.socket, buf, sizeof(buf), 0);
        if (n <= 0) return false;  // timeout, closed, or error
        const bool ok = conn.framing.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                                          [&](const std::string& frame) {
                                              if (joinPayload.empty()) joinPayload = frame;
                                          });
        if (!ok) return false;
    }

    auto msg = decodeMessage(joinPayload);
    if (!msg || msg->type != MessageType::JoinRequest) return false;

    const int version = msg->payload.value("version", -1);
    if (version != kNetProtocolVersion) {
        sendOn(conn, MessageType::Welcome,
               {{"accepted", false},
                {"reason", "version mismatch: server speaks " +
                               std::to_string(kNetProtocolVersion) + ", you sent " +
                               std::to_string(version) + " — update the game"}});
        return false;
    }
    if (!settings_.joinCode.empty() &&
        msg->payload.value("code", std::string{}) != settings_.joinCode) {
        sendOn(conn, MessageType::Welcome,
               {{"accepted", false}, {"reason", "wrong join code"}});
        return false;
    }

    // Assign the lowest free remote player id (1..kMaxPlayers-1); the host
    // is always player 0. Full when none is free.
    int assigned = -1;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (int candidate = 1; candidate < kMaxPlayers; ++candidate) {
            bool used = false;
            for (const auto& other : connections_) {
                if (other->alive && other->playerId == candidate) used = true;
            }
            if (!used) {
                assigned = candidate;
                break;
            }
        }
        if (assigned != -1) {
            conn.name = msg->payload.value("name", "player" + std::to_string(assigned));
            conn.playerId = assigned;  // set under the lock; playerCount sees it
            std::lock_guard<std::mutex> poseLock(conn.poseMutex);
            conn.pose.playerId = assigned;
            conn.pose.name = conn.name;
        }
    }
    if (assigned == -1) {
        sendOn(conn, MessageType::Welcome,
               {{"accepted", false}, {"reason", "session full"}});
        return false;
    }

    setRecvTimeoutMs(conn.socket, 0);  // back to blocking reads
    return sendOn(conn, MessageType::Welcome,
                  {{"accepted", true}, {"playerId", assigned}});
}

void NetServer::readerLoop(Connection& conn) {
    if (!handshake(conn)) {
        // Only mark dead — the tick thread's reaper owns closing the socket
        // (after joining this thread), so a recycled fd can never be
        // shut down by mistake.
        conn.alive = false;
        conn.playerId = -1;
        return;
    }

    while (running_ && conn.alive) {
        char buf[4096];
        const auto n = ::recv(conn.socket, buf, sizeof(buf), 0);
        if (n <= 0) break;  // disconnected (or stop() shut the socket down)
        const bool ok = conn.framing.feed(
            std::string_view(buf, static_cast<std::size_t>(n)),
            [&](const std::string& frame) {
                if (auto msg = decodeMessage(frame)) {
                    handleMessage(conn, *msg);
                } else {
                    conn.alive = false;  // garbage on the wire; drop them
                }
            });
        if (!ok) break;  // framing violation; stream unrecoverable
    }
    conn.alive = false;
}

void NetServer::handleMessage(Connection& conn, const NetMessage& msg) {
    switch (msg.type) {
        case MessageType::PlayerInput: {
            std::lock_guard<std::mutex> lock(conn.poseMutex);
            if (msg.payload.contains("pos")) {
                conn.pose.position = vec3FromJson(msg.payload["pos"]);
            }
            conn.pose.facingDeg = msg.payload.value("facing", conn.pose.facingDeg);
            break;
        }
        case MessageType::ChatOpen:
        case MessageType::ChatLine: {
            ChatEvent ev;
            ev.playerId = conn.playerId;
            ev.open = (msg.type == MessageType::ChatOpen);
            ev.npcIndex = msg.payload.value("npc", -1);
            ev.text = msg.payload.value("text", std::string{});
            std::lock_guard<std::mutex> lock(chatMutex_);
            chatEvents_.push_back(std::move(ev));
            break;
        }
        case MessageType::Disconnect:
            conn.alive = false;
            break;
        default:
            // Clients have no business sending server->client types; ignore
            // rather than kill (forward compatibility within a version).
            break;
    }
}

nlohmann::json NetServer::buildSnapshot() {
    nlohmann::json players = nlohmann::json::array();
    nlohmann::json npcs = nlohmann::json::array();
    std::uint64_t tick = 0;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        players.push_back(playerPoseToJson(hostPose_));
        for (const auto& npc : npcPoses_) npcs.push_back(npcPoseToJson(npc));
        tick = ++tick_;
    }
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto& conn : connections_) {
            if (!conn->alive || conn->playerId < 0) continue;
            std::lock_guard<std::mutex> poseLock(conn->poseMutex);
            players.push_back(playerPoseToJson(conn->pose));
        }
    }
    return {{"tick", tick}, {"players", players}, {"npcs", npcs}};
}

void NetServer::tickLoop() {
    const auto period = std::chrono::duration<double>(1.0 / settings_.snapshotHz);
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(tickMutex_);
            tickCv_.wait_for(lock, period, [this] { return !running_.load(); });
        }
        if (!running_) return;

        broadcast(MessageType::WorldSnapshot, buildSnapshot());

        // Reap connections whose reader has finished (failed handshake or
        // disconnect) so their player id frees up and threads get joined.
        std::vector<std::unique_ptr<Connection>> dead;
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            for (auto it = connections_.begin(); it != connections_.end();) {
                if (!(*it)->alive) {
                    dead.push_back(std::move(*it));
                    it = connections_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& conn : dead) {
            if (conn->socket != kInvalidSocket) shutdownSocket(conn->socket);
            if (conn->reader.joinable()) conn->reader.join();
            if (conn->socket != kInvalidSocket) closeSocket(conn->socket);
        }
    }
}

}  // namespace llm_npc
