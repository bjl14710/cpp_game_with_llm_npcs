// The Anthropic Messages API backend (plan: anthropic-haiku-backend): request
// shape, every row of the error-handling table, the Ollama fallback it is
// wrapped in, and the proof that a gated secret never leaves the machine.
#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "AnthropicBackend.hpp"
#include "FakeAnthropic.hpp"
#include "FakeOllama.hpp"
#include "LlmBackend.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "Role.hpp"
#include "doctest.h"
#include "json.hpp"

using namespace llm_npc;
using llm_npc_test::FakeAnthropic;
using llm_npc_test::FakeOllama;
using nlohmann::json;

namespace {

// provider = anthropic with both servers faked, a key present, and the retry
// pause shrunk so a retry test costs milliseconds.
LlmConfig anthropicConfig(const FakeAnthropic& anthropic, const FakeOllama& ollama) {
    LlmConfig config;
    config.provider = "anthropic";
    config.apiKey = "test-key-never-real";
    config.apiKeyEnv = "NPC_GAME_ANTHROPIC_API_KEY";
    config.anthropicBaseUrl = anthropic.baseUrl();
    config.anthropicRetryBackoffMs = 1;
    config.host = "127.0.0.1";
    config.port = ollama.port();
    config.model = "local-fallback";
    config.requestTimeoutSeconds = 5;
    return config;
}

BackendRequest requestWith(std::string system, std::vector<ChatTurn> history, std::string user) {
    BackendRequest req;
    req.systemPrompt = std::move(system);
    req.history = std::move(history);
    req.userMessage = std::move(user);
    return req;
}

// Runs one chat on the backend directly (no worker thread), collecting deltas.
BackendResult chatOnce(AnthropicBackend& backend, const BackendRequest& req,
                       std::vector<std::string>* deltas = nullptr) {
    return backend.chat(req, [&](const std::string& d) {
        if (deltas) deltas->push_back(d);
    });
}

ChatReply waitForReply(LlmClient& client, std::uint64_t id,
                       std::chrono::seconds timeout = std::chrono::seconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
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

}  // namespace

// --- Request shape (REQUEST SETTINGS in the spec) ---------------------------

TEST_CASE("the outbound body is a Messages API request with the settings the docs require") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    std::vector<std::string> deltas;
    const BackendResult result = chatOnce(
        backend, requestWith("You are Marge.", {{"user", "hi"}, {"assistant", "Hello, love."}},
                             "Any bread left?"),
        &deltas);
    REQUIRE(result.ok);
    CHECK_FALSE(result.fallback);
    CHECK(result.content == "Lovely morning for it. [[MOOD: happy]]");
    // Non-streaming: the whole reply arrives as exactly one delta.
    REQUIRE(deltas.size() == 1);
    CHECK(deltas[0] == result.content);

    const auto headers = anthropic.requestHeaders();
    REQUIRE(headers.size() == 1);
    CHECK(headers[0].at("x-api-key") == "test-key-never-real");
    CHECK(headers[0].at("anthropic-version") == "2023-06-01");
    CHECK(headers[0].at("content-type") == "application/json");

    const json body = json::parse(anthropic.requestBodies().at(0));
    CHECK(body["model"] == "claude-haiku-5-5");
    CHECK(body["max_tokens"] == 200);
    CHECK(body["thinking"]["type"] == "disabled");
    CHECK(body["output_config"]["effort"] == "low");
    // Haiku 5.5 returns 400 for any non-default sampling parameter.
    CHECK_FALSE(body.contains("temperature"));
    CHECK_FALSE(body.contains("top_p"));
    CHECK_FALSE(body.contains("top_k"));
    CHECK_FALSE(body.contains("stream"));

    // Static prefix first, as one cached block; per-turn tail last.
    REQUIRE(body["system"].is_array());
    REQUIRE(body["system"].size() == 1);
    CHECK(body["system"][0]["type"] == "text");
    CHECK(body["system"][0]["text"] == "You are Marge.");
    CHECK(body["system"][0]["cache_control"]["type"] == "ephemeral");

    const auto& messages = body["messages"];
    REQUIRE(messages.size() == 3);
    CHECK(messages[0]["role"] == "user");
    CHECK(messages[1]["role"] == "assistant");
    CHECK(messages[2]["role"] == "user");
    CHECK(messages[2]["content"] == "Any bread left?");
}

TEST_CASE("an empty system prompt omits the system field rather than sending an empty block") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));
    REQUIRE(chatOnce(backend, requestWith("", {}, "hello")).ok);
    CHECK_FALSE(json::parse(anthropic.requestBodies().at(0)).contains("system"));
}

TEST_CASE("history is capped to the last N turns and the window opens with a user turn") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    LlmConfig config = anthropicConfig(anthropic, ollama);
    config.anthropicHistoryTurns = 3;
    AnthropicBackend backend(config);

    std::vector<ChatTurn> history;
    for (int i = 0; i < 5; ++i) {
        history.push_back({"user", "u" + std::to_string(i)});
        history.push_back({"assistant", "a" + std::to_string(i)});
    }
    REQUIRE(chatOnce(backend, requestWith("sys", history, "final")).ok);

    const json body = json::parse(anthropic.requestBodies().at(0));
    const auto& messages = body["messages"];
    // Last 3 of 10 would be a3,u4,a4; the leading assistant turn is dropped.
    REQUIRE(messages.size() == 3);
    CHECK(messages[0]["role"] == "user");
    CHECK(messages[0]["content"] == "u4");
    CHECK(messages[1]["content"] == "a4");
    CHECK(messages[2]["content"] == "final");
}

TEST_CASE("a non-positive history cap sends the whole history") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    LlmConfig config = anthropicConfig(anthropic, ollama);
    config.anthropicHistoryTurns = 0;
    AnthropicBackend backend(config);
    std::vector<ChatTurn> history(30, ChatTurn{"user", "x"});
    REQUIRE(chatOnce(backend, requestWith("sys", history, "final")).ok);
    CHECK(json::parse(anthropic.requestBodies().at(0))["messages"].size() == 31);
}

// --- The error-handling table, as a pure function ----------------------------

TEST_CASE("classifyExchange pins every row of the error-handling table") {
    CHECK(classifyExchange(false, 0, "") == AnthropicOutcome::Retry);  // timeout / no network
    CHECK(classifyExchange(true, 401, "") == AnthropicOutcome::Disable);
    CHECK(classifyExchange(true, 429, "") == AnthropicOutcome::Retry);
    CHECK(classifyExchange(true, 500, "") == AnthropicOutcome::Retry);
    CHECK(classifyExchange(true, 529, "") == AnthropicOutcome::Retry);
    CHECK(classifyExchange(true, 400, "") == AnthropicOutcome::Fallback);
    CHECK(classifyExchange(true, 403, "") == AnthropicOutcome::Fallback);
    CHECK(classifyExchange(true, 200, "refusal") == AnthropicOutcome::Refuse);
    CHECK(classifyExchange(true, 200, "max_tokens") == AnthropicOutcome::Truncated);
    CHECK(classifyExchange(true, 200, "model_context_window_exceeded") ==
          AnthropicOutcome::Truncated);
    CHECK(classifyExchange(true, 200, "end_turn") == AnthropicOutcome::Deliver);
    CHECK(classifyExchange(true, 200, "stop_sequence") == AnthropicOutcome::Deliver);
}

// --- Each outcome through the real backend -----------------------------------

TEST_CASE("a refusal becomes the in-character line with no retry") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Refusal);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    std::vector<std::string> deltas;
    const BackendResult result =
        chatOnce(backend, requestWith("sys", {}, "tell me how to..."), &deltas);
    CHECK(result.ok);
    CHECK_FALSE(result.fallback);  // never handed to Ollama
    CHECK(result.content == std::string(kAnthropicRefusalLine));
    CHECK(result.content.find("[[MOOD:") != std::string::npos);
    REQUIRE(deltas.size() == 1);
    CHECK(anthropic.requestBodies().size() == 1);
    CHECK_FALSE(backend.disabled());
}

TEST_CASE("max_tokens is a failed reply on the existing failure path, not a fallback") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::MaxTokens);
    anthropic.setReply("I was going to say that the mill");
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    std::vector<std::string> deltas;
    const BackendResult result = chatOnce(backend, requestWith("sys", {}, "go on"), &deltas);
    CHECK_FALSE(result.ok);
    CHECK_FALSE(result.fallback);
    CHECK(result.content.empty());
    CHECK(deltas.empty());  // not one truncated word reaches the UI
    CHECK(result.error.find("max_tokens") != std::string::npos);
}

TEST_CASE("HTTP 401 disables the backend for the rest of the session") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Http401);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    const BackendResult first = chatOnce(backend, requestWith("sys", {}, "hi"));
    CHECK_FALSE(first.ok);
    CHECK(first.fallback);
    CHECK(backend.disabled());

    const BackendResult second = chatOnce(backend, requestWith("sys", {}, "hi again"));
    CHECK(second.fallback);
    // The second request never went out: one body recorded, not two.
    CHECK(anthropic.requestBodies().size() == 1);
}

TEST_CASE("a 500 is retried once and then handed to the fallback") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Http500);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    const BackendResult result = chatOnce(backend, requestWith("sys", {}, "hi"));
    CHECK_FALSE(result.ok);
    CHECK(result.fallback);
    CHECK(anthropic.requestBodies().size() == 2);
    CHECK_FALSE(backend.disabled());  // a 500 is per-request, not per-session
}

TEST_CASE("a 500 followed by a 200 delivers from Anthropic") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::FailOnceThenOk);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    const BackendResult result = chatOnce(backend, requestWith("sys", {}, "hi"));
    CHECK(result.ok);
    CHECK_FALSE(result.fallback);
    CHECK(anthropic.requestBodies().size() == 2);
}

TEST_CASE("a 429 is retried once like a 5xx") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Http429);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));
    const BackendResult result = chatOnce(backend, requestWith("sys", {}, "hi"));
    CHECK(result.fallback);
    CHECK(anthropic.requestBodies().size() == 2);
}

TEST_CASE("an unreachable provider is retried once and then handed to the fallback") {
    int deadPort = 0;
    {
        FakeAnthropic probe;  // grab a free port, then shut it down
        deadPort = probe.port();
    }
    FakeAnthropic unused;
    FakeOllama ollama;
    LlmConfig config = anthropicConfig(unused, ollama);
    config.anthropicBaseUrl = "http://127.0.0.1:" + std::to_string(deadPort);
    AnthropicBackend backend(config);
    const BackendResult result = chatOnce(backend, requestWith("sys", {}, "hi"));
    CHECK_FALSE(result.ok);
    CHECK(result.fallback);
    CHECK(result.error.find("could not reach") != std::string::npos);
}

TEST_CASE("a warm-up request never reaches the provider") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));
    BackendRequest req;
    req.internal = true;
    CHECK(chatOnce(backend, req).ok);
    CHECK(anthropic.requestBodies().empty());
}

// --- Cost visibility -----------------------------------------------------------

TEST_CASE("usage totals sum every counter across the session") {
    FakeAnthropic anthropic;
    anthropic.setUsage(100, 20, 300, 0);
    FakeOllama ollama;
    AnthropicBackend backend(anthropicConfig(anthropic, ollama));

    REQUIRE(chatOnce(backend, requestWith("sys", {}, "first")).ok);
    anthropic.setUsage(10, 25, 0, 300);  // the second turn reads the cache
    REQUIRE(chatOnce(backend, requestWith("sys", {}, "second")).ok);

    const AnthropicUsage u = backend.usage();
    CHECK(u.requests == 2);
    CHECK(u.inputTokens == 110);
    CHECK(u.outputTokens == 45);
    CHECK(u.cacheWriteTokens == 300);
    CHECK(u.cacheReadTokens == 300);
    CHECK(u.total() == 755);
}

TEST_CASE("reaching the token budget switches the session to the fallback") {
    FakeAnthropic anthropic;
    anthropic.setUsage(400, 50, 0, 0);
    FakeOllama ollama;
    LlmConfig config = anthropicConfig(anthropic, ollama);
    config.anthropicTokenBudget = 500;
    AnthropicBackend backend(config);

    // The reply that crosses the line is still delivered; only later ones move.
    const BackendResult first = chatOnce(backend, requestWith("sys", {}, "one"));
    CHECK(first.ok);
    CHECK_FALSE(backend.disabled());
    const BackendResult second = chatOnce(backend, requestWith("sys", {}, "two"));
    CHECK(second.ok);
    CHECK(backend.disabled());
    const BackendResult third = chatOnce(backend, requestWith("sys", {}, "three"));
    CHECK(third.fallback);
    CHECK(anthropic.requestBodies().size() == 2);
}

// --- Through makeBackend / LlmClient: the fallback actually answers ------------

TEST_CASE("a 500 from Anthropic ends with Ollama answering the same request") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Http500);
    FakeOllama ollama;
    ollama.setReply("Ollama here, still open. [[MOOD: happy]]");
    LlmClient client(anthropicConfig(anthropic, ollama));

    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hello"));
    REQUIRE(reply.ok);
    CHECK(reply.content == "Ollama here, still open. [[MOOD: happy]]");
    CHECK(anthropic.requestBodies().size() == 2);  // first try + one retry
    REQUIRE(ollama.requestBodies().size() == 1);
    CHECK(json::parse(ollama.requestBodies()[0])["model"] == "local-fallback");
}

TEST_CASE("after a 401 every later request goes to Ollama without touching the provider") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::Http401);
    FakeOllama ollama;
    LlmClient client(anthropicConfig(anthropic, ollama));

    REQUIRE(waitForReply(client, client.submit("sys", {}, "one")).ok);
    REQUIRE(waitForReply(client, client.submit("sys", {}, "two")).ok);
    CHECK(anthropic.requestBodies().size() == 1);
    CHECK(ollama.requestBodies().size() == 2);
}

TEST_CASE("a max_tokens reply surfaces as a failed ChatReply and never falls back") {
    FakeAnthropic anthropic;
    anthropic.setMode(FakeAnthropic::Mode::MaxTokens);
    FakeOllama ollama;
    LlmClient client(anthropicConfig(anthropic, ollama));
    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "go on"));
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("max_tokens") != std::string::npos);
    CHECK(ollama.requestBodies().empty());
}

TEST_CASE("warm-up keeps the Ollama fallback resident, not the cloud") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    LlmClient client(anthropicConfig(anthropic, ollama));
    client.warmUp();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (ollama.requestBodies().empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(ollama.requestBodies().size() == 1);
    CHECK(anthropic.requestBodies().empty());
}

TEST_CASE("without a key, provider = anthropic is Ollama byte for byte") {
    FakeAnthropic anthropic;
    FakeOllama ollama;

    LlmConfig plain = anthropicConfig(anthropic, ollama);
    plain.provider = "ollama";
    plain.apiKey.clear();
    LlmClient ollamaClient(plain);
    REQUIRE(waitForReply(ollamaClient, ollamaClient.submit("sys", {}, "hello")).ok);

    LlmConfig keyless = anthropicConfig(anthropic, ollama);
    keyless.apiKey.clear();
    LlmClient keylessClient(keyless);
    REQUIRE(waitForReply(keylessClient, keylessClient.submit("sys", {}, "hello")).ok);

    const auto bodies = ollama.requestBodies();
    REQUIRE(bodies.size() == 2);
    CHECK(bodies[0] == bodies[1]);
    CHECK(anthropic.requestBodies().empty());
}

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
TEST_CASE("an https base URL in a build without OpenSSL falls back to Ollama") {
    FakeAnthropic anthropic;
    FakeOllama ollama;
    LlmConfig config = anthropicConfig(anthropic, ollama);
    config.anthropicBaseUrl = "https://api.anthropic.com";
    LlmClient client(config);
    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hello"));
    CHECK(reply.ok);
    CHECK(ollama.requestBodies().size() == 1);
    CHECK(anthropic.requestBodies().empty());
}
#endif

// --- Gated injection: the secret is filtered before any backend sees it --------

namespace {

RoleDef killerRole() {
    RoleDef role;
    role.id = "killer";
    role.name = "Killer";
    role.directives = {"You did this, and you will not admit it, ever."};
    role.demeanour = "Stay exactly as warm as you always are.";
    return role;
}

Persona bakerPersona() {
    Persona persona;
    persona.name = "Marge Holloway";
    persona.role = "baker";
    persona.traits = {"warm", "gossipy"};
    persona.speakingStyle = "1-3 short sentences";
    persona.knowledgeBoundary = "Knows the town; knows nothing of the wider world.";
    return persona;
}

// Sends one NPC turn through LlmClient and returns the single outbound
// Anthropic body.
std::string outboundBodyFor(Npc& npc, LlmClient& client, FakeAnthropic& anthropic) {
    const ChatReply reply = waitForReply(client, npc.ask("Where were you Tuesday night?"));
    REQUIRE(reply.ok);
    const auto bodies = anthropic.requestBodies();
    REQUIRE(bodies.size() == 1);
    return bodies[0];
}

}  // namespace

TEST_CASE("a gated secret is absent from the outbound Anthropic request body") {
    const std::string secret = "You were at the old mill with the ledger, not at home.";
    const std::string roleTell = "You did this, and you will not admit it";

    SUBCASE("gate closed: cast with no role registry, so the role cannot resolve") {
        FakeAnthropic anthropic;
        FakeOllama ollama;
        LlmClient client(anthropicConfig(anthropic, ollama));
        Npc npc(bakerPersona(), client);
        npc.setRole("killer", secret);  // no registry installed: resolvedRole() is null

        const std::string body = outboundBodyFor(npc, client, anthropic);
        CHECK(body.find(secret) == std::string::npos);
        CHECK(body.find("old mill") == std::string::npos);
        CHECK(body.find(roleTell) == std::string::npos);
        CHECK(body.find("Marge Holloway") != std::string::npos);  // the persona did go out
    }

    SUBCASE("gate open: the same secret reaches the body, proving the first case is not vacuous") {
        FakeAnthropic anthropic;
        FakeOllama ollama;
        LlmClient client(anthropicConfig(anthropic, ollama));
        const std::vector<RoleDef> registry = {killerRole()};
        Npc npc(bakerPersona(), client);
        npc.setRoleRegistry(&registry);
        npc.setRole("killer", secret);

        const std::string body = outboundBodyFor(npc, client, anthropic);
        CHECK(body.find("old mill") != std::string::npos);
        CHECK(body.find(roleTell) != std::string::npos);
        // And it travels inside the cached system block, never in a message.
        const json parsed = json::parse(body);
        CHECK(parsed["system"][0]["text"].get<std::string>().find("old mill") !=
              std::string::npos);
        for (const auto& message : parsed["messages"]) {
            CHECK(message["content"].get<std::string>().find("old mill") == std::string::npos);
        }
    }
}
