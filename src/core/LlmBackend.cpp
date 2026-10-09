#include "LlmBackend.hpp"

#include <iostream>
#include <utility>

#include "AnthropicBackend.hpp"
#include "OllamaBackend.hpp"
#include "OpenAiBackend.hpp"

namespace llm_npc {

namespace {

// Two backends behind one seam: `primary` answers unless it declines
// (BackendResult::fallback), in which case `secondary` takes the same request.
// Warm-up goes straight to the secondary — the cloud has nothing to keep
// resident, and the local model must be warm for the moment the cloud drops out.
class FallbackBackend : public LlmBackend {
   public:
    FallbackBackend(std::unique_ptr<LlmBackend> primary, std::unique_ptr<LlmBackend> secondary)
        : primary_(std::move(primary)), secondary_(std::move(secondary)) {}

    BackendResult chat(const BackendRequest& req, const DeltaCallback& onDelta) override {
        if (req.internal) return secondary_->chat(req, onDelta);
        BackendResult result = primary_->chat(req, onDelta);
        if (!result.fallback) return result;
        return secondary_->chat(req, onDelta);
    }
    std::vector<std::string> listModels() override { return primary_->listModels(); }
    std::string model() const override { return primary_->model(); }
    void setModel(const std::string& model) override { primary_->setModel(model); }

   private:
    std::unique_ptr<LlmBackend> primary_;
    std::unique_ptr<LlmBackend> secondary_;
};

}  // namespace

std::unique_ptr<LlmBackend> makeBackend(const LlmConfig& config) {
    if (config.provider.empty() || config.provider == "ollama") {
        return std::make_unique<OllamaBackend>(config);
    }
    if (config.provider == "anthropic") {
        // No key means the cloud backend is never constructed, so the game runs
        // exactly as `provider = ollama` would — this is the only line that
        // tells the host why.
        if (config.apiKey.empty()) {
            std::cerr << "[llm_npc] provider = anthropic but no API key was found in $"
                      << config.apiKeyEnv << " or config/secrets.cfg; using Ollama ("
                      << config.model << " at " << config.host << ":" << config.port << ")\n";
            return std::make_unique<OllamaBackend>(config);
        }
        return std::make_unique<FallbackBackend>(std::make_unique<AnthropicBackend>(config),
                                                 std::make_unique<OllamaBackend>(config));
    }
    // Every other provider (openrouter, openai, gemini, a local /v1, ...) speaks
    // the OpenAI-compatible wire format.
    return std::make_unique<OpenAiBackend>(config);
}

}  // namespace llm_npc
