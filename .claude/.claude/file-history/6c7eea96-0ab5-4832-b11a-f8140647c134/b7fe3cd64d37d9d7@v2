#include <chrono>
#include <optional>
#include <string>
#include <thread>

#include "FakeOllama.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "doctest.h"

using namespace llm_npc;
using llm_npc_test::FakeOllama;

namespace {

// A minimal persona for history tests.
Persona testPersona() {
    Persona p;
    p.name = "Gus";
    p.role = "hot-dog vendor";
    return p;
}

// Runs one full ask -> reply -> onReplyArrived round trip, returning the
// assistant text the NPC accepted (empty on failure or timeout).
std::string exchange(Npc& npc, LlmClient& client, const std::string& line) {
    const auto id = npc.ask(line);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        for (const auto& reply : client.drainReplies()) {
            if (reply.id != id) continue;
            const std::optional<std::string> text = npc.onReplyArrived(reply);
            return text.value_or("");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return {};
}

}  // namespace

TEST_CASE("Npc appends user and assistant turns on success") {
    FakeOllama fake;
    fake.setReply("One dog, mustard only!");
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);

    const std::string text = exchange(npc, client, "One hot dog please");
    CHECK(text == "One dog, mustard only!");
    REQUIRE(npc.history().size() == 2);
    CHECK(npc.history()[0].role == "user");
    CHECK(npc.history()[0].content == "One hot dog please");
    CHECK(npc.history()[1].role == "assistant");
    CHECK(npc.history()[1].content == "One dog, mustard only!");
    CHECK_FALSE(npc.waiting());
}

TEST_CASE("Npc keeps history out of failed exchanges") {
    FakeOllama fake;
    fake.setMode(FakeOllama::Mode::Http500);
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);

    const std::string text = exchange(npc, client, "hello?");
    CHECK(text.empty());
    CHECK(npc.history().empty());
    CHECK_FALSE(npc.waiting());
}

TEST_CASE("Npc trims history to the configured turn budget") {
    FakeOllama fake;
    fake.setReply("Sure thing!");
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client, /*maxHistoryTurns=*/2);

    exchange(npc, client, "first");
    exchange(npc, client, "second");
    exchange(npc, client, "third");

    // 2 turns = at most 4 entries, and the oldest exchange fell off.
    REQUIRE(npc.history().size() == 4);
    CHECK(npc.history()[2].role == "user");
    CHECK(npc.history()[2].content == "third");
}

TEST_CASE("Npc ignores replies meant for other requests") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);

    npc.ask("hello");
    ChatReply stray;
    stray.id = 9999;
    stray.ok = true;
    stray.content = "wrong number";
    CHECK_FALSE(npc.onReplyArrived(stray).has_value());
    CHECK(npc.history().empty());
    CHECK(npc.waiting());
}
