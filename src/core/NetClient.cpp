#include "NetClient.hpp"

#include <cstring>

#ifdef _WIN32
// WSAPoll and ioctlsocket come with the NetSocket.hpp includes.
#else
#include <fcntl.h>
#include <poll.h>
#endif

namespace llm_npc {

namespace {
constexpr int kConnectTimeoutMs = 4000;
constexpr int kHandshakeTimeoutMs = 5000;

// Connect with a timeout: flip the socket non-blocking, start the connect,
// poll for writability, then flip it back. A closed port on localhost still
// fails instantly (ECONNREFUSED); this guards against silent packet drops.
bool connectWithTimeout(socket_t s, const sockaddr_in& addr, int timeoutMs) {
#ifdef _WIN32
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    const int rc = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    bool ok = (rc == 0);
    if (!ok && WSAGetLastError() == WSAEWOULDBLOCK) {
        WSAPOLLFD pfd{};
        pfd.fd = s;
        pfd.events = POLLOUT;
        ok = WSAPoll(&pfd, 1, timeoutMs) == 1 && (pfd.revents & POLLOUT);
    }
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
    return ok;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
    const int rc = ::connect(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
    bool ok = (rc == 0);
    if (!ok && errno == EINPROGRESS) {
        pollfd pfd{};
        pfd.fd = s;
        pfd.events = POLLOUT;
        if (::poll(&pfd, 1, timeoutMs) == 1 && (pfd.revents & POLLOUT)) {
            int soError = 0;
            socklen_t len = sizeof(soError);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &soError, &len);
            ok = (soError == 0);
        }
    }
    fcntl(s, F_SETFL, flags);
    return ok;
#endif
}

}  // namespace

NetClient::~NetClient() { disconnect(); }

bool NetClient::connect(const std::string& host, int port,
                        const std::string& playerName, const std::string& joinCode) {
    if (connected_) disconnect();
    ensureSocketsInit();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        setError("invalid address \"" + host + "\" — use a numeric IP like 192.168.1.20");
        return false;
    }

    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == kInvalidSocket) {
        setError("could not create socket");
        return false;
    }
    configureSocket(socket_);

    if (!connectWithTimeout(socket_, addr, kConnectTimeoutMs)) {
        setError("could not reach " + host + ":" + std::to_string(port) +
                 " — is the host running and the port right?");
        closeSocket(socket_);
        socket_ = kInvalidSocket;
        return false;
    }

    // Handshake: JoinRequest up, one Welcome frame back.
    {
        std::lock_guard<std::mutex> lock(sendMutex_);
        const std::string wire = FrameAssembler::encode(
            encodeMessage(MessageType::JoinRequest, {{"version", kNetProtocolVersion},
                                                     {"name", playerName},
                                                     {"code", joinCode}}));
        if (!sendAll(socket_, wire)) {
            setError("connection dropped during handshake");
            closeSocket(socket_);
            socket_ = kInvalidSocket;
            return false;
        }
    }

    setRecvTimeoutMs(socket_, kHandshakeTimeoutMs);
    std::string welcomePayload;
    while (welcomePayload.empty()) {
        char buf[1024];
        const auto n = ::recv(socket_, buf, sizeof(buf), 0);
        if (n <= 0) {
            setError("host did not answer the handshake");
            closeSocket(socket_);
            socket_ = kInvalidSocket;
            return false;
        }
        const bool ok = framing_.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                                      [&](const std::string& frame) {
                                          if (welcomePayload.empty()) welcomePayload = frame;
                                      });
        if (!ok) {
            setError("malformed handshake from host");
            closeSocket(socket_);
            socket_ = kInvalidSocket;
            return false;
        }
    }

    auto welcome = decodeMessage(welcomePayload);
    if (!welcome || welcome->type != MessageType::Welcome) {
        setError("unexpected handshake reply from host");
        closeSocket(socket_);
        socket_ = kInvalidSocket;
        return false;
    }
    // A malicious or mismatched HOST can send these, so they get the same
    // treatment as anything a client sends (#245).
    if (!readBool(welcome->payload, "accepted", false)) {
        setError(readString(welcome->payload, "reason", "join refused"));
        closeSocket(socket_);
        socket_ = kInvalidSocket;
        return false;
    }

    playerId_ = readInt(welcome->payload, "playerId", -1);
    setRecvTimeoutMs(socket_, 0);  // reader blocks until data or shutdown
    connected_ = true;
    readerThread_ = std::thread([this] { readerLoop(); });
    return true;
}

void NetClient::disconnect() {
    if (socket_ != kInvalidSocket && connected_) {
        send(MessageType::Disconnect, {{"reason", "left"}});
    }
    connected_ = false;
    if (socket_ != kInvalidSocket) {
        shutdownSocket(socket_);  // unblocks the reader's recv
    }
    if (readerThread_.joinable()) readerThread_.join();
    if (socket_ != kInvalidSocket) {
        closeSocket(socket_);
        socket_ = kInvalidSocket;
    }
    playerId_ = -1;
}

void NetClient::sendInput(const Vec3& position, float facingDeg) {
    send(MessageType::PlayerInput, {{"pos", vec3ToJson(position)}, {"facing", facingDeg}});
}

void NetClient::sendChatOpen(int npcIndex) {
    send(MessageType::ChatOpen, {{"npc", npcIndex}});
}

void NetClient::sendChatLine(int npcIndex, const std::string& text) {
    send(MessageType::ChatLine, {{"npc", npcIndex}, {"text", text}});
}

std::vector<NetMessage> NetClient::poll() {
    std::vector<NetMessage> out;
    std::lock_guard<std::mutex> lock(inboxMutex_);
    out.swap(inbox_);
    return out;
}

std::string NetClient::lastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void NetClient::send(MessageType type, nlohmann::json payload) {
    if (!connected_ && type != MessageType::Disconnect) return;
    if (socket_ == kInvalidSocket) return;
    const std::string wire = FrameAssembler::encode(encodeMessage(type, std::move(payload)));
    std::lock_guard<std::mutex> lock(sendMutex_);
    if (!sendAll(socket_, wire)) {
        connected_ = false;
        setError("connection to host lost");
    }
}

void NetClient::readerLoop() {
    while (connected_) {
        char buf[4096];
        const auto n = ::recv(socket_, buf, sizeof(buf), 0);
        if (n <= 0) {
            if (connected_) setError("connection to host lost");
            connected_ = false;
            return;
        }
        const bool ok = framing_.feed(
            std::string_view(buf, static_cast<std::size_t>(n)),
            [&](const std::string& frame) {
                if (auto msg = decodeMessage(frame)) {
                    if (msg->type == MessageType::Disconnect) {
                        setError(readString(msg->payload, "reason", "host closed"));
                        connected_ = false;
                        return;
                    }
                    std::lock_guard<std::mutex> lock(inboxMutex_);
                    inbox_.push_back(std::move(*msg));
                }
                // Undecodable frames from a same-version server shouldn't
                // happen; skipping one is safer than killing the session.
            });
        if (!ok) {
            setError("malformed data from host");
            connected_ = false;
            return;
        }
    }
}

void NetClient::setError(std::string message) {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = std::move(message);
}

}  // namespace llm_npc
