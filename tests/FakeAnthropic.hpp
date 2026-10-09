#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "httplib.h"
#include "json.hpp"

namespace llm_npc_test {

// In-process stand-in for the Anthropic Messages API (POST /v1/messages) so the
// backend's request shape and every row of its error-handling table can be
// tested offline. Records each request's body and the two headers the API
// requires. Response shapes follow platform.claude.com/docs/en/api/messages.
class FakeAnthropic {
   public:
    enum class Mode {
        Ok,              // 200, stop_reason end_turn, replyText
        Refusal,         // 200, stop_reason refusal
        MaxTokens,       // 200, stop_reason max_tokens
        Http401,         // authentication_error
        Http429,         // rate_limit_error
        Http500,         // api_error
        FailOnceThenOk,  // first request 500, every later one Ok
        BadJson,         // 200 whose body is not JSON
        MalformedBlocks, // 200 whose content blocks are the wrong shape
        EmptyContent,    // 200 with an empty content array
    };

    FakeAnthropic() {
        server_.Post("/v1/messages", [this](const httplib::Request& req, httplib::Response& res) {
            Mode mode;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                requestBodies_.push_back(req.body);
                std::map<std::string, std::string> headers;
                headers["x-api-key"] = req.get_header_value("x-api-key");
                headers["anthropic-version"] = req.get_header_value("anthropic-version");
                headers["content-type"] = req.get_header_value("Content-Type");
                requestHeaders_.push_back(headers);
                mode = mode_;
                if (mode == Mode::FailOnceThenOk) {
                    mode = requestBodies_.size() == 1 ? Mode::Http500 : Mode::Ok;
                }
            }
            respond(mode, res);
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }

    ~FakeAnthropic() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    FakeAnthropic(const FakeAnthropic&) = delete;
    FakeAnthropic& operator=(const FakeAnthropic&) = delete;

    // The localhost port the fake is listening on.
    int port() const { return port_; }
    // An http base URL the backend can be pointed at.
    std::string baseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

    void setMode(Mode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        mode_ = mode;
    }
    void setReply(std::string text) {
        std::lock_guard<std::mutex> lock(mutex_);
        replyText_ = std::move(text);
    }
    // The usage block every 200 carries.
    void setUsage(std::uint64_t input, std::uint64_t output, std::uint64_t cacheWrite,
                  std::uint64_t cacheRead) {
        std::lock_guard<std::mutex> lock(mutex_);
        input_ = input;
        output_ = output;
        cacheWrite_ = cacheWrite;
        cacheRead_ = cacheRead;
    }

    // Raw JSON bodies of every request received so far.
    std::vector<std::string> requestBodies() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestBodies_;
    }
    // x-api-key / anthropic-version / content-type of every request so far.
    std::vector<std::map<std::string, std::string>> requestHeaders() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requestHeaders_;
    }

   private:
    void respond(Mode mode, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto error = [&](int status, const char* type, const char* message) {
            res.status = status;
            nlohmann::json body = {{"type", "error"},
                                   {"error", {{"type", type}, {"message", message}}}};
            res.set_content(body.dump(), "application/json");
        };
        switch (mode) {
            case Mode::Http401:
                error(401, "authentication_error", "invalid x-api-key");
                return;
            case Mode::Http429:
                error(429, "rate_limit_error", "rate limited");
                return;
            case Mode::Http500:
            case Mode::FailOnceThenOk:
                error(500, "api_error", "internal server error");
                return;
            case Mode::BadJson:
                res.status = 200;
                res.set_content("<html>not json</html>", "text/html");
                return;
            case Mode::Ok:
            case Mode::Refusal:
            case Mode::MaxTokens:
            case Mode::MalformedBlocks:
            case Mode::EmptyContent:
                break;
        }
        nlohmann::json body = {
            {"id", "msg_test"},
            {"type", "message"},
            {"role", "assistant"},
            {"model", "claude-haiku-5-5"},
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", replyText_}}})},
            {"stop_reason", mode == Mode::Refusal     ? "refusal"
                            : mode == Mode::MaxTokens ? "max_tokens"
                                                      : "end_turn"},
            {"stop_sequence", nullptr},
            {"usage",
             {{"input_tokens", input_},
              {"output_tokens", output_},
              {"cache_creation_input_tokens", cacheWrite_},
              {"cache_read_input_tokens", cacheRead_}}},
        };
        if (mode == Mode::Refusal) {
            body["stop_details"] = {{"type", "refusal"}, {"category", "general_harms"}};
            body["content"] = nlohmann::json::array();
        }
        if (mode == Mode::EmptyContent) body["content"] = nlohmann::json::array();
        if (mode == Mode::MalformedBlocks) {
            // A bare string, a block with a null text, a block whose type is
            // not a string, and a thinking block: none of them is a reply.
            body["content"] = nlohmann::json::array(
                {"just a string",
                 {{"type", "text"}, {"text", nullptr}},
                 {{"type", 7}, {"text", "x"}},
                 {{"type", "thinking"}, {"thinking", "..."}}});
        }
        res.status = 200;
        res.set_content(body.dump(), "application/json");
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;

    std::mutex mutex_;
    Mode mode_ = Mode::Ok;
    std::string replyText_ = "Lovely morning for it. [[MOOD: happy]]";
    std::uint64_t input_ = 120;
    std::uint64_t output_ = 18;
    std::uint64_t cacheWrite_ = 0;
    std::uint64_t cacheRead_ = 0;
    std::vector<std::string> requestBodies_;
    std::vector<std::map<std::string, std::string>> requestHeaders_;
};

}  // namespace llm_npc_test
