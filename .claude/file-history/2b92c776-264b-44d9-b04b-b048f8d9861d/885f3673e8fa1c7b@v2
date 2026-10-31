#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "LlmBackend.hpp"
#include "LlmClient.hpp"  // LlmConfig

namespace llm_npc {

// The default provider: a local Ollama server. Streams NDJSON from /api/chat for
// replies and reads /api/tags for the installed-model list. No TLS, so it needs
// no extra dependencies.
class OllamaBackend : public LlmBackend {
   public:
    explicit OllamaBackend(LlmConfig config);

    BackendResult chat(const BackendRequest& req, const DeltaCallback& onDelta) override;
    std::vector<std::string> listModels() override;
    std::string model() const override;
    void setModel(const std::string& model) override;

   private:
    LlmConfig config_;
    // The model can be switched from the main thread (picker) while chat() runs
    // on the worker thread; everything else in config_ is fixed after construction.
    mutable std::mutex modelMutex_;
};

}  // namespace llm_npc
