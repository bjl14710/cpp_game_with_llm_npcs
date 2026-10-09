#include "TtsConfig.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "Config.hpp"

namespace llm_npc {

namespace {

// `kv[key]` as a non-negative integer no greater than `max`, or `fallback`
// (with one log line) when the key is absent, not all digits, negative or too
// large: stoull would throw on "abc" and silently wrap "-1".
std::uint64_t readCount(const std::unordered_map<std::string, std::string>& kv,
                        const std::string& key, std::uint64_t fallback,
                        std::uint64_t max = std::numeric_limits<std::uint64_t>::max()) {
    const auto it = kv.find(key);
    if (it == kv.end()) return fallback;
    const std::string value = trim(it->second);
    bool ok = !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    std::uint64_t parsed = 0;
    if (ok) {
        try {
            parsed = std::stoull(value);
        } catch (const std::exception&) {
            ok = false;  // out_of_range
        }
    }
    if (!ok || parsed > max) {
        std::cerr << "[llm_npc] tts: tts.cfg " << key << " = \"" << it->second
                  << "\" is not a whole number in range; using " << fallback << "\n";
        return fallback;
    }
    return parsed;
}

}  // namespace

TtsConfig loadTtsConfig(const std::filesystem::path& configDir) {
    TtsConfig cfg;
    auto kv = readKv(configDir / "tts.cfg");
    if (auto it = kv.find("provider"); it != kv.end()) cfg.provider = it->second;
    if (auto it = kv.find("elevenlabs_model"); it != kv.end()) cfg.elevenLabsModel = it->second;
    if (auto it = kv.find("elevenlabs_output_format"); it != kv.end()) {
        cfg.elevenLabsOutputFormat = it->second;
    }
    if (auto it = kv.find("elevenlabs_base_url"); it != kv.end()) {
        cfg.elevenLabsBaseUrl = it->second;
    }
    constexpr std::uint64_t kIntMax = std::numeric_limits<int>::max();
    cfg.elevenLabsTimeoutSeconds = static_cast<int>(
        readCount(kv, "elevenlabs_timeout_s",
                  static_cast<std::uint64_t>(cfg.elevenLabsTimeoutSeconds), kIntMax));
    cfg.elevenLabsRetryBackoffMs = static_cast<int>(
        readCount(kv, "elevenlabs_retry_backoff_ms",
                  static_cast<std::uint64_t>(cfg.elevenLabsRetryBackoffMs), kIntMax));
    cfg.characterBudget = readCount(kv, "character_budget", cfg.characterBudget);
    if (auto it = kv.find("api_key_env"); it != kv.end()) cfg.apiKeyEnv = it->second;
    cfg.cacheMaxMb = readCount(kv, "cache_max_mb", cfg.cacheMaxMb);

    std::filesystem::path cacheDir = "saves/tts_cache";
    if (auto it = kv.find("cache_dir"); it != kv.end() && !it->second.empty()) {
        cacheDir = it->second;
    }
    cfg.cacheDir = cacheDir.is_absolute() ? cacheDir : configDir.parent_path() / cacheDir;

    if (cfg.provider == "none") return cfg;

    // Env var first, then the gitignored secrets file under its own key so an
    // LLM key in the shared `api_key` slot is never sent to ElevenLabs.
    // A key pasted into a shell export often carries a trailing newline or
    // space; trim it the way readKv trims the file value.
    const char* env = std::getenv(cfg.apiKeyEnv.c_str());
    if (const std::string envKey = env ? trim(env) : ""; !envKey.empty()) {
        cfg.apiKey = envKey;
    } else {
        auto secrets = readKv(configDir / "secrets.cfg");
        if (auto it = secrets.find("elevenlabs_api_key"); it != secrets.end()) {
            cfg.apiKey = it->second;
        }
    }
    if (cfg.apiKey.empty()) {
        std::cerr << "[llm_npc] tts: provider = " << cfg.provider << " but no API key was found in $"
                  << cfg.apiKeyEnv << " or config/secrets.cfg (elevenlabs_api_key); NPC voices off\n";
        cfg.provider = "none";
    }
    return cfg;
}

VoiceMap loadVoiceMap(const std::filesystem::path& configDir) {
    VoiceMap voices;
    for (const auto& [key, value] : readKv(configDir / "voices.cfg")) {
        // npc.<name>.<field>; the name may itself contain dots, so split on the
        // first and the last.
        if (key.rfind("npc.", 0) != 0) continue;
        const auto lastDot = key.rfind('.');
        if (lastDot == std::string::npos || lastDot <= 4) continue;
        const std::string name = key.substr(4, lastDot - 4);
        const std::string field = key.substr(lastDot + 1);
        VoiceEntry& entry = voices[name];
        if (field == "voice_id") {
            entry.voiceId = value;
        } else if (field == "model_id") {
            entry.modelId = value;
        } else if (field == "backend") {
            entry.backend = value;
        }
    }
    for (auto it = voices.begin(); it != voices.end();) {
        if (it->second.backend.empty()) it->second.backend = "elevenlabs";
        if (it->second.voiceId.empty()) {
            std::cerr << "[llm_npc] tts: voices.cfg entry for \"" << it->first
                      << "\" has no voice_id; ignored\n";
            it = voices.erase(it);
        } else {
            ++it;
        }
    }
    return voices;
}

}  // namespace llm_npc
