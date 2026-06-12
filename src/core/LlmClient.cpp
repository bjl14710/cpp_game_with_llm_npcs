#include "LlmClient.hpp"

#include <string_view>
#include <utility>

#include "StreamAssembler.hpp"
#include "httplib.h"
#include "json.hpp"

namespace llm_npc {

using nlohmann::json;

LlmClient::LlmClient(LlmConfig config) : config_(std::move(config)) {
    worker_ = std::thread(&LlmClient::workerLoop, this);
}

LlmClient::~LlmClient() {
    stop_.store(true);
    requestCv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void LlmClient::enqueue(ChatRequest req) {
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        requests_.push(std::move(req));
    }
    requestCv_.notify_one();
}

std::uint64_t LlmClient::submit(std::string systemPrompt,
                                std::vector<ChatTurn> history,
                                std::string userMessage) {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.systemPrompt = std::move(systemPrompt);
    req.history = std::move(history);
    req.userMessage = std::move(userMessage);

    const std::uint64_t id = req.id;
    enqueue(std::move(req));
    return id;
}

void LlmClient::warmUp() {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.internal = true;
    enqueue(std::move(req));
}

std::vector<ChatDelta> LlmClient::drainDeltas() {
    std::vector<ChatDelta> out;
    std::lock_guard<std::mutex> lock(replyMutex_);
    out.swap(deltas_);
    return out;
}

std::vector<ChatReply> LlmClient::drainReplies() {
    std::vector<ChatReply> out;
    std::lock_guard<std::mutex> lock(replyMutex_);
    out.swap(replies_);
    return out;
}

void LlmClient::workerLoop() {
    while (true) {
        ChatRequest req;
        {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait(lock, [this] { return stop_.load() || !requests_.empty(); });
            if (stop_.load() && requests_.empty()) return;
            req = std::move(requests_.front());
            requests_.pop();
        }

        inFlight_.store(true);
        ChatReply reply = processOne(req);
        if (!req.internal) {
            std::lock_guard<std::mutex> lock(replyMutex_);
            replies_.push_back(std::move(reply));
        }
        // Drop the flag only after the reply is queued so UI can poll busy()
        // without seeing a brief idle window before the reply lands.
        inFlight_.store(false);
    }
}

ChatReply LlmClient::processOne(const ChatRequest& req) {
    ChatReply reply;
    reply.id = req.id;

    // Build Ollama /api/chat body. Streaming NDJSON keeps perceived latency
    // low (first words appear immediately); keep_alive holds the model in
    // memory between conversations. Warm-up requests send no messages, which
    // makes Ollama load the model and return at once.
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
        {"model", config_.model},
        {"messages", messages},
        {"stream", true},
        {"keep_alive", config_.keepAlive},
        {"options", {{"temperature", config_.temperature}}},
    };

    httplib::Client cli(config_.host, config_.port);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_write_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(5, 0);

    int status = 0;
    std::string errorBody;   // body bytes of a non-200 response
    std::string content;     // accumulated assistant text
    std::string streamError; // first error found inside the stream
    bool sawDone = false;
    StreamAssembler assembler;

    // Parses one NDJSON line: append text, surface deltas, note completion.
    const auto handleLine = [&](const std::string& line) {
        const OllamaChunk chunk = parseOllamaChunk(line);
        if (!chunk.error.empty()) {
            if (streamError.empty()) streamError = chunk.error;
            return;
        }
        if (!chunk.delta.empty()) {
            content += chunk.delta;
            if (!req.internal) {
                std::lock_guard<std::mutex> lock(replyMutex_);
                deltas_.push_back(ChatDelta{req.id, chunk.delta});
            }
        }
        if (chunk.done) sawDone = true;
    };

    // httplib v0.15 has no streaming Post() overload, so drive a raw Request
    // through Client::send with a content receiver.
    httplib::Request hreq;
    hreq.method = "POST";
    hreq.path = "/api/chat";
    hreq.set_header("Content-Type", "application/json");
    hreq.body = body.dump();
    hreq.response_handler = [&](const httplib::Response& response) {
        status = response.status;
        return true;
    };
    hreq.content_receiver = [&](const char* data, size_t length, uint64_t, uint64_t) {
        if (status != 200) {
            errorBody.append(data, length);
        } else {
            assembler.feed(std::string_view(data, length), handleLine);
        }
        return true;
    };

    const httplib::Result result = cli.send(hreq);
    if (!result) {
        const std::string detail = result.error() == httplib::Error::Success
                                       ? "request aborted"
                                       : to_string(result.error());
        reply.errorMessage = "could not reach Ollama at " + config_.host + ":" +
                             std::to_string(config_.port) + " (" + detail + ")";
        return reply;
    }

    // A final line without a trailing newline still counts.
    const std::string tail = assembler.takeRemainder();
    if (!tail.empty()) handleLine(tail);

    if (status != 200) {
        reply.errorMessage = "Ollama returned HTTP " + std::to_string(status) +
                             (errorBody.empty() ? "" : ": " + errorBody);
        return reply;
    }
    if (!streamError.empty()) {
        reply.errorMessage = streamError;
        return reply;
    }
    if (!sawDone) {
        reply.errorMessage = "Ollama stream ended before completion";
        return reply;
    }
    reply.ok = true;
    reply.content = std::move(content);
    return reply;
}

}  // namespace llm_npc
