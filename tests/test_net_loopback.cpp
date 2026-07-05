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

#include "City.hpp"
#include "FakeOllama.hpp"
#include "HostChatRouter.hpp"
#include "LlmClient.hpp"
#include "NetClient.hpp"
#include "NetServer.hpp"
#include "Npc.hpp"
#include "NpcAction.hpp"
#include "Persona.hpp"
#include "World.hpp"
#include "doctest.h"

using namespace std::chrono_literals;
using llm_npc::kMaxPlayers;
using llm_npc::kNetProtocolVersion;
using llm_npc::MessageType;
using llm_npc::NetClient;
using llm_npc::NetMessage;
using llm_npc::NetServer;
using llm_npc::NetNpcPose;
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
    NetNpcPose npc;
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

namespace {

// Loopback plus a real World + LlmClient (against FakeOllama) + the router —
// the same wiring main.cpp uses when hosting, minus rendering.
struct ChatLoopback : Loopback {
    llm_npc_test::FakeOllama fake;
    llm_npc::LlmClient llm;
    llm_npc::World world;
    llm_npc::HostChatRouter router;

    // `fake` is declared before `llm`, so its port is ready when the
    // LlmConfig is built (member initialization order matters here).
    ChatLoopback()
        : llm(llm_npc::LlmConfig{"127.0.0.1", fake.port(), "test-model", 0.0, 5, "1m"}),
          world(llm_npc::City{}),
          router(world, server) {}

    // One frame of the host loop: chat events in, LLM traffic out.
    void hostFrame() {
        router.update();
        for (auto& d : llm.drainDeltas()) router.routeDelta(d);
        for (auto& r : llm.drainReplies()) router.routeReply(r);
    }

    // Messages of `type` that client i has received for NPC `npc`.
    std::vector<const NetMessage*> chatMessages(std::size_t i, MessageType type,
                                                int npc = 0) const {
        std::vector<const NetMessage*> out;
        for (const auto& msg : received[i]) {
            if (msg.type == type && msg.payload.value("npc", -1) == npc) {
                out.push_back(&msg);
            }
        }
        return out;
    }
};

}  // namespace

TEST_CASE("shared NPC chat updates mood for the bystander too") {
    ChatLoopback net;
    net.fake.setReply("Grr, watch it! [MOOD: angry]");

    llm_npc::Persona grump;
    grump.name = "Grump";
    net.world.addNpc(llm_npc::Npc(grump, net.llm));

    REQUIRE(net.join("asker"));
    REQUIRE(net.join("bystander"));

    net.clients[0]->sendChatOpen(0);
    net.clients[0]->sendChatLine(0, "hello there");

    // Asker streams deltas and gets the final directive-stripped reply.
    CHECK(eventually(
        [&] {
            const auto replies = net.chatMessages(0, MessageType::ChatReply);
            return !replies.empty() && replies[0]->payload.value("ok", false) &&
                   replies[0]->payload.value("text", std::string{}) == "Grr, watch it!";
        },
        [&] {
            net.hostFrame();
            net.pump();
        }));
    CHECK_FALSE(net.chatMessages(0, MessageType::ChatDelta).empty());

    // The bystander — who never opened a chat — sees the bubble and the mood.
    CHECK(eventually(
        [&] {
            const auto bubbles = net.chatMessages(1, MessageType::NpcSpeechBubble);
            const auto moods = net.chatMessages(1, MessageType::NpcMoodUpdate);
            return !bubbles.empty() &&
                   bubbles[0]->payload.value("text", std::string{}) == "Grr, watch it!" &&
                   !moods.empty() &&
                   moods.back()->payload.value("mood", 0) ==
                       static_cast<int>(llm_npc::NpcMood::Angry);
        },
        [&] {
            net.hostFrame();
            net.pump();
        }));

    // And the host's world agrees — one shared NPC, one shared mood.
    CHECK(net.world.npcs()[0].mood() == llm_npc::NpcMood::Angry);
    CHECK(net.world.npcs()[0].history().size() == 2);
}

TEST_CASE("LLM failure is reported only to the requesting client") {
    ChatLoopback net;
    net.fake.setMode(llm_npc_test::FakeOllama::Mode::Http500);

    llm_npc::Persona p;
    p.name = "Clerk";
    net.world.addNpc(llm_npc::Npc(p, net.llm));

    REQUIRE(net.join("asker"));
    REQUIRE(net.join("bystander"));

    net.clients[0]->sendChatLine(0, "hello?");

    CHECK(eventually(
        [&] {
            const auto replies = net.chatMessages(0, MessageType::ChatReply);
            return !replies.empty() && !replies[0]->payload.value("ok", true) &&
                   replies[0]->payload.value("error", std::string{}).find("500") !=
                       std::string::npos;
        },
        [&] {
            net.hostFrame();
            net.pump();
        }));

    // The bystander saw nothing chat-related and is still connected.
    CHECK(net.chatMessages(1, MessageType::ChatReply).empty());
    CHECK(net.chatMessages(1, MessageType::ChatDelta).empty());
    CHECK(net.chatMessages(1, MessageType::NpcSpeechBubble).empty());
    CHECK(net.chatMessages(1, MessageType::NpcMoodUpdate).empty());
    CHECK(net.clients[1]->connected());
}

TEST_CASE("two clients messaging the same NPC both get answered in order") {
    ChatLoopback net;
    net.fake.setReply("One at a time, please.");

    llm_npc::Persona p;
    p.name = "Barkeep";
    net.world.addNpc(llm_npc::Npc(p, net.llm));

    REQUIRE(net.join("first"));
    REQUIRE(net.join("second"));

    net.clients[0]->sendChatLine(0, "line from first");
    // Ensure first's line is submitted (NPC busy) before second's arrives,
    // so the queue order is deterministic.
    CHECK(eventually([&] {
        net.hostFrame();
        return net.world.npcs()[0].waiting();
    }));
    net.clients[1]->sendChatLine(0, "line from second");

    // Both get exactly their own reply, serialized through the one NPC.
    CHECK(eventually(
        [&] {
            return net.chatMessages(0, MessageType::ChatReply).size() == 1 &&
                   net.chatMessages(1, MessageType::ChatReply).size() == 1;
        },
        [&] {
            net.hostFrame();
            net.pump();
        }));
    CHECK(net.chatMessages(0, MessageType::ChatReply)[0]->payload.value("ok", false));
    CHECK(net.chatMessages(1, MessageType::ChatReply)[0]->payload.value("ok", false));

    // The shared history holds both exchanges in submission order.
    const auto& history = net.world.npcs()[0].history();
    REQUIRE(history.size() == 4);
    CHECK(history[0].content == "line from first");
    CHECK(history[2].content == "line from second");
}

TEST_CASE("client disconnect mid-conversation leaves the server healthy") {
    ChatLoopback net;
    net.fake.setReply("Anyone there?");

    llm_npc::Persona p;
    p.name = "Greeter";
    net.world.addNpc(llm_npc::Npc(p, net.llm));

    REQUIRE(net.join("quitter"));
    REQUIRE(net.join("stayer"));

    // Quitter asks, then leaves before the host processes anything.
    net.clients[0]->sendChatLine(0, "hello and goodbye");
    CHECK(eventually([&] {
        net.hostFrame();
        return net.world.npcs()[0].waiting();
    }));
    net.clients[0]->disconnect();

    // The orphaned reply resolves without harm and the NPC frees up.
    CHECK(eventually([&] {
        net.hostFrame();
        return !net.world.npcs()[0].waiting();
    }));

    // The stayer can immediately use the same NPC.
    net.clients[1]->sendChatLine(0, "still here");
    CHECK(eventually(
        [&] { return net.chatMessages(1, MessageType::ChatReply).size() == 1; },
        [&] {
            net.hostFrame();
            net.pump();
        }));
    CHECK(net.chatMessages(1, MessageType::ChatReply)[0]->payload.value("ok", false));
}
