// tts.cfg and voices.cfg loading: defaults, parsing, key resolution and the
// no-key downgrade to provider none.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "TtsBackend.hpp"
#include "TtsConfig.hpp"
#include "doctest.h"

using namespace llm_npc;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("llm_npc_tts_cfg_" + std::to_string(std::rand()));
        fs::create_directories(path / "config");
    }
    ~TempDir() { fs::remove_all(path); }
    fs::path configDir() const { return path / "config"; }
    void write(const std::string& name, const std::string& content) const {
        std::ofstream out(path / "config" / name);
        out << content;
    }
};

}  // namespace

TEST_CASE("loadTtsConfig defaults leave the game silent") {
    TempDir tmp;
    unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
    const TtsConfig cfg = loadTtsConfig(tmp.configDir());
    CHECK(cfg.provider == "none");
    CHECK(cfg.elevenLabsModel == "eleven_flash_v2_5");
    CHECK(cfg.elevenLabsOutputFormat == "pcm_22050");
    CHECK(cfg.elevenLabsBaseUrl == "https://api.elevenlabs.io");
    CHECK(cfg.elevenLabsTimeoutSeconds == 15);
    CHECK(cfg.elevenLabsRetryBackoffMs == 750);
    CHECK(cfg.characterBudget == 0);
    CHECK(cfg.apiKeyEnv == "NPC_GAME_ELEVENLABS_API_KEY");
    CHECK(cfg.apiKey.empty());
    CHECK(cfg.cacheMaxMb == 200);
    CHECK(cfg.cacheDir == tmp.path / "saves/tts_cache");
    CHECK(makeTtsBackend(cfg) == nullptr);
}

TEST_CASE("loadTtsConfig reads every key") {
    TempDir tmp;
    unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
    tmp.write("tts.cfg",
              "provider = elevenlabs\n"
              "elevenlabs_model = eleven_flash_v2\n"
              "elevenlabs_output_format = pcm_24000\n"
              "elevenlabs_base_url = http://127.0.0.1:1\n"
              "elevenlabs_timeout_s = 3\n"
              "elevenlabs_retry_backoff_ms = 5\n"
              "character_budget = 12345\n"
              "cache_dir = /abs/cache\n"
              "cache_max_mb = 7\n");
    tmp.write("secrets.cfg", "elevenlabs_api_key = test-key-never-real\n");
    const TtsConfig cfg = loadTtsConfig(tmp.configDir());
    CHECK(cfg.provider == "elevenlabs");
    CHECK(cfg.elevenLabsModel == "eleven_flash_v2");
    CHECK(cfg.elevenLabsOutputFormat == "pcm_24000");
    CHECK(cfg.elevenLabsBaseUrl == "http://127.0.0.1:1");
    CHECK(cfg.elevenLabsTimeoutSeconds == 3);
    CHECK(cfg.elevenLabsRetryBackoffMs == 5);
    CHECK(cfg.characterBudget == 12345);
    CHECK(cfg.cacheDir == fs::path("/abs/cache"));
    CHECK(cfg.cacheMaxMb == 7);
    CHECK(cfg.apiKey == "test-key-never-real");
}

TEST_CASE("garbage, negative and oversized numbers keep their defaults instead of throwing") {
    TempDir tmp;
    unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
    tmp.write("tts.cfg",
              "elevenlabs_timeout_s = abc\n"
              "elevenlabs_retry_backoff_ms = -5\n"
              "character_budget = 99999999999999999999999\n"
              "cache_max_mb = -1\n");
    TtsConfig cfg;
    REQUIRE_NOTHROW(cfg = loadTtsConfig(tmp.configDir()));
    CHECK(cfg.elevenLabsTimeoutSeconds == 15);
    CHECK(cfg.elevenLabsRetryBackoffMs == 750);
    CHECK(cfg.characterBudget == 0);
    CHECK(cfg.cacheMaxMb == 200);
    // Zero is a legal value, not garbage.
    tmp.write("tts.cfg", "character_budget = 0\ncache_max_mb = 0\n");
    CHECK(loadTtsConfig(tmp.configDir()).cacheMaxMb == 0);
}

TEST_CASE("whitespace around the env var key is trimmed, and a blank one falls back to the file") {
    TempDir tmp;
    tmp.write("tts.cfg", "provider = elevenlabs\n");
    tmp.write("secrets.cfg", "elevenlabs_api_key = file-key-never-real\n");
    setenv("NPC_GAME_ELEVENLABS_API_KEY", "  env-key-never-real\n", 1);
    CHECK(loadTtsConfig(tmp.configDir()).apiKey == "env-key-never-real");
    setenv("NPC_GAME_ELEVENLABS_API_KEY", " \n", 1);
    CHECK(loadTtsConfig(tmp.configDir()).apiKey == "file-key-never-real");
    unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
}

TEST_CASE("the env var wins over secrets.cfg, and the LLM api_key slot is never used") {
    TempDir tmp;
    tmp.write("tts.cfg", "provider = elevenlabs\n");
    tmp.write("secrets.cfg",
              "api_key = sk-ant-llm-key-never-real\n"
              "elevenlabs_api_key = file-key-never-real\n");
    SUBCASE("file key when the env var is unset") {
        unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
        CHECK(loadTtsConfig(tmp.configDir()).apiKey == "file-key-never-real");
    }
    SUBCASE("env var when set") {
        setenv("NPC_GAME_ELEVENLABS_API_KEY", "env-key-never-real", 1);
        CHECK(loadTtsConfig(tmp.configDir()).apiKey == "env-key-never-real");
        unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
    }
    SUBCASE("only the LLM slot present: no key") {
        unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
        tmp.write("secrets.cfg", "api_key = sk-ant-llm-key-never-real\n");
        CHECK(loadTtsConfig(tmp.configDir()).apiKey.empty());
    }
}

TEST_CASE("provider = elevenlabs with no key downgrades to none, so nothing is built") {
    TempDir tmp;
    unsetenv("NPC_GAME_ELEVENLABS_API_KEY");
    tmp.write("tts.cfg", "provider = elevenlabs\n");
    const TtsConfig cfg = loadTtsConfig(tmp.configDir());
    CHECK(cfg.provider == "none");
    CHECK(makeTtsBackend(cfg) == nullptr);
}

TEST_CASE("a custom api_key_env is honoured") {
    TempDir tmp;
    tmp.write("tts.cfg", "provider = elevenlabs\napi_key_env = LLM_NPC_TEST_TTS_KEY\n");
    setenv("LLM_NPC_TEST_TTS_KEY", "custom-env-key-never-real", 1);
    CHECK(loadTtsConfig(tmp.configDir()).apiKey == "custom-env-key-never-real");
    unsetenv("LLM_NPC_TEST_TTS_KEY");
}

TEST_CASE("makeTtsBackend builds ElevenLabs only for that provider, and logs unknown ones") {
    TtsConfig cfg;
    cfg.provider = "elevenlabs";
    cfg.apiKey = "test-key-never-real";
    CHECK(makeTtsBackend(cfg) != nullptr);
    cfg.provider = "kokoro";
    CHECK(makeTtsBackend(cfg) == nullptr);
}

TEST_CASE("loadVoiceMap keys entries by persona name, with defaults for backend and model") {
    TempDir tmp;
    tmp.write("voices.cfg",
              "# comment\n"
              "npc.Marge Holloway.voice_id = voiceA\n"
              "npc.Marge Holloway.model_id = eleven_flash_v2\n"
              "npc.Jim.voice_id = voiceB\n"
              "npc.Jim.backend = elevenlabs\n"
              "npc.Dotted.Name.voice_id = voiceC\n"
              "npc.NoVoice.model_id = eleven_flash_v2\n"
              "unrelated = 1\n");
    const VoiceMap voices = loadVoiceMap(tmp.configDir());
    REQUIRE(voices.size() == 3);
    CHECK(voices.at("Marge Holloway").voiceId == "voiceA");
    CHECK(voices.at("Marge Holloway").modelId == "eleven_flash_v2");
    CHECK(voices.at("Marge Holloway").backend == "elevenlabs");
    CHECK(voices.at("Jim").voiceId == "voiceB");
    CHECK(voices.at("Jim").modelId.empty());
    CHECK(voices.at("Dotted.Name").voiceId == "voiceC");
    CHECK(voices.count("NoVoice") == 0);
}

TEST_CASE("loadVoiceMap is empty when voices.cfg is missing") {
    TempDir tmp;
    CHECK(loadVoiceMap(tmp.configDir()).empty());
}

TEST_CASE("ttsCharacterCount counts code points, not bytes") {
    CHECK(ttsCharacterCount("") == 0);
    CHECK(ttsCharacterCount("abc") == 3);
    CHECK(ttsCharacterCount("caf\xC3\xA9") == 4);      // é is two bytes
    CHECK(ttsCharacterCount("\xE2\x80\x94") == 1);     // em dash, three bytes
}
