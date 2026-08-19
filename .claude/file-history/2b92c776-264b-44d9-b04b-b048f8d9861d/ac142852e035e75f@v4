// Tests for StreamAssembler, parseOllamaChunk (Ollama NDJSON), and
// parseOpenAiChunk (OpenAI-compatible SSE) — the plumbing that turns streamed
// chat bytes into per-token text deltas.
#include <string>
#include <vector>

#include "OpenAiChunk.hpp"
#include "StreamAssembler.hpp"
#include "doctest.h"

using llm_npc::OllamaChunk;
using llm_npc::OpenAiChunk;
using llm_npc::parseOllamaChunk;
using llm_npc::parseOpenAiChunk;
using llm_npc::StreamAssembler;

namespace {

// Feeds bytes and collects every completed line.
std::vector<std::string> collectLines(StreamAssembler& a, std::string_view bytes) {
    std::vector<std::string> lines;
    a.feed(bytes, [&](const std::string& line) { lines.push_back(line); });
    return lines;
}

}  // namespace

TEST_CASE("StreamAssembler emits complete lines from one feed") {
    StreamAssembler a;
    auto lines = collectLines(a, "first\nsecond\n");
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "first");
    CHECK(lines[1] == "second");
    CHECK(a.takeRemainder().empty());
}

TEST_CASE("StreamAssembler reassembles a line split across feeds") {
    StreamAssembler a;
    CHECK(collectLines(a, "{\"mess").empty());
    CHECK(collectLines(a, "age\": 1").empty());
    auto lines = collectLines(a, "}\n");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "{\"message\": 1}");
}

TEST_CASE("StreamAssembler keeps trailing partial line as remainder") {
    StreamAssembler a;
    auto lines = collectLines(a, "done\npartial");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0] == "done");
    CHECK(a.takeRemainder() == "partial");
    CHECK(a.takeRemainder().empty());  // remainder is consumed
}

TEST_CASE("parseOllamaChunk extracts a text delta") {
    OllamaChunk c = parseOllamaChunk(
        R"({"model":"m","message":{"role":"assistant","content":"Hel"},"done":false})");
    CHECK(c.delta == "Hel");
    CHECK_FALSE(c.done);
    CHECK(c.error.empty());
}

TEST_CASE("parseOllamaChunk reports the final done line") {
    OllamaChunk c = parseOllamaChunk(
        R"({"model":"m","message":{"role":"assistant","content":""},"done":true})");
    CHECK(c.delta.empty());
    CHECK(c.done);
    CHECK(c.error.empty());
}

TEST_CASE("parseOllamaChunk surfaces server error payloads") {
    OllamaChunk c = parseOllamaChunk(R"({"error":"model not found"})");
    CHECK(c.error == "model not found");
    CHECK_FALSE(c.done);
}

TEST_CASE("parseOllamaChunk flags unparseable lines") {
    OllamaChunk c = parseOllamaChunk("not json at all");
    CHECK_FALSE(c.error.empty());
}

TEST_CASE("parseOllamaChunk ignores blank and CRLF-framed lines") {
    OllamaChunk blank = parseOllamaChunk("");
    CHECK(blank.delta.empty());
    CHECK(blank.error.empty());
    CHECK_FALSE(blank.done);

    OllamaChunk crlf = parseOllamaChunk(
        "{\"message\":{\"content\":\"hi\"},\"done\":false}\r");
    CHECK(crlf.delta == "hi");
    CHECK(crlf.error.empty());
}

TEST_CASE("parseOpenAiChunk extracts a text delta from a data line") {
    OpenAiChunk c = parseOpenAiChunk(
        R"(data: {"choices":[{"delta":{"content":"Hel"}}]})");
    CHECK(c.delta == "Hel");
    CHECK_FALSE(c.done);
    CHECK(c.error.empty());
}

TEST_CASE("parseOpenAiChunk treats [DONE] as completion") {
    OpenAiChunk c = parseOpenAiChunk("data: [DONE]");
    CHECK(c.delta.empty());
    CHECK(c.done);
    CHECK(c.error.empty());
}

TEST_CASE("parseOpenAiChunk marks done on a non-null finish_reason") {
    OpenAiChunk c = parseOpenAiChunk(
        R"(data: {"choices":[{"delta":{},"finish_reason":"stop"}]})");
    CHECK(c.delta.empty());
    CHECK(c.done);
    CHECK(c.error.empty());
}

TEST_CASE("parseOpenAiChunk surfaces an error object's message") {
    OpenAiChunk c = parseOpenAiChunk(
        R"(data: {"error":{"message":"invalid api key","type":"auth"}})");
    CHECK(c.error == "invalid api key");
    CHECK_FALSE(c.done);
}

TEST_CASE("parseOpenAiChunk ignores blank lines and SSE comments") {
    OpenAiChunk blank = parseOpenAiChunk("");
    CHECK(blank.delta.empty());
    CHECK(blank.error.empty());
    CHECK_FALSE(blank.done);

    OpenAiChunk comment = parseOpenAiChunk(": OPENROUTER PROCESSING");
    CHECK(comment.delta.empty());
    CHECK(comment.error.empty());
    CHECK_FALSE(comment.done);

    // A bare keep-alive "data:" with nothing after it is a no-op too.
    OpenAiChunk emptyData = parseOpenAiChunk("data:");
    CHECK(emptyData.delta.empty());
    CHECK_FALSE(emptyData.done);
}

TEST_CASE("parseOpenAiChunk tolerates CRLF framing") {
    OpenAiChunk c = parseOpenAiChunk(
        "data: {\"choices\":[{\"delta\":{\"content\":\"hi\"}}]}\r");
    CHECK(c.delta == "hi");
    CHECK(c.error.empty());
}
