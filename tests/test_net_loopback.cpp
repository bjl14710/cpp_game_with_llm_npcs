// Loopback integration tests: NetServer + NetClient over 127.0.0.1 with the
// LLM served by FakeOllama (tests/FakeOllama.hpp), so no real Ollama or
// network is needed (plan: .claude/plans/multiplayer-and-aws-deploy.md).
//
// NPC-chat routing cases arrive with the chat-routing issue; they stay
// * doctest::skip() below until then.
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "NetClient.hpp"
#include "NetServer.hpp"
#include "doctest.h"

using namespace std::chrono_literals;
using llm_npc::kMaxPlayers;
using llm_npc::kNetProtocolVersion;
using llm_npc::MessageType;
using llm_npc::NetClient;
using llm_npc::NetMessage;
using llm_npc::NetServer;
using llm_npc::NpcPose;
using llm_npc::Vec3;

namespace {

// Deadline-polls `predicate`, pumping `pump` between checks (no fixed sleeps
// — the suite stays fast when things are ready and honest when they hang).
bool eventually(const std::function<bool()>& predicate,
                const std::function<void()>& pump = {},
                std::chrono::milliseconds deadline = 3000ms) {
    const auto end = std::chrono::steady_clock::now() + deadline;
    while (std::chrono::steady_clock::now() < end) {
        if (pump) pump();
        if (predicate()) return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

// A host on an ephemeral port plus N joined clients, each remembering the
// messages it drained. The chat-routing tests grow this with a World +
// FakeOllama-backed LlmClient.
struct Loopback {
    NetServer server;
    std::vector<std::unique_ptr<NetClient>> clients;
    std::vector<std::vector<NetMessage>> received;  // per client, all drained

    explicit Loopback(NetServer::Settings settings = {}) : server(std::move(settings)) {
        REQUIRE(server.start());
        REQUIRE(server.port() > 0);
    }

    // Connects one more client; true on success (index = clients.size()-1).
    bool join(const std::string& name, const std::string& code = "") {
        auto client = std::make_unique<NetClient>();
        const bool ok = client->connect("127.0.0.1", server.port(), name, code);
        clients.push_back(std::move(client));
        received.emplace_back();
        return ok;
    }

    // Drains every client's poll() into `received`.
    void pump() {
        for (std::size_t i = 0; i < clients.size(); ++i) {
            for (auto& msg : clients[i]->poll()) received[i].push_back(std::move(msg));
        }
    }

    // Latest WorldSnapshot client i has seen; nullptr when none arrived yet.
    const NetMessage* latestSnapshot(std::size_t i) const {
        for (auto it = received[i].rbegin(); it != received[i].rend(); ++it) {
            if (it->type == MessageType::WorldSnapshot) return &*it;
        }
        return nullptr;
    }
};

// Finds player `id` in a snapshot payload; nullptr when absent.
const nlohmann::json* findPlayer(const NetMessage& snapshot, int id) {
    for (const auto& p : snapshot.payload["players"]) {
        if (p.value("id", -1) == id) return &p;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("client joins and both sides see two players in the snapshot") {
    Loopback net;
    REQUIRE(net.join("guest"));
    CHECK(net.clients[0]->playerId() == 1);

    CHECK(eventually([&] { return net.server.playerCount() == 1; }));

    // The next snapshot lists the host (0) and the guest (1).
    CHECK(eventually(
        [&] {
            const NetMessage* snap = net.latestSnapshot(0);
            return snap && findPlayer(*snap, 0) && findPlayer(*snap, 1);
        },
        [&] { net.pump(); }));
}

TEST_CASE("player movement propagates through server snapshots") {
    Loopback net;
    REQUIRE(net.join("guest"));

    net.clients[0]->sendInput(Vec3{5.f, 0.f, -7.f}, 123.f);

    CHECK(eventually(
        [&] {
            const NetMessage* snap = net.latestSnapshot(0);
            if (!snap) return false;
            const nlohmann::json* me = findPlayer(*snap, 1);
            return me && (*me)["pos"].value("x", 0.f) == doctest::Approx(5.f) &&
                   (*me)["pos"].value("z", 0.f) == doctest::Approx(-7.f) &&
                   (*me).value("facing", 0.f) == doctest::Approx(123.f);
        },
        [&] { net.pump(); }));

    // The host's own pose and published NPCs ride the same snapshot.
    net.server.setHostPose(Vec3{1.f, 0.f, 2.f}, 45.f);
    NpcPose npc;
    npc.npcIndex = 3;
    npc.position = {9.f, 0.f, 9.f};
    npc.mood = 2;
    net.server.publishNpcPoses({npc});

    CHECK(eventually(
        [&] {
            const NetMessage* snap = net.latestSnapshot(0);
            if (!snap) return false;
            const nlohmann::json* host = findPlayer(*snap, 0);
            if (!host || (*host)["pos"].value("x", 0.f) != doctest::Approx(1.f)) return false;
            const auto& npcs = snap->payload["npcs"];
            return npcs.size() == 1 && npcs[0].value("i", -1) == 3 &&
                   npcs[0].value("mood", 0) == 2;
        },
        [&] { net.pump(); }));
}

TEST_CASE("connect to an unreachable address fails cleanly") {
    // Grab a port that is definitely closed: bind a server, note the port,
    // stop it, then aim a client at the corpse.
    int deadPort = 0;
    {
        NetServer probe({});
        REQUIRE(probe.start());
        deadPort = probe.port();
        probe.stop();
    }

    NetClient client;
    CHECK_FALSE(client.connect("127.0.0.1", deadPort, "guest", ""));
    CHECK_FALSE(client.connected());
    CHECK_FALSE(client.lastError().empty());

    // Bogus hostnames fail with a message instead of hanging (numeric-only v1).
    CHECK_FALSE(client.connect("not-an-ip", 1234, "guest", ""));
    CHECK_FALSE(client.lastError().empty());
}

TEST_CASE("a session-full join attempt is rejected") {
    Loopback net;
    // kMaxPlayers includes the host, so kMaxPlayers-1 remote slots exist.
    for (int i = 1; i < kMaxPlayers; ++i) {
        REQUIRE(net.join("guest" + std::to_string(i)));
    }
    CHECK(eventually([&] { return net.server.playerCount() == kMaxPlayers - 1; }));

    CHECK_FALSE(net.join("latecomer"));
    CHECK(net.clients.back()->lastError() == "session full");

    // A slot frees up when someone leaves; the next join succeeds.
    net.clients[0]->disconnect();
    CHECK(eventually([&] { return net.server.playerCount() == kMaxPlayers - 2; }));
    CHECK(net.join("replacement"));
}

TEST_CASE("protocol version mismatch is rejected at the handshake") {
    // Speak the handshake by hand with a deliberately wrong version, using a
    // second server's client machinery would hide the version constant — so
    // drive a raw NetClient against a hand-rolled JoinRequest via NetServer's
    // normal path: the only lever is the version field, which NetClient
    // always sends correctly. Simplest honest check: a server refuses a
    // JoinRequest whose version differs, which we exercise through a raw
    // socket.
    Loopback net;

    llm_npc::socket_t raw = ::socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE(raw != llm_npc::kInvalidSocket);
    llm_npc::configureSocket(raw);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(net.server.port()));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    REQUIRE(::connect(raw, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);

    const std::string join = llm_npc::FrameAssembler::encode(llm_npc::encodeMessage(
        MessageType::JoinRequest,
        {{"version", kNetProtocolVersion + 1}, {"name", "time traveler"}, {"code", ""}}));
    REQUIRE(llm_npc::sendAll(raw, join));

    // Read the refusal Welcome.
    llm_npc::FrameAssembler framing;
    std::string reply;
    llm_npc::setRecvTimeoutMs(raw, 3000);
    while (reply.empty()) {
        char buf[512];
        const auto n = ::recv(raw, buf, sizeof(buf), 0);
        REQUIRE(n > 0);
        framing.feed(std::string_view(buf, static_cast<std::size_t>(n)),
                     [&](const std::string& frame) {
                         if (reply.empty()) reply = frame;
                     });
    }
    llm_npc::closeSocket(raw);

    auto welcome = llm_npc::decodeMessage(reply);
    REQUIRE(welcome.has_value());
    CHECK(welcome->type == MessageType::Welcome);
    CHECK_FALSE(welcome->payload.value("accepted", true));
    CHECK(welcome->payload.value("reason", std::string{}).find("version mismatch") !=
          std::string::npos);
    CHECK(eventually([&] { return net.server.playerCount() == 0; }));
}

TEST_CASE("shared NPC chat updates mood for the bystander too" * doctest::skip()) {
    // Chat-routing issue: client A chats with NPC 0 (FakeOllama streams a
    // reply with a [[MOOD:...]] directive); A receives ChatDelta/ChatReply,
    // and client B — who never opened the chat — receives NpcMoodUpdate/
    // NpcSpeechBubble for NPC 0.
}

TEST_CASE("LLM failure is reported only to the requesting client" * doctest::skip()) {
    // Chat-routing issue: FakeOllama in Http500 mode — client A's ChatReply
    // carries the error text, client B receives nothing chat-related and
    // stays connected.
}

TEST_CASE("two clients messaging the same NPC both get answered in order" * doctest::skip()) {
    // Chat-routing issue: concurrent ChatLines to NPC 0 serialize through
    // the single LlmClient worker; each client gets exactly its own reply,
    // no interleaving or crash.
}

TEST_CASE("client disconnect mid-conversation leaves the server healthy" * doctest::skip()) {
    // Chat-routing issue: A disconnects while its reply streams; the server
    // drops A's in-flight routing, B can immediately chat with the same NPC.
}
