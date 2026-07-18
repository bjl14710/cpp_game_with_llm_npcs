#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "json.hpp"

namespace llm_npc {

// One parsed line of Ollama's streaming /api/chat response (NDJSON).
// Exactly one of these situations holds per line:
//   - delta carries a fragment of assistant text (possibly empty on the
//     final line), done tells us whether the stream is complete;
//   - error is non-empty when the line was an {"error": ...} payload or
//     could not be parsed at all.
struct OllamaChunk {
    std::string delta;
    bool done = false;
    std::string error;
};

// Parses a single newline-delimited JSON line from Ollama's streaming chat
// endpoint. Blank lines yield an empty chunk (no delta, not done, no error)
// so callers can simply ignore them.
inline OllamaChunk parseOllamaChunk(const std::string& line) {
    OllamaChunk chunk;

    // Tolerate \r\n framing and skip keep-alive blank lines.
    std::string trimmed = line;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ' ')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) return chunk;

    nlohmann::json parsed = nlohmann::json::parse(trimmed, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded()) {
        chunk.error = "unparseable stream line from Ollama";
        return chunk;
    }
    if (parsed.contains("error")) {
        chunk.error = parsed["error"].is_string() ? parsed["error"].get<std::string>()
                                                  : parsed["error"].dump();
        if (chunk.error.empty()) chunk.error = "unknown Ollama error";
        return chunk;
    }
    if (parsed.contains("message") && parsed["message"].contains("content") &&
        parsed["message"]["content"].is_string()) {
        chunk.delta = parsed["message"]["content"].get<std::string>();
    }
    chunk.done = parsed.value("done", false);
    return chunk;
}

// Reassembles newline-delimited lines from arbitrary byte slices, as
// delivered by an HTTP content receiver. Bytes for a line may arrive split
// across any number of feed() calls; onLine fires once per complete line
// (without the trailing '\n').
class StreamAssembler {
   public:
    // Append bytes; invokes onLine for every completed line found.
    void feed(std::string_view bytes, const std::function<void(const std::string&)>& onLine) {
        std::size_t start = 0;
        while (start < bytes.size()) {
            const std::size_t nl = bytes.find('\n', start);
            if (nl == std::string_view::npos) {
                buffer_.append(bytes.substr(start));
                return;
            }
            buffer_.append(bytes.substr(start, nl - start));
            onLine(buffer_);
            buffer_.clear();
            start = nl + 1;
        }
    }

    // Returns and clears any trailing bytes that never saw a '\n'
    // (end-of-stream flush).
    std::string takeRemainder() {
        std::string out;
        out.swap(buffer_);
        return out;
    }

   private:
    std::string buffer_;
};

}  // namespace llm_npc
