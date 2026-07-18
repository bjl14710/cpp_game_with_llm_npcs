// Tests for StreamAssembler and parseOllamaChunk — the NDJSON plumbing that
// turns Ollama's streamed /api/chat bytes into per-token text deltas.
#include <string>
#include <vector>

#include "StreamAssembler.hpp"
#include "doctest.h"

using llm_npc::OllamaChunk;
using llm_npc::parseOllamaChunk;
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
