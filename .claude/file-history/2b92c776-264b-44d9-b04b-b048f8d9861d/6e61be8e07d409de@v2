#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "LlmClient.hpp"  // ChatTurn, LlmConfig

namespace llm_npc {

// One chat turn handed to a backend. `internal` marks a warm-up request that
// must produce no user-visible reply or deltas.
struct BackendRequest {
    std::string systemPrompt;
    std::vector<ChatTurn> history;
    std::string userMessage;
    bool internal = false;
};

// Outcome of a backend chat call: the assembled assistant text on success, or a
// human-readable error.
struct BackendResult {
    bool ok = false;
    std::string content;
    std::string error;
};

// Invoked with each streamed text fragment as it arrives, in order.
using DeltaCallback = std::function<void(const std::string&)>;

// A provider that turns a chat request into a (streamed) reply. Implementations
// own the HTTP and wire-format details; LlmClient owns the worker thread and the
// delta/reply queues and calls chat() from that thread.
class LlmBackend {
   public:
    virtual ~LlmBackend() = default;

    // Performs one chat turn, invoking onDelta for each text fragment, and
    // returns the assembled result.
    virtual BackendResult chat(const BackendRequest& req, const DeltaCallback& onDelta) = 0;

    // Models the in-game picker may offer (e.g. installed Ollama tags). May be
    // empty or a single entry when the provider doesn't enumerate models.
    virtual std::vector<std::string> listModels() = 0;

    // The model used for new requests, and a live setter for the picker.
    virtual std::string model() const = 0;
    virtual void setModel(const std::string& model) = 0;
};

// Builds the backend named by config.provider: "ollama" (the default) selects
// the native Ollama backend; any other value selects the OpenAI-compatible
// backend (OpenRouter, OpenAI, Gemini's compat layer, a local /v1, ...).
std::unique_ptr<LlmBackend> makeBackend(const LlmConfig& config);

}  // namespace llm_npc
