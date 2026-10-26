#include "OllamaBackend.hpp"

#include <algorithm>
#include <chrono>
#include <string_view>
#include <utility>

#include "StreamAssembler.hpp"
#include "httplib.h"
#include "json.hpp"

namespace llm_npc {

using nlohmann::json;

OllamaBackend::OllamaBackend(LlmConfig config) : config_(std::move(config)) {}

std::string OllamaBackend::model() const {
    std::lock_guard<std::mutex> lock(modelMutex_);
    return config_.model;
}

void OllamaBackend::setModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(modelMutex_);
    config_.model = model;
}

std::vector<std::string> OllamaBackend::listModels() {
    httplib::Client cli(config_.host, config_.port);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(3, 0);

    std::vector<std::string> names;
    const httplib::Result res = cli.Get("/api/tags");
    if (!res || res->status != 200) return names;
    const json parsed = json::parse(res->body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.contains("models")) return names;
    for (const auto& m : parsed["models"]) {
        if (m.contains("name") && m["name"].is_string()) {
            names.push_back(m["name"].get<std::string>());
        }
    }
    return names;
}

BackendResult OllamaBackend::chat(const BackendRequest& req, const DeltaCallback& onDelta) {
    BackendResult reply;

    std::string model;
    {
        std::lock_guard<std::mutex> lock(modelMutex_);
        model = config_.model;
    }

    // Build Ollama /api/chat body. Streaming NDJSON keeps perceived latency low
    // (first words appear immediately); keep_alive holds the model in memory
    // between conversations. Warm-up requests send no messages, which makes
    // Ollama load the model and return at once.
    json messages = json::array();
    if (!req.internal) {
        if (!req.systemPrompt.empty()) {
            messages.push_back({{"role", "system"}, {"content", req.systemPrompt}});
        }
        for (const auto& turn : req.history) {
            messages.push_back({{"role", turn.role}, {"content", turn.content}});
        }
        messages.push_back({{"role", "user"}, {"content", req.userMessage}});
    }

    json body = {
        {"model", model},
        {"messages", messages},
        {"stream", true},
        {"keep_alive", config_.keepAlive},
        {"options", {{"temperature", config_.temperature}}},
    };
    // Only reasoning models understand `think`; omit unless configured so
    // ordinary models never see an unknown field. qwen3-class models need
    // "false" for chat-speed replies (24s -> 2.7s TTFT, bench/REPORT.md).
    // Re-added when this body moved out of LlmClient — dev grew `think`
    // AFTER the pluggable branch diverged (the port's one audit finding).
    if (!config_.think.empty()) body["think"] = (config_.think == "true");
    if (config_.maxReplyTokens > 0) body["options"]["num_predict"] = config_.maxReplyTokens;

    // The read timeout only bounds silence between bytes; the deadline bounds
    // the whole request, so a server trickling keep-alive bytes cannot hold
    // the single worker thread (and every other NPC) indefinitely.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(config_.requestTimeoutSeconds);
    httplib::Client cli(config_.host, config_.port);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_write_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(5, 0);

    int status = 0;
    std::string errorBody;    // body bytes of a non-200 response
    std::string content;      // accumulated assistant text
    std::string streamError;  // first error found inside the stream
    std::string abortReason;  // why the receiver cut the request short
    std::size_t received = 0;
    bool sawDone = false;
    StreamAssembler assembler;

    // Parses one NDJSON line: append text, surface deltas, note completion.
    const auto handleLine = [&](const std::string& line) {
        if (!abortReason.empty()) return;
        const OllamaChunk chunk = parseOllamaChunk(line);
        if (!chunk.error.empty()) {
            if (streamError.empty()) streamError = chunk.error;
            return;
        }
        if (!chunk.delta.empty()) {
            if (content.size() + chunk.delta.size() > kMaxReplyBytes) {
                abortReason = "Ollama reply exceeded " + std::to_string(kMaxReplyBytes) + " bytes";
                return;
            }
            content += chunk.delta;
            onDelta(chunk.delta);
        }
        if (chunk.done) sawDone = true;
    };
    const std::string timeoutError =
        "Ollama request timed out after " + std::to_string(config_.requestTimeoutSeconds) + " s";

    // httplib v0.15 has no streaming Post() overload, so drive a raw Request
    // through Client::send with a content receiver.
    httplib::Request hreq;
    hreq.method = "POST";
    hreq.path = "/api/chat";
    hreq.set_header("Content-Type", "application/json");
    hreq.body = body.dump();
    hreq.response_handler = [&](const httplib::Response& response) {
        status = response.status;
        if (std::chrono::steady_clock::now() >= deadline) abortReason = timeoutError;
        return abortReason.empty();
    };
    // Returning false aborts the transfer; abortReason says why.
    hreq.content_receiver = [&](const char* data, size_t length, uint64_t, uint64_t) {
        received += length;
        if (std::chrono::steady_clock::now() >= deadline) {
            abortReason = timeoutError;
        } else if (received > kMaxStreamBytes) {
            abortReason = "Ollama response exceeded " + std::to_string(kMaxStreamBytes) + " bytes";
        } else if (status != 200) {
            errorBody.append(data, std::min(length, kMaxReplyBytes - errorBody.size()));
            if (errorBody.size() >= kMaxReplyBytes) {
                abortReason = "Ollama returned HTTP " + std::to_string(status) +
                              " with an oversized error body";
            }
        } else {
            assembler.feed(std::string_view(data, length), handleLine);
        }
        return abortReason.empty();
    };

    const httplib::Result result = cli.send(hreq);
    // A final line without a trailing newline still counts.
    if (result) {
        const std::string tail = assembler.takeRemainder();
        if (!tail.empty()) handleLine(tail);
    }
    // A read timeout that lands past the deadline (a stall after partial
    // output) is the same failure as the receiver noticing it.
    if (!result && abortReason.empty() && std::chrono::steady_clock::now() >= deadline) {
        abortReason = timeoutError;
    }
    if (!abortReason.empty()) {
        reply.error = abortReason;
        return reply;
    }
    if (!result) {
        const std::string detail = result.error() == httplib::Error::Success
                                       ? "request aborted"
                                       : to_string(result.error());
        reply.error = "could not reach Ollama at " + config_.host + ":" +
                      std::to_string(config_.port) + " (" + detail + ")";
        return reply;
    }

    if (status != 200) {
        reply.error = "Ollama returned HTTP " + std::to_string(status) +
                      (errorBody.empty() ? "" : ": " + errorBody);
        return reply;
    }
    if (!streamError.empty()) {
        reply.error = streamError;
        return reply;
    }
    if (!sawDone) {
        reply.error = "Ollama stream ended before completion";
        return reply;
    }
    reply.ok = true;
    reply.content = std::move(content);
    return reply;
}

}  // namespace llm_npc
