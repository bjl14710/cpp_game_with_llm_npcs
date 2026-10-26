// Hostile-peer and lifecycle tests for NetServer/NetClient over 127.0.0.1
// (issue #329). Each case drives a REAL server through raw sockets the way a
// misbehaving or malicious peer would, then proves an ordinary client can
// still join — the #245 decoder tests never reached a live server.
//
// Timing knobs (snapshot rate, handshake/send timeouts) are turned up or down
// per test so every case finishes in well under three seconds while its
// deadlines stay generous.
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/time.h>
#endif

#include "NetClient.hpp"
#include "NetFraming.hpp"
#include "NetServer.hpp"
#include "doctest.h"

using namespace std::chrono_literals;
using llm_npc::kNetProtocolVersion;
using llm_npc::MessageType;
using llm_npc::NetClient;
using llm_npc::NetMessage;
using llm_npc::NetNpcPose;
using llm_npc::NetServer;
using llm_npc::socket_t;

namespace {

// Deadline-polls `predicate` (same shape as test_net_loopback.cpp's helper).
bool eventually(const std::function<bool()>& predicate,
                std::chrono::milliseconds deadline = 3000ms) {
    const auto end = std::chrono::steady_clock::now() + deadline;
    while (std::chrono::steady_clock::now() < end) {
        if (predicate()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return predicate();
}

// A hand-driven TCP peer: connects, sends whatever frames a test wants and
// reads replies, with no NetClient logic in between.
struct RawPeer {
    socket_t sock = llm_npc::kInvalidSocket;
    llm_npc::FrameAssembler framing;
    std::vector<std::string> frames;  // received, not yet taken
    bool peerClosed = false;

    // `rcvbuf` > 0 shrinks the receive buffer before connecting, so a peer
    // that stops reading backs the server up quickly.
    explicit RawPeer(int port, int rcvbuf = 0) {
        sock = ::socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE(sock != llm_npc::kInvalidSocket);
        llm_npc::configureSocket(sock);
        if (rcvbuf > 0) {
            setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
                       sizeof(rcvbuf));
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(port));
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        REQUIRE(::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    }
    ~RawPeer() { close(); }
    RawPeer(const RawPeer&) = delete;
    RawPeer& operator=(const RawPeer&) = delete;

    void close() {
        if (sock != llm_npc::kInvalidSocket) llm_npc::closeSocket(sock);
        sock = llm_npc::kInvalidSocket;
    }

    // Closes with an RST instead of a FIN (SO_LINGER 0): no TIME_WAIT is left
    // behind on either side, so hundreds of these in a row stay fast.
    void abort() {
        linger hard{};
        hard.l_onoff = 1;
        hard.l_linger = 0;
        setsockopt(sock, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&hard),
                   sizeof(hard));
        close();
    }

    bool sendBytes(const std::string& bytes) { return llm_npc::sendAll(sock, bytes); }

    bool sendMessage(MessageType type, nlohmann::json payload) {
        return sendBytes(llm_npc::FrameAssembler::encode(
            llm_npc::encodeMessage(type, std::move(payload))));
    }

    // Sends an arbitrary payload in a well-formed frame (e.g. not-JSON).
    bool sendFrame(const std::string& payload) {
        return sendBytes(llm_npc::FrameAssembler::encode(payload));
    }

    // One recv() of at most `timeoutMs`; records frames and an orderly close.
    void pumpOnce(int timeoutMs) {
        llm_npc::setRecvTimeoutMs(sock, timeoutMs);
        char buf[4096];
        const auto n = ::recv(sock, buf, sizeof(buf), 0);
        if (n == 0) {
            peerClosed = true;
            return;
        }
        if (n < 0) return;  // timeout (or reset; the callers only care about frames/EOF)
        framing.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                     [&](const std::string& f) { frames.push_back(f); });
    }

    // Next decoded message, waiting up to `timeoutMs`; nullopt on timeout/close.
    std::optional<NetMessage> nextMessage(int timeoutMs = 2000) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (frames.empty() && !peerClosed && std::chrono::steady_clock::now() < end) {
            pumpOnce(50);
        }
        if (frames.empty()) return std::nullopt;
        auto msg = llm_npc::decodeMessage(frames.front());
        frames.erase(frames.begin());
        return msg;
    }

    // True once the server has closed its end (EOF seen within `timeoutMs`).
    bool closedWithin(int timeoutMs) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!peerClosed && std::chrono::steady_clock::now() < end) {
            frames.clear();
            pumpOnce(20);
        }
        return peerClosed;
    }

    // Sends a correct JoinRequest and returns the Welcome reply.
    std::optional<NetMessage> join(const nlohmann::json& name = "raw") {
        nlohmann::json req = {{"version", kNetProtocolVersion}, {"code", ""}};
        req["name"] = name;
        if (!sendMessage(MessageType::JoinRequest, req)) return std::nullopt;
        return nextMessage();
    }
};

// A host on an ephemeral port with test-tuned timing.
NetServer::Settings fastSettings(float snapshotHz = 100.f) {
    NetServer::Settings s;
    s.snapshotHz = snapshotHz;
    return s;
}

// Joins a fresh NetClient, retrying while the server reaps the debris of a
// previous step (a connection over the cap is closed, which reads as a failed
// join). Returns the connected client, or nullptr if no join succeeded.
std::unique_ptr<NetClient> joinEventually(const NetServer& server, const std::string& name) {
    std::unique_ptr<NetClient> client;
    eventually([&] {
        client = std::make_unique<NetClient>();
        return client->connect("127.0.0.1", server.port(), name, "");
    });
    return client && client->connected() ? std::move(client) : nullptr;
}

// Counts WorldSnapshots in a drained batch.
int snapshotsIn(const std::vector<NetMessage>& batch) {
    int n = 0;
    for (const auto& msg : batch) n += (msg.type == MessageType::WorldSnapshot) ? 1 : 0;
    return n;
}

}  // namespace

TEST_CASE("a connect-then-close storm never frees a connection under its reader") {
    // Bug 1: the Connection was published before its reader thread existed,
    // so a peer that hung up at once could be reaped (and freed) while the
    // accept thread was still assigning the thread into it. A 5 kHz tick reaps
    // as often as possible to make that window easy to hit (and TSan sees it).
    NetServer server(fastSettings(5000.f));
    REQUIRE(server.start());

    for (int i = 0; i < 300; ++i) {
        RawPeer peer(server.port());
        peer.abort();  // the handshake fails at once on the reset
        // Keeps the burst under the accept rate (a thread start is several
        // ms under TSan): a full listen queue only costs a 1 s SYN
        // retransmit, which would slow the test, not break the server.
        std::this_thread::sleep_for(1ms);
    }

    auto client = joinEventually(server, "survivor");
    REQUIRE(client);
    CHECK(eventually([&] { return server.playerCount() == 1; }));
}

TEST_CASE("a client that stops reading is dropped without freezing the host") {
    // Bug 2: broadcast() did a blocking send() while holding the connections
    // lock. A peer that never reads fills its buffers, and then the tick
    // thread — and playerCount(), remotePlayerPoses(), stop() — hung forever.
    auto settings = fastSettings(50.f);
    settings.sendTimeoutMs = 200;
    NetServer server(settings);
    REQUIRE(server.start());

    RawPeer stalled(server.port(), /*rcvbuf=*/4096);
    const auto welcome = stalled.join("stalled");
    REQUIRE(welcome.has_value());
    REQUIRE(welcome->type == MessageType::Welcome);
    REQUIRE(llm_npc::readBool(welcome->payload, "accepted", false));

    NetClient healthy;
    REQUIRE(healthy.connect("127.0.0.1", server.port(), "healthy", ""));
    CHECK(eventually([&] { return server.playerCount() == 2; }));

    // From here `stalled` never reads. A stand-in for the host loop pushes
    // 128 KiB broadcasts so the kernel buffers (a few MiB on loopback) fill
    // in well under a second; its sends to `stalled` then block until the
    // send timeout.
    std::atomic<bool> pushing{true};
    std::thread host([&] {
        const nlohmann::json bulk = {{"npc", 0}, {"text", std::string(128 * 1024, 'z')}};
        while (pushing) {
            server.broadcast(MessageType::NpcSpeechBubble, bulk);
            std::this_thread::sleep_for(10ms);
        }
    });

    // Every host-side query must keep answering promptly meanwhile.
    auto slowest = 0ms;
    const bool dropped = eventually(
        [&] {
            healthy.poll();  // keep the healthy client's inbox from growing
            const auto t0 = std::chrono::steady_clock::now();
            const int count = server.playerCount();
            server.remotePlayerPoses();
            const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0);
            if (took > slowest) slowest = took;
            return count == 1;
        },
        2500ms);
    pushing = false;
    host.join();

    CHECK(dropped);
    CHECK(slowest < 150ms);  // the send timeout is 200 ms; a lock held across it shows here
    CHECK(healthy.connected());
    CHECK(stalled.closedWithin(1000));  // reaped, not just flagged
}

TEST_CASE("an oversized or unprintable name is refused and others stay connected") {
    // Bug 3: names up to 1 MiB were admitted, and two of them pushed every
    // WorldSnapshot past kMaxFrameBytes, disconnecting the whole session.
    NetServer server(fastSettings());
    REQUIRE(server.start());

    NetClient guest;
    REQUIRE(guest.connect("127.0.0.1", server.port(), "guest", ""));

    for (const std::string& bad :
         {std::string(600 * 1024, 'n'), std::string(llm_npc::kMaxPlayerNameChars + 1, 'n'),
          std::string("new\nline")}) {
        CAPTURE(bad.size());
        NetClient hostile;
        CHECK_FALSE(hostile.connect("127.0.0.1", server.port(), bad, ""));
        CHECK(hostile.lastError().find("name must be at most 32 printable ASCII") !=
              std::string::npos);
    }

    // At the cap is fine.
    NetClient longest;
    CHECK(longest.connect("127.0.0.1", server.port(),
                          std::string(llm_npc::kMaxPlayerNameChars, 'L'), ""));

    // The bystander kept receiving snapshots throughout.
    guest.poll();
    int fresh = 0;
    CHECK(eventually([&] {
        fresh += snapshotsIn(guest.poll());
        return fresh >= 3;
    }));
    CHECK(guest.connected());
    CHECK(server.playerCount() == 2);
}

TEST_CASE("connections beyond the cap are closed and idle joiners time out") {
    // Bug 4: every accept spawned a thread with no cap, and a joiner that
    // never spoke held its slot for 5 s PER recv() — forever if it trickled.
    auto settings = fastSettings();
    settings.handshakeTimeoutMs = 800;
    NetServer server(settings);
    REQUIRE(server.start());

    const int extra = 4;
    std::vector<std::unique_ptr<RawPeer>> idle;
    for (int i = 0; i < NetServer::kMaxConnections + extra; ++i) {
        idle.push_back(std::make_unique<RawPeer>(server.port()));
    }

    // Phase 1, well inside the 800 ms deadline: only the extras are closed.
    // (Checking the 8 open ones costs ~20 ms each, so this ends by ~300 ms.)
    int closedEarly = 0;
    std::this_thread::sleep_for(100ms);
    for (auto& peer : idle) closedEarly += peer->closedWithin(10) ? 1 : 0;
    CHECK(closedEarly == extra);

    // Phase 2: the rest hit the TOTAL handshake deadline and are closed too.
    int closedLate = 0;
    for (auto& peer : idle) closedLate += peer->closedWithin(1500) ? 1 : 0;
    CHECK(closedLate == NetServer::kMaxConnections + extra);

    // The slots are free again for a real player.
    auto client = joinEventually(server, "patient");
    CHECK(client != nullptr);
}

TEST_CASE("a joiner trickling bytes is dropped at the total handshake deadline") {
    // A per-recv() timeout never fires for a peer that sends one byte now
    // and then; the deadline must cover the whole handshake.
    auto settings = fastSettings();
    settings.handshakeTimeoutMs = 300;
    NetServer server(settings);
    REQUIRE(server.start());

    RawPeer slow(server.port());
    const std::string join = llm_npc::FrameAssembler::encode(llm_npc::encodeMessage(
        MessageType::JoinRequest, {{"version", kNetProtocolVersion}, {"name", "slow"}}));
    const auto start = std::chrono::steady_clock::now();
    bool closed = false;
    for (std::size_t i = 0; i < join.size() - 1 && !closed; ++i) {
        slow.sendBytes(join.substr(i, 1));
        closed = slow.closedWithin(50);
    }
    CHECK(closed);
    CHECK(std::chrono::steady_clock::now() - start < 1500ms);
    CHECK(server.playerCount() == 0);
}

#ifndef _WIN32
TEST_CASE("accept errors back off instead of spinning a core") {
    // Bug 4, the spin: when the process is out of descriptors, accept()
    // fails with EMFILE immediately and keeps failing while the connection
    // waits in the backlog. Recreate that by lowering RLIMIT_NOFILE so the
    // client's socket is the last descriptor available, then measure how much
    // CPU the process burns while the server cannot accept. POSIX-only: the
    // limit and getrusage have no winsock equivalent.
    NetServer server(fastSettings(12.f));
    REQUIRE(server.start());

    rlimit original{};
    REQUIRE(getrlimit(RLIMIT_NOFILE, &original) == 0);
    struct RestoreLimit {
        rlimit saved;
        ~RestoreLimit() { setrlimit(RLIMIT_NOFILE, &saved); }
    } restore{original};

    // New descriptors take the lowest free number, so every number below
    // this one is in use; a limit of probe+1 leaves `probe` the last slot.
    const socket_t probe = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(probe != llm_npc::kInvalidSocket);
    rlimit tight = original;
    tight.rlim_cur = static_cast<rlim_t>(probe) + 1;
    REQUIRE(setrlimit(RLIMIT_NOFILE, &tight) == 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(server.port()));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    // Completes in the kernel backlog; the server's accept() now hits EMFILE.
    REQUIRE(::connect(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    const auto cpuMs = [] {
        rusage u{};
        getrusage(RUSAGE_SELF, &u);
        return (u.ru_utime.tv_sec + u.ru_stime.tv_sec) * 1000.0 +
               (u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1000.0;
    };
    const double before = cpuMs();
    std::this_thread::sleep_for(500ms);
    const double burned = cpuMs() - before;
    CAPTURE(burned);
    // A spinning accept loop burns ~500 ms of CPU here; backing off 50 ms per
    // failure burns a few.
    CHECK(burned < 150.0);

    // Descriptors back: the parked connection is accepted and served.
    setrlimit(RLIMIT_NOFILE, &original);
    llm_npc::closeSocket(probe);
    auto client = joinEventually(server, "after-emfile");
    CHECK(client != nullptr);
}
#endif

TEST_CASE("many joins in a row never see a snapshot before their Welcome") {
    // Bug 5: the player id was published (snapshot-eligible) before Welcome
    // was sent, so the tick thread could slip a WorldSnapshot in first and
    // the joiner failed with "unexpected handshake reply from host". A 2 kHz
    // tick keeps snapshots flying through every join.
    NetServer server(fastSettings(2000.f));
    REQUIRE(server.start());
    server.publishNpcPoses(std::vector<NetNpcPose>(20));

    int failures = 0;
    std::string lastError;
    for (int i = 0; i < 200; ++i) {
        NetClient client;
        if (!client.connect("127.0.0.1", server.port(), "joiner", "")) {
            ++failures;
            lastError = client.lastError();
        }
        client.disconnect();
        // Wait for the slot to free so "session full" can't masquerade as
        // the race.
        REQUIRE(eventually([&] { return server.playerCount() == 0; }));
    }
    CAPTURE(lastError);
    CHECK(failures == 0);
}

TEST_CASE("a reused NetClient reconnects cleanly after the host dropped it") {
    // Bug 8: connect() only cleaned up when connected_ was still true, but
    // the reader clears it when the host drops — so the next connect()
    // move-assigned onto a joinable thread (std::terminate), leaked the old
    // socket, and kept half a frame of the old stream in front of the next
    // Welcome.
    //
    // A fake host welcomes us, sends HALF of another frame, and hangs up.
    socket_t listener = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(listener != llm_npc::kInvalidSocket);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    REQUIRE(::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    REQUIRE(::listen(listener, 1) == 0);
    socklen_t len = sizeof(addr);
    REQUIRE(::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    const int fakePort = ntohs(addr.sin_port);

    std::thread fakeHost([listener] {
        const socket_t s = ::accept(listener, nullptr, nullptr);
        if (s == llm_npc::kInvalidSocket) return;
        llm_npc::configureSocket(s);
        llm_npc::FrameAssembler framing;
        bool gotJoin = false;
        llm_npc::setRecvTimeoutMs(s, 2000);
        while (!gotJoin) {
            char buf[1024];
            const auto n = ::recv(s, buf, sizeof(buf), 0);
            if (n <= 0) break;
            framing.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                         [&](const std::string&) { gotJoin = true; });
        }
        const std::string next = llm_npc::FrameAssembler::encode(
            llm_npc::encodeMessage(MessageType::WorldSnapshot, {{"tick", 1}}));
        llm_npc::sendAll(s, llm_npc::FrameAssembler::encode(llm_npc::encodeMessage(
                                MessageType::Welcome, {{"accepted", true}, {"playerId", 1}})) +
                                next.substr(0, next.size() / 2));
        llm_npc::closeSocket(s);
    });

    NetClient client;
    CHECK(client.connect("127.0.0.1", fakePort, "guest", ""));
    fakeHost.join();
    llm_npc::closeSocket(listener);
    CHECK(eventually([&] { return !client.connected(); }));

    // Same object, a real host this time.
    NetServer server(fastSettings());
    REQUIRE(server.start());
    REQUIRE(client.connect("127.0.0.1", server.port(), "guest", ""));
    CHECK(client.playerId() == 1);
    CHECK(eventually([&] { return snapshotsIn(client.poll()) > 0; }));

    // And again after an orderly leave.
    client.disconnect();
    CHECK(eventually([&] { return server.playerCount() == 0; }));
    REQUIRE(client.connect("127.0.0.1", server.port(), "guest", ""));
    CHECK(eventually([&] { return server.playerCount() == 1; }));
}

TEST_CASE("hostile frames over a raw socket leave the server accepting joins") {
    // Bug 10: the #245 hostile-payload cases only exercised the decoders.
    // These go over a real socket into a live server's reader threads.
    NetServer server(fastSettings());
    REQUIRE(server.start());

    // A joined peer that then sends wrong-typed fields of every flavour.
    RawPeer rogue(server.port());
    const auto welcome = rogue.join("rogue");
    REQUIRE(welcome.has_value());
    REQUIRE(llm_npc::readBool(welcome->payload, "accepted", false));
    CHECK(rogue.sendMessage(MessageType::PlayerInput, {{"facing", "north"}}));
    CHECK(rogue.sendMessage(MessageType::PlayerInput, {{"pos", "somewhere"}, {"facing", nullptr}}));
    CHECK(rogue.sendMessage(MessageType::PlayerInput, {{"pos", {{"x", "a"}, {"z", {1, 2}}}}}));
    CHECK(rogue.sendMessage(MessageType::ChatOpen, {{"npc", 1.5}}));
    CHECK(rogue.sendMessage(MessageType::ChatLine, {{"npc", "x"}, {"text", 5}}));
    CHECK(rogue.sendMessage(MessageType::VoteNominate, {{"name", nlohmann::json::array()}}));
    CHECK(rogue.sendMessage(MessageType::WorldSnapshot, {{"players", "all of them"}}));

    // The wrong-typed chat fields were absorbed into defaults, not thrown.
    std::vector<NetServer::ChatEvent> events;
    CHECK(eventually([&] {
        for (auto& ev : server.drainChatEvents()) events.push_back(std::move(ev));
        return events.size() == 2;
    }));
    for (const auto& ev : events) {
        CHECK(ev.npcIndex == -1);
        CHECK(ev.text.empty());
    }

    // A handshake with every field wrong-typed is refused, not fatal.
    RawPeer garbled(server.port());
    CHECK(garbled.sendMessage(MessageType::JoinRequest,
                              {{"version", "two"}, {"name", 17}, {"code", {1}}}));
    const auto refusal = garbled.nextMessage();
    REQUIRE(refusal.has_value());
    CHECK_FALSE(llm_npc::readBool(refusal->payload, "accepted", true));

    // A wrong-typed name alone falls back to the default name.
    RawPeer nameless(server.port());
    const auto named = nameless.join(17);
    REQUIRE(named.has_value());
    CHECK(llm_npc::readBool(named->payload, "accepted", false));

    // Not-JSON, and a length prefix far past the cap: both just drop that peer.
    RawPeer junk(server.port());
    CHECK(junk.sendFrame("this is not json"));
    CHECK(junk.closedWithin(2000));
    RawPeer huge(server.port());
    CHECK(huge.sendBytes(std::string("\x7F\xFF\xFF\xFF", 4)));
    CHECK(huge.closedWithin(2000));
    CHECK(rogue.sendFrame("{\"type\":"));  // the rogue's own stream turns to garbage
    CHECK(rogue.closedWithin(2000));

    // Through all of it, the server still admits an ordinary player.
    NetClient player;
    REQUIRE(player.connect("127.0.0.1", server.port(), "player", ""));
    CHECK(eventually([&] { return snapshotsIn(player.poll()) > 0; }));
    CHECK(eventually([&] { return server.playerCount() == 2; }));  // nameless + player
}
