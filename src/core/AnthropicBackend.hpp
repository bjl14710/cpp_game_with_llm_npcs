#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "BaseUrl.hpp"
#include "LlmBackend.hpp"
#include "LlmClient.hpp"  // LlmConfig

namespace llm_npc {

// What one HTTP exchange with the Messages API means for the request it carried.
// This is the ERROR HANDLING table of plan anthropic-haiku-backend as a type, so
// a test can pin every row without a network.
enum class AnthropicOutcome {
    Deliver,    // 200 and the model finished: hand the text on
    Refuse,     // 200 with stop_reason "refusal": in-character line, no retry
    Truncated,  // 200 but cut off (max_tokens): a failed reply, never rendered
    Retry,      // 429, 5xx, timeout, unreachable: one more try, then Fallback
    Fallback,   // any other failure: Ollama answers this one request
    Disable,    // 401: the key is wrong; Ollama for the rest of the session
};

// `reached` is false when no HTTP status came back at all (timeout, connection
// refused, DNS). `stopReason` is only consulted for a 200.
AnthropicOutcome classifyExchange(bool reached, int status, const std::string& stopReason);

// Token counts read from the `usage` block of every 200 and summed for the
// session. Billable input is inputTokens + cacheWriteTokens + cacheReadTokens.
struct AnthropicUsage {
    std::uint64_t requests = 0;
    std::uint64_t inputTokens = 0;
    std::uint64_t outputTokens = 0;
    std::uint64_t cacheWriteTokens = 0;
    std::uint64_t cacheReadTokens = 0;

    // Every counter summed; what the session budget is measured against.
    std::uint64_t total() const {
        return inputTokens + outputTokens + cacheWriteTokens + cacheReadTokens;
    }
};

// The reply a player sees when the model refuses. Carries a MOOD tag so it
// passes the directive contract like any generated line.
extern const char* const kAnthropicRefusalLine;

// Claude via the Anthropic Messages API, non-streaming: one POST per turn, the
// whole text delivered as a single delta. Never answers a warm-up (nothing to
// keep resident) and never retries a refusal. Declines (BackendResult::fallback)
// rather than fails whenever Ollama could reasonably take the request instead;
// makeBackend wraps it so that is exactly what happens.
//
// The system prompt goes out as one cached text block and the history is capped
// to the last N turns, so the stable prefix (persona, lore, role block) is what
// the cache keys on. Note the API's 512-token minimum cacheable prefix on this
// model: a short persona is silently uncached and both cache counters stay 0.
class AnthropicBackend : public LlmBackend {
   public:
    explicit AnthropicBackend(LlmConfig config);
    // Prints the session's usage totals as one [llm_npc] line, if anything was
    // sent. LlmClient destructs at shutdown, so this is the shutdown summary.
    ~AnthropicBackend() override;

    BackendResult chat(const BackendRequest& req, const DeltaCallback& onDelta) override;
    std::vector<std::string> listModels() override;
    std::string model() const override;
    void setModel(const std::string& model) override;

    AnthropicUsage usage() const;
    // True once a 401 or the token budget has switched the session to Ollama.
    bool disabled() const;

   private:
    // One raw HTTP exchange; `reached` is false when no status came back.
    struct Exchange {
        bool reached = false;
        int status = 0;
        std::string body;
        std::string transportError;
    };

    std::string buildBody(const BackendRequest& req) const;
    Exchange send(const ParsedUrl& url, const std::string& body) const;
    // A result that asks the fallback to take the request. Logs `why` once per
    // distinct reason, so a provider that is down for an hour prints one line,
    // not one per NPC turn.
    BackendResult decline(const std::string& why) const;
    void disable(const std::string& why);
    // Adds one response's usage block to the totals and applies the budget.
    void recordUsage(std::uint64_t input, std::uint64_t output, std::uint64_t cacheWrite,
                     std::uint64_t cacheRead);

    LlmConfig config_;
    // Guards model_, usage_ and disabled_: chat() runs on the worker thread while
    // the picker, usage() and the destructor run on the main thread.
    mutable std::mutex mutex_;
    std::string model_;
    AnthropicUsage usage_;
    bool disabled_ = false;
    mutable std::string lastDeclineReason_;
    // Touched only from chat() on the single worker thread.
    bool loggedTls_ = false;
};

}  // namespace llm_npc
