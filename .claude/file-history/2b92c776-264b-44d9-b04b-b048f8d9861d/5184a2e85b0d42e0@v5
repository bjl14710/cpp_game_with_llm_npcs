// Tests for FrameAssembler — the length-prefixed TCP framing under every
// multiplayer message (plan: .claude/plans/multiplayer-and-aws-deploy.md).
#include <string>
#include <vector>

#include "NetFraming.hpp"
#include "doctest.h"

using llm_npc::FrameAssembler;
using llm_npc::kMaxFrameBytes;

namespace {

// Feeds bytes and collects every completed frame plus the feed() verdict.
struct Collector {
    FrameAssembler assembler;
    std::vector<std::string> frames;
    bool feed(std::string_view bytes) {
        return assembler.feed(bytes,
                              [&](const std::string& frame) { frames.push_back(frame); });
    }
};

// Hand-builds a frame with an arbitrary declared length (for bad prefixes).
std::string rawPrefix(std::uint32_t declared) {
    std::string prefix;
    prefix.push_back(static_cast<char>((declared >> 24) & 0xFF));
    prefix.push_back(static_cast<char>((declared >> 16) & 0xFF));
    prefix.push_back(static_cast<char>((declared >> 8) & 0xFF));
    prefix.push_back(static_cast<char>(declared & 0xFF));
    return prefix;
}

}  // namespace

TEST_CASE("FrameAssembler encode/feed round-trips one payload") {
    Collector c;
    CHECK(c.feed(FrameAssembler::encode("hello")));
    REQUIRE(c.frames.size() == 1);
    CHECK(c.frames[0] == "hello");

    // No residue: the next frame arrives intact.
    CHECK(c.feed(FrameAssembler::encode("again")));
    REQUIRE(c.frames.size() == 2);
    CHECK(c.frames[1] == "again");
}

TEST_CASE("FrameAssembler reassembles frames split at arbitrary boundaries") {
    const std::string wire = FrameAssembler::encode("{\"type\":\"PlayerInput\"}");
    Collector c;
    // One byte at a time splits mid-prefix and mid-payload.
    for (char byte : wire) {
        CHECK(c.feed(std::string_view(&byte, 1)));
    }
    REQUIRE(c.frames.size() == 1);
    CHECK(c.frames[0] == "{\"type\":\"PlayerInput\"}");
}

TEST_CASE("FrameAssembler emits multiple frames from one feed") {
    Collector c;
    CHECK(c.feed(FrameAssembler::encode("first") + FrameAssembler::encode("second")));
    REQUIRE(c.frames.size() == 2);
    CHECK(c.frames[0] == "first");
    CHECK(c.frames[1] == "second");
}

TEST_CASE("FrameAssembler rejects an oversized length prefix") {
    Collector c;
    CHECK_FALSE(c.feed(rawPrefix(kMaxFrameBytes + 1)));
    CHECK(c.frames.empty());
}

TEST_CASE("FrameAssembler handles an empty payload frame") {
    Collector c;
    CHECK(c.feed(FrameAssembler::encode("")));
    REQUIRE(c.frames.size() == 1);
    CHECK(c.frames[0].empty());
}
