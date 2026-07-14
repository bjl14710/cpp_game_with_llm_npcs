#pragma once

#include <string>

#include "json.hpp"

namespace llm_npc {

// One parsed line of an OpenAI-compatible /chat/completions stream (Server-Sent
// Events). Exactly one situation holds per line:
//   - delta carries a fragment of assistant text (choices[0].delta.content);
//   - done is set by the terminal `data: [DONE]` or a non-null finish_reason;
//   - error is non-empty for an {"error": ...} payload or an unparseable line.
struct OpenAiChunk {
    std::string delta;
    bool done = false;
    std::string error;
};

// Parses a single SSE line. Event-separator blanks and SSE comment lines
// (starting ':') yield an empty chunk callers can ignore. The "data:" field
// prefix is stripped; "[DONE]" sets done.
inline OpenAiChunk parseOpenAiChunk(const std::string& line) {
    OpenAiChunk chunk;

    // Tolerate \r\n framing and trailing spaces.
    std::string trimmed = line;
    while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == ' ')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) return chunk;    // SSE event separator
    if (trimmed[0] == ':') return chunk;  // SSE comment / keep-alive

    // Strip the "data:" field name (with an optional single leading space).
    std::string payload = trimmed;
    if (payload.rfind("data:", 0) == 0) {
        payload = payload.substr(5);
        if (!payload.empty() && payload.front() == ' ') payload.erase(payload.begin());
    }
    if (payload.empty()) return chunk;
    if (payload == "[DONE]") {
        chunk.done = true;
        return chunk;
    }

    nlohmann::json parsed = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded()) {
        chunk.error = "unparseable stream line from provider";
        return chunk;
    }
    if (parsed.contains("error")) {
        const auto& e = parsed["error"];
        if (e.is_string()) {
            chunk.error = e.get<std::string>();
        } else if (e.is_object() && e.contains("message") && e["message"].is_string()) {
            chunk.error = e["message"].get<std::string>();
        } else {
            chunk.error = e.dump();
        }
        if (chunk.error.empty()) chunk.error = "unknown provider error";
        return chunk;
    }
    if (parsed.contains("choices") && parsed["choices"].is_array() && !parsed["choices"].empty()) {
        const auto& choice = parsed["choices"][0];
        if (choice.contains("delta") && choice["delta"].contains("content") &&
            choice["delta"]["content"].is_string()) {
            chunk.delta = choice["delta"]["content"].get<std::string>();
        }
        if (choice.contains("finish_reason") && !choice["finish_reason"].is_null()) {
            chunk.done = true;
        }
    }
    return chunk;
}

}  // namespace llm_npc
