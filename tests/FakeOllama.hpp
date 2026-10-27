#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "json.hpp"

namespace llm_npc_test {

// In-process stand-in for Ollama. Listens on a random localhost port and
// serves canned streams so the backends' protocol handling can be tested
// offline, including error paths and chunk framing that splits lines across
// TCP packets. Like real Ollama it answers both its native NDJSON /api/chat
// and the OpenAI-compatible SSE /v1/chat/completions, so one fake covers
// OllamaBackend and OpenAiBackend; every Mode applies to both wire formats.
class FakeOllama {
   public:
    enum class Mode {
        StreamWords,       // stream replyText word by word, then the end marker
        Http500,           // non-200 status with a JSON error body
        ErrorLine,         // 200, but the stream carries an {"error": ...} line
        TruncatedStream,   // 200, deltas arrive but the end marker never does
        KeepAliveForever,  // 200, a keep-alive line every 100 ms, never a reply
        Flood,             // 200, an endless run of bytes with no newline
        FloodError,        // 500, an endless error body
    };

    FakeOllama() {
        server_.Post("/api/chat", [this](const httplib::Request& req, httplib::Response& res) {
            handle(req, res, /*sse=*/false);
        });
        server_.Post("/v1/chat/completions",
                     [this](const httplib::Request& req, httplib::Response& res) {
                         handle(req, res, /*sse=*/true);
                     });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }

    ~FakeOllama() {
        // Endless and delayed responses watch this flag, so stop() never
        // waits on a handler that would otherwise run forever.
        stopping_ = true;
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    FakeOllama(const FakeOllama&) = delete;
    FakeOllama& operator=(const FakeOllama&) = delete;

    // The localhost port the fake is listening on.
    int port() const { return port_; }

    // Selects the canned behavior for subsequent requests. Safe to call while
    // a request is being served; that request keeps the mode it started with.
    void setMode(Mode mode) { mode_ = mode; }

    // Holds every response this long before its first byte — a slow model.
    void setResponseDelay(std::chrono::milliseconds delay) { delayMs_ = delay.count(); }

    // The assistant text streamed in StreamWords mode.
    void setReply(std::string text) {
        std::lock_guard<std::mutex> lock(mutex_);
        replyText_ = std::move(text);
    }

    // Raw JSON bodies of every request received so far.
    std::vector<std::string> requestBodies() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestBodies_;
    }

   private:
    // Records the request, waits out any configured delay, then answers in
    // the current mode using NDJSON (Ollama) or SSE (OpenAI) framing.
    void handle(const httplib::Request& req, httplib::Response& res, bool sse) {
        // Read before the body is recorded: a test that waits for the body and
        // then switches modes must not change this request's behaviour.
        const Mode mode = mode_;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            requestBodies_.push_back(req.body);
        }
        const auto delayEnd =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs_.load());
        while (!stopping_ && std::chrono::steady_clock::now() < delayEnd) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (mode == Mode::Http500) {
            res.status = 500;
            res.set_content("{\"error\":\"server exploded\"}", "application/json");
            return;
        }
        if (mode == Mode::KeepAliveForever) {
            // An SSE comment like OpenRouter's queue heartbeat; Ollama's
            // parser skips the blank line.
            serveForever(res, 200, sse ? ": OPENROUTER PROCESSING\n\n" : "\n",
                         std::chrono::milliseconds(100));
            return;
        }
        if (mode == Mode::Flood || mode == Mode::FloodError) {
            serveForever(res, mode == Mode::Flood ? 200 : 500, std::string(64 * 1024, 'x'),
                         std::chrono::milliseconds(0));
            return;
        }
        respondStreaming(res, mode, sse);
    }

    // Writes `piece` every `interval` until the client hangs up or the fake
    // shuts down.
    void serveForever(httplib::Response& res, int status, std::string piece,
                      std::chrono::milliseconds interval) {
        res.status = status;
        auto data = std::make_shared<std::string>(std::move(piece));
        res.set_chunked_content_provider(
            "text/plain",
            [this, data, interval](std::size_t, httplib::DataSink& sink) {
                const auto next = std::chrono::steady_clock::now() + interval;
                while (!stopping_ && std::chrono::steady_clock::now() < next) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (stopping_ || !sink.is_writable()) return false;
                return sink.write(data->data(), data->size());
            },
            nullptr);
    }

    // One assistant-text fragment in the requested framing.
    static std::string deltaLine(const std::string& text, bool sse) {
        if (sse) {
            nlohmann::json chunk = {
                {"choices", {{{"delta", {{"content", text}}}, {"finish_reason", nullptr}}}}};
            return "data: " + chunk.dump() + "\n\n";
        }
        nlohmann::json line = {{"message", {{"role", "assistant"}, {"content", text}}},
                               {"done", false}};
        return line.dump() + "\n";
    }

    // Builds the payload for `mode` and serves it in small fixed-size chunks
    // so lines straddle receive callbacks on the client.
    void respondStreaming(httplib::Response& res, Mode mode, bool sse) {
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (mode == Mode::ErrorLine) {
                payload = sse ? "data: {\"error\":{\"message\":\"model not found\"}}\n\n"
                              : "{\"error\":\"model not found\"}\n";
            } else {
                std::string word;
                std::vector<std::string> words;
                for (char c : replyText_) {
                    word.push_back(c);
                    if (c == ' ') {
                        words.push_back(word);
                        word.clear();
                    }
                }
                if (!word.empty()) words.push_back(word);
                for (const auto& w : words) payload += deltaLine(w, sse);
                if (mode == Mode::StreamWords && sse) {
                    payload +=
                        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n"
                        "data: [DONE]\n\n";
                } else if (mode == Mode::StreamWords) {
                    nlohmann::json final = {{"message", {{"role", "assistant"}, {"content", ""}}},
                                            {"done", true}};
                    payload += final.dump() + "\n";
                }
            }
        }

        // 7-byte chunks guarantee JSON lines split mid-token on the wire.
        const std::size_t kChunk = 7;
        auto data = std::make_shared<std::string>(std::move(payload));
        res.set_chunked_content_provider(
            sse ? "text/event-stream" : "application/x-ndjson",
            [data, kChunk](std::size_t offset, httplib::DataSink& sink) {
                if (offset >= data->size()) {
                    sink.done();
                    return true;
                }
                const std::size_t len = std::min<std::size_t>(kChunk, data->size() - offset);
                sink.write(data->data() + offset, len);
                return true;
            },
            nullptr);
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;
    std::atomic<Mode> mode_{Mode::StreamWords};
    std::atomic<long long> delayMs_{0};
    std::atomic<bool> stopping_{false};

    std::mutex mutex_;
    std::string replyText_ = "Hello there, traveler!";
    std::vector<std::string> requestBodies_;
};

}  // namespace llm_npc_test
