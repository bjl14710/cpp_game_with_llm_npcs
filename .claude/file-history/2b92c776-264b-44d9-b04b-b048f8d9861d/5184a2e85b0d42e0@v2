// Tests for FrameAssembler — the length-prefixed TCP framing under every
// multiplayer message. All stubs are * doctest::skip() until the feature is
// implemented (plan: .claude/plans/multiplayer-and-aws-deploy.md).
#include <string>
#include <vector>

#include "NetFraming.hpp"
#include "doctest.h"

using llm_npc::FrameAssembler;
using llm_npc::kMaxFrameBytes;

TEST_CASE("FrameAssembler encode/feed round-trips one payload" * doctest::skip()) {
    // encode("hello") fed back through feed() must fire onFrame exactly once
    // with "hello" and leave no residue in the buffer.
}

TEST_CASE("FrameAssembler reassembles frames split at arbitrary boundaries" * doctest::skip()) {
    // Feed an encoded frame one byte at a time (splits mid-prefix and
    // mid-payload); onFrame fires once with the full payload. Mirrors
    // "StreamAssembler reassembles a line split across feeds".
}

TEST_CASE("FrameAssembler emits multiple frames from one feed" * doctest::skip()) {
    // Two encoded frames concatenated into a single feed() call produce two
    // onFrame calls in order.
}

TEST_CASE("FrameAssembler rejects an oversized length prefix" * doctest::skip()) {
    // A hand-built prefix declaring kMaxFrameBytes+1 makes feed() return
    // false and never invokes onFrame — the caller drops the connection
    // (plan edge case: malformed or oversized incoming frame).
}

TEST_CASE("FrameAssembler handles an empty payload frame" * doctest::skip()) {
    // A zero-length frame is legal on the wire: onFrame fires with "".
}
