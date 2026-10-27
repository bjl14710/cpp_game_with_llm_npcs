#include "NetServer.hpp"

#include <chrono>
#include <cstring>
#include <iostream>

#include "NetFraming.hpp"

namespace llm_npc {

namespace {
// Pause after a failed accept(). EMFILE/ENFILE fail instantly and keep
// failing while the connection waits in the backlog; without a pause the
// accept loop spins a core until a descriptor frees up.
constexpr auto kAcceptBackoff = std::chrono::milliseconds(50);

// listen() backlog: enough for a burst of joiners plus a few strays.
constexpr int kListenBacklog = 2 * NetServer::kMaxConnections;

// The refusal a joiner sees when its name fails isValidPlayerName().
const std::string kBadNameReason =
    "name must be at most " + std::to_string(kMaxPlayerNameChars) +
    " printable ASCII characters";
}  // namespace

struct NetServer::Connection {
    socket_t socket = kInvalidSocket;
    int playerId = -1;      // reserved id; written under connectionsMutex_
    bool admitted = false;  // Welcome sent; guarded by connectionsMutex_
    std::string name;
    std::atomic<bool> alive{true};

    std::mutex sendMutex;   // frames from tick/host threads interleave here
    std::mutex poseMutex;   // reader writes PlayerInput, tick reads
    PlayerPose pose;

    FrameAssembler framing;
    std::thread reader;

    // Every owner that drops a Connection joins `reader` first (reaper,
    // stop()), so only the socket is left to release here.
    ~Connection() {
        if (socket != kInvalidSocket) closeSocket(socket);
    }
};

NetServer::NetServer(Settings settings)
    : settings_(std::move(settings)), listenSocket_(kInvalidSocket) {
    hostPose_.playerId = 0;
    hostPose_.name = settings_.hostName;
}

NetServer::~NetServer() { stop(); }

bool NetServer::start() {
    ensureSocketsInit();

    // Built in a local and published once ready: acceptLoop and stop() only
    // ever see a fully set-up listener (or none).
    const socket_t listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener == kInvalidSocket) {
        lastError_ = "could not create listen socket";
        return false;
    }
    configureSocket(listener);
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one),
               sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<std::uint16_t>(settings_.port));
    if (::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        lastError_ = "could not bind port " + std::to_string(settings_.port);
        closeSocket(listener);
        return false;
    }
    if (::listen(listener, kListenBacklog) != 0) {
        lastError_ = "could not listen on port " + std::to_string(settings_.port);
        closeSocket(listener);
        return false;
    }

    // Learn the real port when 0 asked the OS to pick one.
    sockaddr_in bound{};
    socklen_t len = sizeof(bound);
    if (::getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
        port_ = static_cast<int>(ntohs(bound.sin_port));
    } else {
        port_ = settings_.port;
    }

    listenSocket_ = listener;
    running_ = true;
    acceptThread_ = std::thread([this] { acceptLoop(); });
    tickThread_ = std::thread([this] { tickLoop(); });
    return true;
}

void NetServer::stop() {
    if (!running_.exchange(false)) return;  // never started, or already stopped

    // Unblock accept(), then the tick sleeper.
    const socket_t listener = listenSocket_.exchange(kInvalidSocket);
    shutdownSocket(listener);
    closeSocket(listener);
    tickCv_.notify_all();

    if (acceptThread_.joinable()) acceptThread_.join();
    if (tickThread_.joinable()) tickThread_.join();

    // Now no thread is creating connections; tell everyone goodbye and
    // unblock their readers.
    std::vector<std::shared_ptr<Connection>> doomed;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        doomed.swap(connections_);
    }
    for (auto& conn : doomed) {
        // sendWire skips connections already dead, so a client that stopped
        // reading costs nothing more here.
        sendOn(*conn, MessageType::Disconnect, {{"reason", "server closed"}});
        conn->alive = false;
        shutdownSocket(conn->socket);
        if (conn->reader.joinable()) conn->reader.join();
    }
    // Dropping `doomed` closes the sockets (Connection's destructor).
}

int NetServer::playerCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    int count = 0;
    for (const auto& conn : connections_) {
        if (conn->alive && conn->admitted) ++count;
    }
    return count;
}

void NetServer::setHostPose(const Vec3& position, float facingDeg) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    hostPose_.position = position;
    hostPose_.facingDeg = facingDeg;
}

void NetServer::publishNpcPoses(std::vector<NetNpcPose> poses) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    npcPoses_ = std::move(poses);
}

std::vector<PlayerPose> NetServer::remotePlayerPoses() const {
    std::vector<PlayerPose> out;
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (const auto& conn : connections_) {
        if (!conn->alive || !conn->admitted) continue;
        std::lock_guard<std::mutex> poseLock(conn->poseMutex);
        out.push_back(conn->pose);
    }
    return out;
}

std::vector<NetServer::ChatEvent> NetServer::drainChatEvents() {
    std::vector<ChatEvent> out;
    std::lock_guard<std::mutex> lock(chatMutex_);
    out.swap(chatEvents_);
    return out;
}

bool NetServer::sendToPlayer(int playerId, MessageType type, nlohmann::json payload) {
    std::shared_ptr<Connection> target;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (const auto& conn : connections_) {
            if (conn->playerId == playerId && conn->alive && conn->admitted) {
                target = conn;
                break;
            }
        }
    }
    // Sent outside connectionsMutex_: a slow peer must not stall playerCount()
    // and every other caller for up to the send timeout.
    return target && sendOn(*target, type, std::move(payload));
}

void NetServer::broadcast(MessageType type, nlohmann::json payload) {
    const std::string wire = FrameAssembler::encode(encodeMessage(type, std::move(payload)));
    if (wire.empty()) {
        std::cerr << "[llm_npc] net: dropped an oversized " << messageTypeToString(type)
                  << " broadcast (over the " << kMaxFrameBytes << "-byte frame cap)\n";
        return;
    }
    for (const auto& conn : admittedConnections()) sendWire(*conn, wire);
}

std::vector<std::shared_ptr<NetServer::Connection>> NetServer::admittedConnections() const {
    std::vector<std::shared_ptr<Connection>> out;
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    for (const auto& conn : connections_) {
        if (conn->alive && conn->admitted) out.push_back(conn);
    }
    return out;
}

bool NetServer::sendOn(Connection& conn, MessageType type, nlohmann::json payload) {
    const std::string wire = FrameAssembler::encode(encodeMessage(type, std::move(payload)));
    if (wire.empty()) {
        // The message is the problem, not the link: drop it, keep the player.
        std::cerr << "[llm_npc] net: dropped an oversized " << messageTypeToString(type)
                  << " message (over the " << kMaxFrameBytes << "-byte frame cap)\n";
        return false;
    }
    return sendWire(conn, wire);
}

bool NetServer::sendWire(Connection& conn, const std::string& wire) {
    std::lock_guard<std::mutex> lock(conn.sendMutex);
    if (!conn.alive) return false;  // already failed once; don't wait out another timeout
    if (!sendAll(conn.socket, wire)) {
        conn.alive = false;  // error or SO_SNDTIMEO expiry; the reaper closes it
        return false;
    }
    return true;
}

void NetServer::acceptLoop() {
    while (running_) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const socket_t accepted =
            ::accept(listenSocket_.load(), reinterpret_cast<sockaddr*>(&peer), &len);
        if (accepted == kInvalidSocket) {
            if (!running_) return;  // stop() closed the listen socket
            std::this_thread::sleep_for(kAcceptBackoff);  // e.g. out of fds
            continue;
        }
        configureSocket(accepted);
        setSendTimeoutMs(accepted, settings_.sendTimeoutMs);

        auto conn = std::make_shared<Connection>();
        conn->socket = accepted;
        bool atCap = false;
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            atCap = static_cast<int>(connections_.size()) >= kMaxConnections;
        }
        // Players who just left may still hold slots until the next tick;
        // reap them now rather than refuse a real joiner on their account.
        if (atCap) reapDeadConnections();

        std::lock_guard<std::mutex> lock(connectionsMutex_);
        if (static_cast<int>(connections_.size()) >= kMaxConnections) {
            continue;  // over the cap: `conn` goes out of scope and closes the socket
        }
        // Handshake runs on the connection's own reader thread so a slow
        // joiner can't stall the accept loop. The thread is started while
        // holding connectionsMutex_, which the reaper also takes, so the
        // reaper can never see — and free — a connection whose reader does
        // not exist yet.
        Connection* raw = conn.get();
        raw->reader = std::thread([this, raw] { readerLoop(*raw); });
        connections_.push_back(std::move(conn));
    }
}

bool NetServer::handshake(Connection& conn) {
    // One deadline for the whole JoinRequest, not one per recv(): a peer
    // trickling a byte at a time must not hold a connection slot forever.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(settings_.handshakeTimeoutMs);

    // Collect frames until the first complete message (or timeout/violation).
    std::string joinPayload;
    while (joinPayload.empty()) {
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     deadline - std::chrono::steady_clock::now())
                                     .count();
        if (remainingMs <= 0) return false;
        setRecvTimeoutMs(conn.socket, static_cast<int>(remainingMs));
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

    // Every read below crosses the trust boundary: these bytes came off a
    // socket. readInt/readString fall back on a type clash instead of
    // throwing out of the connection thread and killing the host (#245).
    const int version = readInt(msg->payload, "version", -1);
    if (version != kNetProtocolVersion) {
        sendOn(conn, MessageType::Welcome,
               {{"accepted", false},
                {"reason", "version mismatch: server speaks " +
                               std::to_string(kNetProtocolVersion) + ", you sent " +
                               std::to_string(version) + " — update the game"}});
        return false;
    }
    if (!settings_.joinCode.empty() &&
        readString(msg->payload, "code") != settings_.joinCode) {
        sendOn(conn, MessageType::Welcome,
               {{"accepted", false}, {"reason", "wrong join code"}});
        return false;
    }
    // Missing, wrong-typed and empty names all get the "player<id>" default
    // below; anything else must pass isValidPlayerName or is refused.
    const std::string requestedName = readString(msg->payload, "name");
    if (!isValidPlayerName(requestedName)) {
        sendOn(conn, MessageType::Welcome, {{"accepted", false}, {"reason", kBadNameReason}});
        return false;
    }

    // Reserve the lowest free remote player id (1..kMaxPlayers-1); the host
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
            conn.name = requestedName.empty() ? "player" + std::to_string(assigned)
                                              : requestedName;
            conn.playerId = assigned;  // reserved, but not yet admitted
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
    // Welcome goes out BEFORE the connection is admitted: snapshots only go
    // to admitted connections, so the joiner's first frame is always its
    // Welcome rather than a WorldSnapshot it would reject.
    if (!sendOn(conn, MessageType::Welcome, {{"accepted", true}, {"playerId", assigned}})) {
        return false;
    }
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    conn.admitted = true;
    return true;
}

void NetServer::readerLoop(Connection& conn) {
    if (!handshake(conn)) {
        // Only mark dead — reapDeadConnections() owns the teardown (after
        // joining this thread). Dead connections never count as holding
        // their reserved player id, so it frees up immediately.
        conn.alive = false;
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
            if (msg.payload.is_object() && msg.payload.contains("pos")) {
                conn.pose.position = vec3FromJson(msg.payload["pos"]);
            }
            conn.pose.facingDeg =
                readFloat(msg.payload, "facing", conn.pose.facingDeg);
            break;
        }
        case MessageType::ChatOpen:
        case MessageType::ChatLine: {
            ChatEvent ev;
            ev.playerId = conn.playerId;
            ev.open = (msg.type == MessageType::ChatOpen);
            ev.npcIndex = readInt(msg.payload, "npc", -1);
            ev.text = readString(msg.payload, "text");
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
        for (const auto& npc : npcPoses_) npcs.push_back(netNpcPoseToJson(npc));
        tick = ++tick_;
    }
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto& conn : connections_) {
            if (!conn->alive || !conn->admitted) continue;
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
        reapDeadConnections();
    }
}

void NetServer::reapDeadConnections() {
    // Connections whose reader has finished (failed handshake or
    // disconnect): their player id frees up and their threads get joined.
    std::vector<std::shared_ptr<Connection>> dead;
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
    // Shut down + join here; the socket itself closes when the last owner
    // (possibly an in-flight broadcast on the host thread) drops it.
    for (auto& conn : dead) {
        if (conn->socket != kInvalidSocket) shutdownSocket(conn->socket);
        if (conn->reader.joinable()) conn->reader.join();
    }
}

}  // namespace llm_npc
