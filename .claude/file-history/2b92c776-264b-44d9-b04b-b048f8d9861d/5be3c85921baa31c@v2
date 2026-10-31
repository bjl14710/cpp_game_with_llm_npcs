#include "NetFraming.hpp"

namespace llm_npc {

std::string FrameAssembler::encode(std::string_view payload) {
    // TODO(implement): 4-byte big-endian length prefix + payload bytes.
    // Reject/assert payload.size() >= kMaxFrameBytes.
    (void)payload;
    return {};
}

bool FrameAssembler::feed(std::string_view bytes,
                          const std::function<void(const std::string&)>& onFrame) {
    // TODO(implement): append to buffer_; while buffer_ holds a full prefix,
    // read the big-endian length, bail out (return false) when it exceeds
    // kMaxFrameBytes, otherwise wait for the full payload and fire onFrame
    // with the prefix stripped. Mirror StreamAssembler::feed's loop shape.
    (void)bytes;
    (void)onFrame;
    return true;
}

}  // namespace llm_npc
