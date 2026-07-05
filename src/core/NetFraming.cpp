#include "NetFraming.hpp"

namespace llm_npc {

namespace {

// Reads the 4-byte big-endian length prefix at the start of `bytes`.
// Caller guarantees bytes.size() >= 4.
std::uint32_t readPrefix(const std::string& bytes) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[0])) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[1])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[2])) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[3]));
}

}  // namespace

std::string FrameAssembler::encode(std::string_view payload) {
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    std::string frame;
    frame.reserve(4 + payload.size());
    frame.push_back(static_cast<char>((len >> 24) & 0xFF));
    frame.push_back(static_cast<char>((len >> 16) & 0xFF));
    frame.push_back(static_cast<char>((len >> 8) & 0xFF));
    frame.push_back(static_cast<char>(len & 0xFF));
    frame.append(payload);
    return frame;
}

bool FrameAssembler::feed(std::string_view bytes,
                          const std::function<void(const std::string&)>& onFrame) {
    buffer_.append(bytes);
    // Peel complete frames off the front. Same loop shape as
    // StreamAssembler::feed, but framed by a length prefix instead of '\n'.
    while (buffer_.size() >= 4) {
        const std::uint32_t declared = readPrefix(buffer_);
        if (declared > kMaxFrameBytes) {
            // A bad prefix means we can never find the next frame boundary
            // again; the caller must drop the connection.
            return false;
        }
        if (buffer_.size() < 4 + static_cast<std::size_t>(declared)) {
            return true;  // wait for the rest of this frame
        }
        const std::string payload = buffer_.substr(4, declared);
        buffer_.erase(0, 4 + static_cast<std::size_t>(declared));
        onFrame(payload);
    }
    return true;
}

}  // namespace llm_npc
