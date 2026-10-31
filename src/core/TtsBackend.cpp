#include "TtsBackend.hpp"

#include <iostream>

#include "ElevenLabsBackend.hpp"

namespace llm_npc {

std::uint64_t ttsCharacterCount(const std::string& text) {
    std::uint64_t count = 0;
    for (const char c : text) {
        // A UTF-8 continuation byte is 10xxxxxx; everything else starts a glyph.
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
    }
    return count;
}

std::unique_ptr<TtsBackend> makeTtsBackend(const TtsConfig& config) {
    if (config.provider.empty() || config.provider == "none") return nullptr;
    if (config.provider == "elevenlabs") {
        if (config.apiKey.empty()) return nullptr;  // loadTtsConfig already said why
        return std::make_unique<ElevenLabsBackend>(config);
    }
    std::cerr << "[llm_npc] tts: unknown provider \"" << config.provider << "\"; NPC voices off\n";
    return nullptr;
}

}  // namespace llm_npc
