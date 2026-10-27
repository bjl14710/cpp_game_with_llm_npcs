#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "FakeOllama.hpp"
#include "LineBank.hpp"
#include "LlmBackend.hpp"  // kMaxReplyBytes
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

TEST_CASE("LlmClient sends options.num_ctx from contextTokens, default 8192") {
    // Without num_ctx Ollama runs at 4096 and truncates a longer prompt from
    // the front, dropping the system prompt first (issue #333).
    FakeOllama fake;
    {
        LlmClient client(testConfig(fake.port()));
        waitForReply(client, client.submit("sys", {}, "hi"));
    }
    LlmConfig sized = testConfig(fake.port());
    sized.contextTokens = 16384;
    {
        LlmClient client(sized);
        waitForReply(client, client.submit("sys", {}, "hi"));
    }
    const auto bodies = fake.requestBodies();
    REQUIRE(bodies.size() == 2);
    const json byDefault = json::parse(bodies[0]);
    REQUIRE(byDefault["options"].contains("num_ctx"));
    CHECK(byDefault["options"]["num_ctx"] == 8192);
    CHECK(json::parse(bodies[1])["options"]["num_ctx"] == 16384);
}

TEST_CASE("warmUp carries the same num_ctx as a chat request") {
    // Ollama reloads the model when num_ctx changes, so a warm-up sized
    // differently from real requests would load the model twice.
    FakeOllama fake;
    LlmConfig config = testConfig(fake.port());
    config.contextTokens = 12288;
    LlmClient client(config);
    client.warmUp();
    waitForReply(client, client.submit("sys", {}, "hi"));
    const auto bodies = fake.requestBodies();
    REQUIRE(bodies.size() == 2);
    CHECK(json::parse(bodies[0])["messages"].empty());  // the warm-up
    CHECK(json::parse(bodies[0])["options"]["num_ctx"] == 12288);
    CHECK(json::parse(bodies[1])["options"]["num_ctx"] == 12288);
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

// --- Line bank integration (issue #150) -------------------------------------
//
// "Served without touching the network" is proved by fake.requestBodies()
// being empty, never by timing — a timing assertion would flake.

namespace {

namespace fs = std::filesystem;

// Two variants so a single lookup is unambiguous about which one it served.
constexpr const char* kBankText =
    "persona = Marge Holloway\n"
    "\n"
    "topic = greeting_first\n"
    "  familiarity = first\n"
    "  trigger = hello\n"
    "  trigger = hi there\n"
    "  reply = Well, good morning! [[MOOD: happy]]\n"
    "  reply = Morning, love. Sourdough's just out. [[MOOD: happy]]\n";

fs::path bankDirWith(const char* stem, const char* text) {
    const fs::path dir = fs::temp_directory_path() / (std::string("llm_npc_cbank_") + stem);
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ofstream(dir / "marge.bank") << text;
    return dir;
}

std::unique_ptr<LineBank> bankAt(const fs::path& dir) {
    return std::make_unique<LineBank>(dir, 0.6f);
}

}  // namespace

TEST_CASE("a banked hit streams deltas and replies without reaching the backend") {
    FakeOllama fake;
    LlmConfig config = testConfig(fake.port());
    // Pacing is a design property, not something worth spending suite seconds
    // on; run it fast here and keep the streaming assertion on delta COUNT.
    config.lineBankCps = 4000;
    LlmClient client(config);
    client.setLineBank(bankAt(bankDirWith("hit", kBankText)));

    std::vector<ChatDelta> deltas;
    const auto id = client.submit("sys", {}, "hello", "Marge Holloway", Familiarity::First);
    const ChatReply reply = waitForReply(client, id, &deltas);

    CHECK(reply.ok);
    CHECK(reply.content.find("good morning") != std::string::npos);
    CHECK(fake.requestBodies().empty());  // never reached the network

    // A banked reply must arrive like a generated one, not in one frame.
    CHECK(deltas.size() >= 2);
    std::string assembled;
    for (const auto& d : deltas) {
        CHECK(d.id == id);
        assembled += d.text;
    }
    CHECK(assembled == reply.content);
}

TEST_CASE("a banked miss reaches the backend unchanged") {
    FakeOllama fake;
    const LlmConfig config = testConfig(fake.port());
    const std::string unmatched = "what did you think of the election";

    LlmClient withBank(config);
    withBank.setLineBank(bankAt(bankDirWith("miss", kBankText)));
    waitForReply(withBank,
                 withBank.submit("sys", {}, unmatched, "Marge Holloway", Familiarity::First));

    LlmClient without(config);
    waitForReply(without, without.submit("sys", {}, unmatched));

    // Byte-identical request bodies: a miss must leave behaviour exactly as it
    // was before the feature existed.
    const auto bodies = fake.requestBodies();
    REQUIRE(bodies.size() == 2);
    CHECK(bodies[0] == bodies[1]);
}

TEST_CASE("submitting without a speakerId never consults the bank") {
    // Group turns and world generation pass no speakerId, so they bypass the
    // bank BY CONSTRUCTION rather than by a flag someone can forget to set.
    FakeOllama fake;
    LlmClient client(testConfig(fake.port()));
    client.setLineBank(bankAt(bankDirWith("nospeaker", kBankText)));

    // "hello" would hit outright if a speaker were named.
    waitForReply(client, client.submit("sys", {}, "hello"));

    CHECK_FALSE(fake.requestBodies().empty());  // went to the backend
    REQUIRE(client.lineBank() != nullptr);
    CHECK(client.lineBank()->stats().hits == 0);
    CHECK(client.lineBank()->stats().misses == 0);  // not even looked up
}

TEST_CASE("with no line bank installed every request reaches the backend") {
    // What `line_bank = off` produces: main.cpp never constructs a bank, so no
    // bank file is read and no code path changes.
    FakeOllama fake;
    LlmClient client(testConfig(fake.port()));
    CHECK(client.lineBank() == nullptr);

    waitForReply(client, client.submit("sys", {}, "hello", "Marge Holloway",
                                       Familiarity::First));
    CHECK_FALSE(fake.requestBodies().empty());
}

// --- Deadlines, output caps and shutdown (issue #328) -----------------------
//
// Timing bounds here are deliberately generous: each asserts "about one
// deadline", against a pre-fix behaviour that was unbounded or several times
// longer, so scheduler jitter cannot flip the result.

namespace {

using Clock = std::chrono::steady_clock;

// Seconds elapsed since `start`.
double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

// Blocks until the fake has received at least `n` requests (or 10 s pass).
void waitForRequests(FakeOllama& fake, std::size_t n) {
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (fake.requestBodies().size() < n && Clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// A reply plus when it landed, in seconds after the caller's start time.
struct TimedReply {
    ChatReply reply;
    double seconds = 0.0;
};

// Like waitForReply, but for several ids at once, so a reply that lands in the
// same drain as another one is kept rather than discarded.
std::map<std::uint64_t, TimedReply> waitForReplies(LlmClient& client, std::size_t count,
                                                   Clock::time_point start) {
    std::map<std::uint64_t, TimedReply> got;
    const auto deadline = Clock::now() + std::chrono::seconds(10);
    while (got.size() < count && Clock::now() < deadline) {
        for (auto& reply : client.drainReplies()) {
            const std::uint64_t id = reply.id;
            got[id] = TimedReply{std::move(reply), secondsSince(start)};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return got;
}

// testConfig, routed through the OpenAI-compatible backend when `openAi` is
// set; the fake serves /v1/chat/completions for exactly this.
LlmConfig backendConfig(int port, bool openAi) {
    LlmConfig config = testConfig(port);
    if (openAi) {
        config.provider = "openai";
        config.baseUrl = "http://127.0.0.1:" + std::to_string(port) + "/v1";
    }
    return config;
}

}  // namespace

TEST_CASE("keep-alive bytes cannot outlast the deadline, and the next request is served") {
    // Pre-fix the read timeout reset on every keep-alive byte, so the first
    // request never ended and the second waited behind it forever.
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        fake.setMode(FakeOllama::Mode::KeepAliveForever);
        LlmConfig config = backendConfig(fake.port(), openAi);
        config.requestTimeoutSeconds = 1;
        LlmClient client(config);

        const auto start = Clock::now();
        const auto stuck = client.submit("sys", {}, "first");
        const auto next = client.submit("sys", {}, "second");
        waitForRequests(fake, 1);
        fake.setMode(FakeOllama::Mode::StreamWords);  // only the first request trickles

        auto replies = waitForReplies(client, 2, start);
        REQUIRE(replies.size() == 2);
        const TimedReply& stuckReply = replies[stuck];
        CHECK_FALSE(stuckReply.reply.ok);
        CHECK(stuckReply.reply.errorMessage.find("timed out") != std::string::npos);
        CHECK(stuckReply.seconds > 0.8);
        CHECK(stuckReply.seconds < 2.5);

        const TimedReply& nextReply = replies[next];
        CHECK_MESSAGE(nextReply.reply.ok, nextReply.reply.errorMessage);
        CHECK(nextReply.reply.content == "Hello there, traveler!");
        CHECK(nextReply.seconds < 3.0);
    }
}

TEST_CASE("a reply that is silent past the deadline reports a timeout") {
    FakeOllama fake;
    fake.setResponseDelay(std::chrono::milliseconds(1500));
    LlmConfig config = testConfig(fake.port());
    config.requestTimeoutSeconds = 1;
    LlmClient client(config);

    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"));
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("timed out") != std::string::npos);
}

TEST_CASE("an OpenAI-compatible stream with [DONE] succeeds") {
    FakeOllama fake;
    LlmClient client(backendConfig(fake.port(), /*openAi=*/true));

    std::vector<ChatDelta> deltas;
    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"), &deltas);
    REQUIRE(reply.ok);
    CHECK(reply.content == "Hello there, traveler!");
    CHECK(deltas.size() > 1);
}

TEST_CASE("an OpenAI-compatible stream cut before [DONE] fails") {
    // Pre-fix any streamed content counted as success, so a cut connection
    // could hand the game half a tag ("[[ACTION: follow") to execute.
    FakeOllama fake;
    fake.setMode(FakeOllama::Mode::TruncatedStream);
    LlmClient client(backendConfig(fake.port(), /*openAi=*/true));

    std::vector<ChatDelta> deltas;
    const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"), &deltas);
    CHECK_FALSE(deltas.empty());  // content did arrive before the cut
    CHECK_FALSE(reply.ok);
    CHECK(reply.errorMessage.find("ended before completion") != std::string::npos);
}

TEST_CASE("request bodies cap output at max_reply_tokens") {
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        LlmConfig config = backendConfig(fake.port(), openAi);
        config.maxReplyTokens = 77;
        LlmClient client(config);
        waitForReply(client, client.submit("sys", {}, "hi"));

        const auto bodies = fake.requestBodies();
        REQUIRE(bodies.size() == 1);
        const json body = json::parse(bodies[0]);
        if (openAi) {
            CHECK(body.value("max_tokens", 0) == 77);
        } else {
            CHECK(body.value("options", json::object()).value("num_predict", 0) == 77);
        }
    }
}

TEST_CASE("max_reply_tokens of 0 omits the output cap") {
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        LlmConfig config = backendConfig(fake.port(), openAi);
        config.maxReplyTokens = 0;
        LlmClient client(config);
        waitForReply(client, client.submit("sys", {}, "hi"));

        const auto bodies = fake.requestBodies();
        REQUIRE(bodies.size() == 1);
        const json body = json::parse(bodies[0]);
        CHECK_FALSE(body.contains("max_tokens"));
        CHECK_FALSE(body.value("options", json::object()).contains("num_predict"));
    }
}

TEST_CASE("a reply longer than kMaxReplyBytes fails instead of growing") {
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        fake.setReply(std::string(kMaxReplyBytes + 1, 'x'));  // one oversized delta
        LlmClient client(backendConfig(fake.port(), openAi));

        const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"));
        CHECK_FALSE(reply.ok);
        CHECK(reply.errorMessage.find("reply exceeded") != std::string::npos);
    }
}

TEST_CASE("an endless response line is cut off at kMaxStreamBytes") {
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        fake.setMode(FakeOllama::Mode::Flood);
        LlmClient client(backendConfig(fake.port(), openAi));

        const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"));
        CHECK_FALSE(reply.ok);
        CHECK(reply.errorMessage.find("response exceeded") != std::string::npos);
    }
}

TEST_CASE("an endless error body is cut off at kMaxReplyBytes") {
    for (const bool openAi : {false, true}) {
        CAPTURE(openAi);
        FakeOllama fake;
        fake.setMode(FakeOllama::Mode::FloodError);
        LlmClient client(backendConfig(fake.port(), openAi));

        const ChatReply reply = waitForReply(client, client.submit("sys", {}, "hi"));
        CHECK_FALSE(reply.ok);
        CHECK(reply.errorMessage.find("HTTP 500") != std::string::npos);
        CHECK(reply.errorMessage.find("oversized error body") != std::string::npos);
    }
}

TEST_CASE("destroying LlmClient drops queued requests instead of serving them") {
    // Pre-fix the worker drained the whole queue over the network before the
    // destructor's join() returned: 4 x 1.5 s = 6 s to quit.
    FakeOllama fake;
    fake.setResponseDelay(std::chrono::milliseconds(1500));
    auto client = std::make_unique<LlmClient>(testConfig(fake.port()));
    for (int i = 0; i < 4; ++i) client->submit("sys", {}, "hi " + std::to_string(i));
    waitForRequests(fake, 1);  // the first is now in flight

    const auto start = Clock::now();
    client.reset();
    CHECK(secondsSince(start) < 2.5);
    CHECK(fake.requestBodies().size() == 1);  // the queued three never went out
}
