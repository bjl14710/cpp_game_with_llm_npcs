#include "LlmBackend.hpp"

#include "OllamaBackend.hpp"
#include "OpenAiBackend.hpp"

namespace llm_npc {

std::unique_ptr<LlmBackend> makeBackend(const LlmConfig& config) {
    if (config.provider.empty() || config.provider == "ollama") {
        return std::make_unique<OllamaBackend>(config);
    }
    // Every other provider (openrouter, openai, gemini, a local /v1, ...) speaks
    // the OpenAI-compatible wire format.
    return std::make_unique<OpenAiBackend>(config);
}

}  // namespace llm_npc
