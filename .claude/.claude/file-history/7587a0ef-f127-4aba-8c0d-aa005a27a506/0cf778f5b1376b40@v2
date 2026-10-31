#include "LlmClient.hpp"

#include <chrono>
#include <utility>

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

std::uint64_t LlmClient::submit(std::string systemPrompt,
                                std::vector<ChatTurn> history,
                                std::string userMessage) {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.systemPrompt = std::move(systemPrompt);
    req.history = std::move(history);
    req.userMessage = std::move(userMessage);

    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        requests_.push(std::move(req));
    }
    requestCv_.notify_one();
    return req.id;
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
        {
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

    // Build Ollama /api/chat body. Ollama is OpenAI-style: messages[] with
    // role/content, optional options for temperature, stream=false for a
    // single non-streamed response.
    json messages = json::array();
    if (!req.systemPrompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", req.systemPrompt}});
    }
    for (const auto& turn : req.history) {
        messages.push_back({{"role", turn.role}, {"content", turn.content}});
    }
    messages.push_back({{"role", "user"}, {"content", req.userMessage}});

    json body = {
        {"model", config_.model},
        {"messages", messages},
        {"stream", false},
        {"options", {{"temperature", config_.temperature}}},
    };
    std::string bodyStr = body.dump();

    httplib::Client cli(config_.host, config_.port);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_write_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(5, 0);

    auto res = cli.Post("/api/chat", bodyStr, "application/json");
    if (!res) {
        reply.errorMessage = "could not reach Ollama at " + config_.host + ":" +
                             std::to_string(config_.port);
        return reply;
    }
    if (res->status != 200) {
        reply.errorMessage = "Ollama returned HTTP " + std::to_string(res->status) +
                             ": " + res->body;
        return reply;
    }

    try {
        json parsed = json::parse(res->body);
        // Ollama replies with { "message": { "role": ..., "content": ... }, ... }
        if (parsed.contains("message") && parsed["message"].contains("content")) {
            reply.content = parsed["message"]["content"].get<std::string>();
            reply.ok = true;
        } else if (parsed.contains("error")) {
            reply.errorMessage = parsed["error"].get<std::string>();
        } else {
            reply.errorMessage = "unexpected response shape from Ollama";
        }
    } catch (const std::exception& e) {
        reply.errorMessage = std::string("failed to parse Ollama reply: ") + e.what();
    }
    return reply;
}

}  // namespace llm_npc
