// Live end-to-end tests against a real local Ollama. Skipped unless the
// environment opts in:
//
//   OLLAMA_LIVE=1 make -C tests test
//   OLLAMA_TEST_MODEL=llama3.2:1b OLLAMA_LIVE=1 make -C tests test
//
// Lands in tests/ at commit 9 alongside the fake-server suite.
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "LlmClient.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// True when the suite should hit a real Ollama instance.
bool liveEnabled() {
    const char* flag = std::getenv("OLLAMA_LIVE");
    return flag && std::string(flag) == "1";
}

// Config for the local Ollama daemon, honoring OLLAMA_TEST_MODEL.
LlmConfig liveConfig() {
    LlmConfig config;
    config.requestTimeoutSeconds = 120;  // first token on cold CPU can be slow
    if (const char* model = std::getenv("OLLAMA_TEST_MODEL")) config.model = model;
    return config;
}

}  // namespace

TEST_CASE("live Ollama streams an answer incrementally" * doctest::skip(!liveEnabled())) {
    LlmClient client(liveConfig());
    client.warmUp();

    const auto id = client.submit(
        "You are a terse assistant. Answer in one short sentence.", {},
        "Say hello to the city of Brightwater.");

    std::vector<ChatDelta> deltas;
    ChatReply reply;
    bool done = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        for (auto& d : client.drainDeltas()) deltas.push_back(std::move(d));
        for (auto& r : client.drainReplies()) {
            if (r.id == id) {
                reply = std::move(r);
                done = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(done);
    REQUIRE(reply.ok);
    CHECK_FALSE(reply.content.empty());

    // The whole point of streaming: the reply arrived in multiple pieces that
    // concatenate to the final content.
    CHECK(deltas.size() > 1);
    std::string joined;
    for (const auto& d : deltas) joined += d.text;
    CHECK(joined == reply.content);
}
