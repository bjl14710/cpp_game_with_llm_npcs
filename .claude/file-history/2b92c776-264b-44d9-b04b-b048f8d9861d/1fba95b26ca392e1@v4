#pragma once

// Minimal cross-platform TCP socket shims shared by NetServer and NetClient.
// Same #ifdef split as external/httplib.h (winsock vs POSIX) so the game
// keeps building on Windows without new dependencies. Internal helper header
// — everything is small enough to stay inline.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
namespace llm_npc {
using socket_t = SOCKET;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
}  // namespace llm_npc
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
namespace llm_npc {
using socket_t = int;
inline constexpr socket_t kInvalidSocket = -1;
}  // namespace llm_npc
#endif

#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>

namespace llm_npc {

// One-time winsock startup (no-op on POSIX). Safe to call from any thread.
inline void ensureSocketsInit() {
#ifdef _WIN32
    static std::once_flag once;
    std::call_once(once, [] {
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
    });
#endif
}

inline void closeSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

// Unblocks any thread stuck in recv()/accept() on this socket — the reliable
// cross-platform way to interrupt a blocking reader during shutdown.
inline void shutdownSocket(socket_t s) {
#ifdef _WIN32
    shutdown(s, SD_BOTH);
#else
    shutdown(s, SHUT_RDWR);
#endif
}

// Sets a receive timeout so a stalled peer cannot hang a reader forever
// (used for handshakes; 0 restores blocking reads).
inline void setRecvTimeoutMs(socket_t s, int ms) {
#ifdef _WIN32
    DWORD tv = static_cast<DWORD>(ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

// Keeps a write to a dead peer from killing the process with SIGPIPE:
// Linux flags it per send, macOS flags it per socket, Windows never signals.
#if defined(MSG_NOSIGNAL)
inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
inline constexpr int kSendFlags = 0;
#endif

// Per-socket setup right after socket()/accept(). Currently only the macOS
// SIGPIPE suppression; both server and client sockets go through this.
inline void configureSocket(socket_t s) {
    (void)s;
#if defined(SO_NOSIGPIPE)
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
}

// Writes the whole buffer, looping over partial sends. False on any error —
// the caller treats the connection as dead.
inline bool sendAll(socket_t s, std::string_view bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const auto n = ::send(s, bytes.data() + sent, bytes.size() - sent, kSendFlags);
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

}  // namespace llm_npc
