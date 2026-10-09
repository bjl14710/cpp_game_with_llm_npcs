#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace llm_npc {

// Settings for the optional cloud TTS backend (plan: elevenlabs-tts-backend).
// Loaded from <configDir>/tts.cfg; every field has a default that leaves the
// game silent, exactly as it is without this feature.
struct TtsConfig {
    // "none" (default: no audio device, no network, no thread) or "elevenlabs".
    std::string provider = "none";
    // ElevenLabs model id. Flash v2.5 is the low-latency model per
    // elevenlabs.io/docs/models (~75 ms, 40,000-character limit).
    std::string elevenLabsModel = "eleven_flash_v2_5";
    // ElevenLabs `output_format` query value. pcm_* is raw 16-bit mono PCM that
    // WavEncode wraps in a header; wav_* is passed through. 22.05 kHz is the
    // cheapest rate that is still clean for a voice; 44.1 kHz PCM/WAV needs the
    // Pro tier per the convert endpoint docs.
    std::string elevenLabsOutputFormat = "pcm_22050";
    // API origin. Tests point this at an in-process fake over http.
    std::string elevenLabsBaseUrl = "https://api.elevenlabs.io";
    // Per-attempt read timeout and the pause before the one retry a 429 / 5xx /
    // timeout earns.
    int elevenLabsTimeoutSeconds = 15;
    int elevenLabsRetryBackoffMs = 750;
    // Session cap on characters sent to the API (cache hits excluded); 0 means
    // unlimited. Once reached the backend is disabled and one log line says so.
    std::uint64_t characterBudget = 0;
    // Environment variable the key is read from before config/secrets.cfg.
    std::string apiKeyEnv = "NPC_GAME_ELEVENLABS_API_KEY";
    // The resolved key. Filled at load time, never written back, never logged.
    std::string apiKey;
    // On-disk audio cache. Relative paths resolve against the project root
    // (the parent of the config dir); saves/ is gitignored.
    std::filesystem::path cacheDir;
    // Size cap for the cache; least recently used files are evicted past it.
    std::uint64_t cacheMaxMb = 200;
};

// One NPC's voice assignment from <configDir>/voices.cfg.
struct VoiceEntry {
    std::string backend;  // "elevenlabs"; anything else is treated as no voice
    std::string voiceId;
    std::string modelId;  // empty: use TtsConfig::elevenLabsModel
};

// NPC id (the persona name, the key every per-NPC store uses) -> voice.
using VoiceMap = std::unordered_map<std::string, VoiceEntry>;

// Loads tts.cfg and resolves the key from $apiKeyEnv, then the gitignored
// config/secrets.cfg key `elevenlabs_api_key`. A missing file yields defaults;
// a provider other than "none" with no key is downgraded to "none" with one log
// line, so the caller never needs a key check of its own.
TtsConfig loadTtsConfig(const std::filesystem::path& configDir);

// Loads voices.cfg. Lines are `npc.<name>.voice_id = ...`,
// `npc.<name>.model_id = ...`, `npc.<name>.backend = ...`. Entries with no
// voice_id are dropped; a missing file yields an empty map.
VoiceMap loadVoiceMap(const std::filesystem::path& configDir);

}  // namespace llm_npc
