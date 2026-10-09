#include "AnthropicBackend.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

#include "httplib.h"
#include "json.hpp"

namespace llm_npc {

using nlohmann::json;

const char* const kAnthropicRefusalLine = "I'd rather not get into that. [[MOOD: neutral]]";

namespace {

// Fixed by the Messages API docs (platform.claude.com/docs/en/api/messages):
// the version header is required and this is the current value.
constexpr const char* kApiVersion = "2023-06-01";
constexpr const char* kMessagesPath = "/v1/messages";
constexpr const char* kLog = "[llm_npc] anthropic: ";

// Provider error bodies are short JSON, but a proxy can hand back a whole HTML
// page; keep the log to one readable line.
std::string clipForLog(const std::string& text) {
    constexpr std::size_t kMax = 240;
    if (text.size() <= kMax) return text;
    return text.substr(0, kMax) + "...";
}

std::uint64_t countOrZero(const json& usage, const char* key) {
    const auto it = usage.find(key);
    if (it == usage.end() || !it->is_number_unsigned()) return 0;
    return it->get<std::uint64_t>();
}

}  // namespace

AnthropicOutcome classifyExchange(bool reached, int status, const std::string& stopReason) {
    if (!reached) return AnthropicOutcome::Retry;
    if (status == 401) return AnthropicOutcome::Disable;
    if (status == 429 || status >= 500) return AnthropicOutcome::Retry;
    if (status != 200) return AnthropicOutcome::Fallback;
    if (stopReason == "refusal") return AnthropicOutcome::Refuse;
    // Both mean the text was cut short; the second is the context window
    // rather than max_tokens, and is just as unfit to render.
    if (stopReason == "max_tokens" || stopReason == "model_context_window_exceeded") {
        return AnthropicOutcome::Truncated;
    }
    return AnthropicOutcome::Deliver;
}

AnthropicBackend::AnthropicBackend(LlmConfig config)
    : config_(std::move(config)), model_(config_.anthropicModel) {}

AnthropicBackend::~AnthropicBackend() {
    const AnthropicUsage u = usage();
    if (u.requests == 0) return;
    std::cerr << kLog << "session usage: requests=" << u.requests << " input=" << u.inputTokens
              << " output=" << u.outputTokens << " cache_write=" << u.cacheWriteTokens
              << " cache_read=" << u.cacheReadTokens << " total=" << u.total() << "\n";
}

std::string AnthropicBackend::model() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return model_;
}

void AnthropicBackend::setModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(mutex_);
    model_ = model;
}

std::vector<std::string> AnthropicBackend::listModels() {
    // Selection is pinned by config; the picker gets the one model.
    return {model()};
}

AnthropicUsage AnthropicBackend::usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return usage_;
}

bool AnthropicBackend::disabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return disabled_;
}

void AnthropicBackend::disable(const std::string& why) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) return;
    disabled_ = true;
    std::cerr << kLog << why << "; using Ollama for the rest of this session\n";
}

BackendResult AnthropicBackend::decline(const std::string& why) const {
    BackendResult result;
    result.fallback = true;
    result.error = why;
    return result;
}

void AnthropicBackend::recordUsage(std::uint64_t input, std::uint64_t output,
                                   std::uint64_t cacheWrite, std::uint64_t cacheRead) {
    std::uint64_t total = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        usage_.requests += 1;
        usage_.inputTokens += input;
        usage_.outputTokens += output;
        usage_.cacheWriteTokens += cacheWrite;
        usage_.cacheReadTokens += cacheRead;
        total = usage_.total();
    }
    if (config_.anthropicTokenBudget > 0 && total >= config_.anthropicTokenBudget) {
        disable("session token budget of " + std::to_string(config_.anthropicTokenBudget) +
                " reached (used " + std::to_string(total) + ")");
    }
}

std::string AnthropicBackend::buildBody(const BackendRequest& req) const {
    // Last N turns only. The window must open with a user turn, which is what
    // the API expects and also what keeps a cut pair from leaving an orphaned
    // assistant line at the front.
    const auto& history = req.history;
    std::size_t begin = 0;
    if (config_.anthropicHistoryTurns > 0 &&
        history.size() > static_cast<std::size_t>(config_.anthropicHistoryTurns)) {
        begin = history.size() - static_cast<std::size_t>(config_.anthropicHistoryTurns);
    }
    while (begin < history.size() && history[begin].role != "user") ++begin;

    json messages = json::array();
    for (std::size_t i = begin; i < history.size(); ++i) {
        messages.push_back({{"role", history[i].role}, {"content", history[i].content}});
    }
    messages.push_back({{"role", "user"}, {"content", req.userMessage}});

    // No temperature/top_p/top_k: this model rejects any non-default value, and
    // its default is the one we would send. Thinking off and effort low so the
    // small max_tokens is spent on the visible reply, not on reasoning.
    json body = {
        {"model", model()},
        {"max_tokens", config_.anthropicMaxTokens},
        {"messages", messages},
        {"thinking", {{"type", "disabled"}}},
        {"output_config", {{"effort", "low"}}},
    };
    // One cached block carrying the whole stable prefix. Omitted when empty:
    // the API rejects an empty text block.
    if (!req.systemPrompt.empty()) {
        body["system"] = json::array({{{"type", "text"},
                                       {"text", req.systemPrompt},
                                       {"cache_control", {{"type", "ephemeral"}}}}});
    }
    return body.dump();
}

AnthropicBackend::Exchange AnthropicBackend::send(const ParsedUrl& url,
                                                  const std::string& body) const {
    httplib::Client cli(url.origin);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_write_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(10, 0);

    // The key travels in this header and nowhere else: never logged, never in
    // a body, never in an error string.
    const httplib::Headers headers = {
        {"x-api-key", config_.apiKey},
        {"anthropic-version", kApiVersion},
    };
    const httplib::Result result =
        cli.Post(url.pathPrefix + kMessagesPath, headers, body, "application/json");

    Exchange exchange;
    if (!result) {
        exchange.transportError = result.error() == httplib::Error::Success
                                      ? "request aborted"
                                      : to_string(result.error());
        return exchange;
    }
    exchange.reached = true;
    exchange.status = result->status;
    exchange.body = result->body;
    return exchange;
}

BackendResult AnthropicBackend::chat(const BackendRequest& req, const DeltaCallback& onDelta) {
    if (req.internal) {
        // Nothing to warm; a round trip here would only cost tokens.
        BackendResult result;
        result.ok = true;
        return result;
    }
    if (disabled()) return decline("anthropic backend is disabled for this session");

    const ParsedUrl url = parseBaseUrl(config_.anthropicBaseUrl);
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
    if (url.https) {
        if (!loggedTls_) {
            loggedTls_ = true;
            std::cerr << kLog << "built without OpenSSL, cannot reach " << config_.anthropicBaseUrl
                      << "; using Ollama\n";
        }
        return decline("built without OpenSSL; https provider unreachable");
    }
#endif

    const std::string body = buildBody(req);

    // Parses a 200 body for its stop reason; anything unparsable reads as an
    // empty reason, which the classifier treats as deliverable and the
    // delivery step below then rejects on its own terms.
    const auto stopReasonOf = [](const Exchange& exchange) -> std::string {
        if (!exchange.reached || exchange.status != 200) return "";
        const json parsed = json::parse(exchange.body, nullptr, false);
        if (parsed.is_discarded() || !parsed.contains("stop_reason") ||
            !parsed["stop_reason"].is_string()) {
            return "";
        }
        return parsed["stop_reason"].get<std::string>();
    };

    Exchange exchange = send(url, body);
    AnthropicOutcome outcome =
        classifyExchange(exchange.reached, exchange.status, stopReasonOf(exchange));
    if (outcome == AnthropicOutcome::Retry) {
        std::cerr << kLog << "transient failure ("
                  << (exchange.reached ? "HTTP " + std::to_string(exchange.status)
                                       : exchange.transportError)
                  << "), retrying once\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.anthropicRetryBackoffMs));
        exchange = send(url, body);
        outcome = classifyExchange(exchange.reached, exchange.status, stopReasonOf(exchange));
        if (outcome == AnthropicOutcome::Retry) outcome = AnthropicOutcome::Fallback;
    }

    switch (outcome) {
        case AnthropicOutcome::Disable:
            disable("HTTP 401 from the Messages API: the key in $" + config_.apiKeyEnv +
                    " (or config/secrets.cfg) was rejected");
            return decline("anthropic key rejected");
        case AnthropicOutcome::Fallback:
            return decline(exchange.reached
                               ? "HTTP " + std::to_string(exchange.status) + ": " +
                                     clipForLog(exchange.body)
                               : "could not reach " + config_.anthropicBaseUrl + " (" +
                                     exchange.transportError + ")");
        case AnthropicOutcome::Retry:  // converted to Fallback above
        case AnthropicOutcome::Deliver:
        case AnthropicOutcome::Refuse:
        case AnthropicOutcome::Truncated:
            break;
    }

    const json parsed = json::parse(exchange.body, nullptr, false);
    if (parsed.is_discarded() || !parsed.contains("content") || !parsed["content"].is_array()) {
        return decline("unparsable 200 body: " + clipForLog(exchange.body));
    }
    if (parsed.contains("usage") && parsed["usage"].is_object()) {
        const json& usage = parsed["usage"];
        recordUsage(countOrZero(usage, "input_tokens"), countOrZero(usage, "output_tokens"),
                    countOrZero(usage, "cache_creation_input_tokens"),
                    countOrZero(usage, "cache_read_input_tokens"));
    }

    BackendResult result;
    if (outcome == AnthropicOutcome::Refuse) {
        // The model's own refusal is final: the API offers no server-side
        // fallback and re-asking Ollama would answer a question Claude judged
        // unsafe. The player sees the NPC change the subject.
        result.ok = true;
        result.content = kAnthropicRefusalLine;
        onDelta(result.content);
        return result;
    }
    if (outcome == AnthropicOutcome::Truncated) {
        // A cut-off reply must never be rendered. ok=false with fallback=false
        // is the existing failure path (the "seems distracted" line).
        result.error = "reply hit max_tokens (" + std::to_string(config_.anthropicMaxTokens) +
                       "); raise anthropic_max_tokens or shorten the prompt";
        return result;
    }

    std::string text;
    for (const json& block : parsed["content"]) {
        // Thinking blocks and anything else are skipped by type.
        if (block.value("type", "") == "text") text += block.value("text", "");
    }
    result.ok = true;
    result.content = std::move(text);
    if (!result.content.empty()) onDelta(result.content);
    return result;
}

}  // namespace llm_npc
