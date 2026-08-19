#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "LlmBackend.hpp"
#include "LlmClient.hpp"  // LlmConfig

namespace llm_npc {

// Talks to any OpenAI-compatible /chat/completions endpoint: OpenRouter (the
// default base), OpenAI, Gemini's compat layer, or a local Ollama /v1. Streams
// Server-Sent Events. HTTPS bases require building with OpenSSL
// (CPPHTTPLIB_OPENSSL_SUPPORT); without it only http bases (e.g. local) work and
// an https base returns a clear error.
class OpenAiBackend : public LlmBackend {
   public:
    explicit OpenAiBackend(LlmConfig config);

    BackendResult chat(const BackendRequest& req, const DeltaCallback& onDelta) override;
    std::vector<std::string> listModels() override;
    std::string model() const override;
    void setModel(const std::string& model) override;

   private:
    LlmConfig config_;
    mutable std::mutex modelMutex_;  // guards config_.model against the picker
};

}  // namespace llm_npc
