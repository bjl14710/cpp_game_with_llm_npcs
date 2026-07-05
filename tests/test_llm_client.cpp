#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "FakeOllama.hpp"
#include "LlmClient.hpp"
#include "doctest.h"
#include "json.hpp"

using namespace llm_npc;
using llm_npc_test::FakeOllama;
using nlohmann::json;

namespace {

// Drains the client every few milliseconds until a reply with `id` arrives or
// the deadline passes. Deltas drained along the way are appended to *deltas.
ChatReply waitForReply(LlmClient& client, std::uint64_t id,
                       std::vector<ChatDelta>* deltas = nullptr,
                       std::chrono::seconds timeout = std::chrono::seconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (deltas) {
            for (auto& d : client.drainDeltas()) deltas->push_back(std::move(d));
        }
        for (auto& reply : client.drainReplies()) {
            if (reply.id == id) return reply;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    ChatReply timedOut;
    timedOut.id = id;
    timedOut.errorMessage = "test timed out waiting for reply";
    return timedOut;
}

// LlmConfig pointed at the fake server with snappy timeouts.
LlmConfig testConfig(int port) {
    LlmConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.model = "test-model";
    config.requestTimeoutSeconds = 5;
    return config;
}

}  // namespace

TEST_CASE("LlmClient sends a well-formed streaming chat request") {
    FakeOllama fake;
    LlmClient client(testConfig(fake.port()));

    std::vector<ChatTurn> history = {{"user", "hi"}, {"assistant", "hello"}};
    const auto id = client.submit("You are Gus.", history, "One hot dog please");
    const ChatReply reply = waitForReply(client, id);
    REQUIRE(reply.ok);

    const auto bodies = fake.requestBodies();
    REQUIRE(bodies.size() == 1);
    const json body = json::parse(bodies[0]);
    CHECK(body["model"] == "test-model");
    CHECK(body["stream"] == true);
    CHECK(body["keep_alive"] == "10m");
    CHECK(body["options"]["temperature"].is_number());

    const auto& messages = body["messages"];
    REQUIRE(messages.size() == 4);
    CHECK(messages[0]["role"] == "system");
    CHECK(messages[0]["content"] == "You are Gus.");
    CHECK(messages[1]["role"] == "user");
    CHECK(messages[2]["role"] == "assistant");
    CHECK(messages[3]["role"] == "user");
    CHECK(messages[3]["content"] == "One hot dog please");
}

TEST_CASE("LlmClient reassembles a streamed reply and surfaces deltas") {
    FakeOllama fake;
    fake.setReply("Welcome to the plaza, friend of the city!");
    LlmClient client(testConfig(fake.port()));

    std::vector<ChatDelta> deltas;
    const auto id = client.submit("system", {}, "hello");
    const ChatReply reply = waitForReply(client, id, &deltas);

    REQUIRE(reply.ok);
    CHECK(reply.content == "Welcome to the plaza, friend of the city!");

    // Streaming means more than one fragment, all tagged with the request id,
    // concatenating to exactly the final content.
    CHECK(deltas.size() > 1);
    std::string joined;
    for (const auto& d : deltas) {
        CHECK(d.id == id);
        joined += d.text;
    }
    CHECK(joined == reply.content);
}

TEST_CASE("LlmClient reports HTTP errors") {
    FakeOllama fake;
    fake.setMode(FakeOllama::Mode::Http500);
    LlmClient client(testConfig(fake.port()));

    const auto id = client.submit("system", {}, "hello");
    const ChatReply reply = waitForReply(client, id);
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("500") != std::string::npos);
}

TEST_CASE("LlmClient reports an error line inside the stream") {
    FakeOllama fake;
    fake.setMode(FakeOllama::Mode::ErrorLine);
    LlmClient client(testConfig(fake.port()));

    const auto id = client.submit("system", {}, "hello");
    const ChatReply reply = waitForReply(client, id);
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("model not found") != std::string::npos);
}

TEST_CASE("LlmClient reports a stream that ends without done") {
    FakeOllama fake;
    fake.setMode(FakeOllama::Mode::TruncatedStream);
    LlmClient client(testConfig(fake.port()));

    const auto id = client.submit("system", {}, "hello");
    const ChatReply reply = waitForReply(client, id);
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("ended before completion") != std::string::npos);
}

TEST_CASE("LlmClient reports an unreachable server") {
    int deadPort = 0;
    {
        FakeOllama fake;  // grab a free port, then shut the server down
        deadPort = fake.port();
    }
    LlmClient client(testConfig(deadPort));

    const auto id = client.submit("system", {}, "hello");
    const ChatReply reply = waitForReply(client, id);
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("could not reach") != std::string::npos);
}

TEST_CASE("warmUp loads the model without producing replies or deltas") {
    FakeOllama fake;
    LlmClient client(testConfig(fake.port()));

    client.warmUp();

    // Wait until the fake saw the warm-up request.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (fake.requestBodies().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto bodies = fake.requestBodies();
    REQUIRE(bodies.size() == 1);
    const json body = json::parse(bodies[0]);
    CHECK(body["messages"].empty());
    CHECK(body["keep_alive"] == "10m");

    // Give any (incorrect) reply time to land, then confirm silence.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(client.drainReplies().empty());
    CHECK(client.drainDeltas().empty());
}

TEST_CASE("LlmClient destruction never loses the shutdown wakeup") {
    // Regression: the destructor used to set stop_ and notify without holding
    // the queue mutex, so the notify could land between the worker evaluating
    // its wait predicate (false) and blocking — a lost wakeup that left
    // join() hanging forever. Tight construct/destroy cycles hit exactly that
    // window; pre-fix this test flakily hangs the suite.
    for (int i = 0; i < 200; ++i) {
        LlmClient client(testConfig(1));  // port unused: no requests submitted
    }
    CHECK(true);  // reaching here means every worker joined
}

TEST_CASE("LlmClient sends think:false only when configured") {
    FakeOllama fake;
    LlmConfig config = testConfig(fake.port());
    config.think = "false";
    LlmClient client(config);
    const auto id = client.submit("sys", {}, "hi");
    waitForReply(client, id);
    const auto bodies = fake.requestBodies();
    REQUIRE(!bodies.empty());
    const json body = json::parse(bodies.back());
    REQUIRE(body.contains("think"));
    CHECK(body["think"] == false);
}

TEST_CASE("LlmClient omits think entirely by default") {
    FakeOllama fake;
    LlmClient client(testConfig(fake.port()));
    const auto id = client.submit("sys", {}, "hi");
    waitForReply(client, id);
    const auto bodies = fake.requestBodies();
    REQUIRE(!bodies.empty());
    CHECK_FALSE(json::parse(bodies.back()).contains("think"));
}
