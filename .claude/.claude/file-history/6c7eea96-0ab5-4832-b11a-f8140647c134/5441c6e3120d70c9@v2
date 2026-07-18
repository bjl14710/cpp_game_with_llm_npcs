#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "httplib.h"
#include "json.hpp"

namespace llm_npc_test {

// In-process stand-in for Ollama's /api/chat endpoint. Listens on a random
// localhost port and serves canned NDJSON streams so LlmClient's protocol
// handling can be tested offline, including error paths and chunk framing
// that splits JSON lines across TCP packets.
class FakeOllama {
   public:
    enum class Mode {
        StreamWords,      // stream replyText word by word, then done:true
        Http500,          // non-200 status with a JSON error body
        ErrorLine,        // 200, but the stream carries an {"error": ...} line
        TruncatedStream,  // 200, deltas arrive but done:true never does
    };

    FakeOllama() {
        server_.Post("/api/chat", [this](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                requestBodies_.push_back(req.body);
            }
            if (mode_ == Mode::Http500) {
                res.status = 500;
                res.set_content("{\"error\":\"server exploded\"}", "application/json");
                return;
            }
            respondStreaming(res);
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }

    ~FakeOllama() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    FakeOllama(const FakeOllama&) = delete;
    FakeOllama& operator=(const FakeOllama&) = delete;

    // The localhost port the fake is listening on.
    int port() const { return port_; }

    // Selects the canned behavior for subsequent requests.
    void setMode(Mode mode) { mode_ = mode; }

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
    // Builds the NDJSON payload for the current mode and serves it in small
    // fixed-size chunks so lines straddle receive callbacks on the client.
    void respondStreaming(httplib::Response& res) {
        std::string payload;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (mode_ == Mode::ErrorLine) {
                payload = "{\"error\":\"model not found\"}\n";
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
                for (const auto& w : words) {
                    nlohmann::json line = {{"message", {{"role", "assistant"}, {"content", w}}},
                                           {"done", false}};
                    payload += line.dump() + "\n";
                }
                if (mode_ == Mode::StreamWords) {
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
            "application/x-ndjson",
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
    Mode mode_ = Mode::StreamWords;

    std::mutex mutex_;
    std::string replyText_ = "Hello there, traveler!";
    std::vector<std::string> requestBodies_;
};

}  // namespace llm_npc_test
