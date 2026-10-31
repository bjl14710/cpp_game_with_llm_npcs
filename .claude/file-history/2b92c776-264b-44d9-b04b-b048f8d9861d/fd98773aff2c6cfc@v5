#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace llm_npc {

// Frames larger than this are treated as protocol violations and the
// connection is dropped — a corrupt or hostile length prefix must never make
// us allocate gigabytes. Snapshots for 4 players + ~43 NPCs are a few KB.
inline constexpr std::uint32_t kMaxFrameBytes = 1 << 20;  // 1 MiB

// Length-prefixed message framing over a TCP byte stream: each frame is a
// 4-byte big-endian payload length followed by that many bytes of JSON.
// This is the TCP analogue of StreamAssembler's newline framing for HTTP —
// pure buffer logic with no sockets, so it is fully unit-testable
// (tests/test_net_framing.cpp).
class FrameAssembler {
   public:
    // Wraps a payload in the 4-byte length prefix, ready to send().
    // TODO(implement): big-endian encode; callers check size < kMaxFrameBytes.
    static std::string encode(std::string_view payload);

    // Appends received bytes; invokes onFrame once per completed payload
    // (prefix stripped). Bytes may arrive split at any boundary, including
    // mid-prefix. Returns false — and stops consuming — when a declared
    // length exceeds kMaxFrameBytes, signalling the caller to drop the
    // connection; the stream cannot be resynchronized after a bad prefix.
    // TODO(implement): accumulate, peel frames in a loop.
    bool feed(std::string_view bytes, const std::function<void(const std::string&)>& onFrame);

   private:
    std::string buffer_;
};

}  // namespace llm_npc
