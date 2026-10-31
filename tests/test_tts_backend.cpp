// The ElevenLabs TTS backend (plan: elevenlabs-tts-backend): request shape,
// every row of the error-handling table, the WAV it hands back, the character
// budget, and the cache in front of it via TtsClient.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "TtsCache.hpp"

#include "ElevenLabsBackend.hpp"
#include "FakeElevenLabs.hpp"
#include "TtsBackend.hpp"
#include "TtsClient.hpp"
#include "WavEncode.hpp"
#include "doctest.h"
#include "json.hpp"

using namespace llm_npc;
using llm_npc_test::FakeElevenLabs;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

TtsConfig ttsConfig(const FakeElevenLabs& fake) {
    TtsConfig config;
    config.provider = "elevenlabs";
    config.apiKey = "test-key-never-real";
    config.elevenLabsBaseUrl = fake.baseUrl();
    config.elevenLabsRetryBackoffMs = 1;
    config.elevenLabsTimeoutSeconds = 5;
    config.cacheDir = fs::temp_directory_path() / ("llm_npc_tts_" + std::to_string(std::rand()));
    config.cacheMaxMb = 1;
    return config;
}

TtsVoice margeVoice() {
    TtsVoice voice;
    voice.npcId = "Marge Holloway";
    voice.voiceId = "voiceA";
    voice.modelId = "eleven_flash_v2_5";
    return voice;
}

struct CacheDirGuard {
    fs::path path;
    ~CacheDirGuard() { fs::remove_all(path); }
};

// Redirects std::cerr into a buffer for the lifetime of the object, so a test
// can count log lines.
struct CerrCapture {
    std::ostringstream buffer;
    std::streambuf* saved;
    CerrCapture() : saved(std::cerr.rdbuf(buffer.rdbuf())) {}
    ~CerrCapture() { std::cerr.rdbuf(saved); }
    std::size_t count(const std::string& needle) const {
        const std::string text = buffer.str();
        std::size_t n = 0;
        for (auto at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++n;
        return n;
    }
};

// A backend that breaks its never-throws promise, to prove the worker survives.
struct ThrowingBackend : TtsBackend {
    TtsResult synthesize(const std::string&, const TtsVoice&) override {
        throw std::runtime_error("boom");
    }
    std::uint32_t sampleRate() const override { return 22050; }
    TtsUsage usage() const override { return {}; }
    bool disabled() const override { return false; }
};

SpeechResult waitForResult(TtsClient& client, std::uint64_t id,
                           std::chrono::seconds timeout = std::chrono::seconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        for (auto& result : client.drainReady()) {
            if (result.id == id) return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    SpeechResult timedOut;
    timedOut.id = id;
    timedOut.error = "test timed out waiting for audio";
    return timedOut;
}

}  // namespace

// --- Request shape --------------------------------------------------------------

TEST_CASE("the outbound request is the convert endpoint with the key header, model and format") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));

    const TtsResult result = backend.synthesize("Any bread left?", margeVoice());
    REQUIRE(result.ok);
    CHECK(result.error.empty());

    const auto seen = fake.requests();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].voiceId == "voiceA");
    CHECK(seen[0].outputFormat == "pcm_22050");
    CHECK(seen[0].headers.at("xi-api-key") == "test-key-never-real");
    CHECK(seen[0].headers.at("content-type") == "application/json");
    const json body = json::parse(seen[0].body);
    CHECK(body["text"] == "Any bread left?");
    CHECK(body["model_id"] == "eleven_flash_v2_5");
    CHECK(body.size() == 2);  // no voice_settings or other fields we did not verify
}

TEST_CASE("an empty voice model id falls back to the configured model") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsModel = "eleven_flash_v2";
    ElevenLabsBackend backend(config);
    TtsVoice voice = margeVoice();
    voice.modelId.clear();
    REQUIRE(backend.synthesize("hi", voice).ok);
    CHECK(json::parse(fake.requests().at(0).body)["model_id"] == "eleven_flash_v2");
}

TEST_CASE("a line with a truncated UTF-8 sequence is still sent, not thrown") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    TtsResult result;
    REQUIRE_NOTHROW(result = backend.synthesize("caf\xC3 au lait", margeVoice()));
    CHECK(result.ok);
    REQUIRE(fake.requests().size() == 1);
    const json body = json::parse(fake.requests().at(0).body);
    CHECK(body["text"].get<std::string>().find("caf") == 0);
    CHECK(body["model_id"] == "eleven_flash_v2_5");
}

TEST_CASE("a malformed voice id never reaches the URL and silences that NPC once") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    TtsVoice voice = margeVoice();
    voice.voiceId = "../other?x=1";
    CerrCapture log;
    CHECK_FALSE(backend.synthesize("hi", voice).ok);
    CHECK_FALSE(backend.synthesize("hi again", voice).ok);
    CHECK(fake.requests().empty());
    CHECK(log.count("is not [A-Za-z0-9_-]+") == 1);
    CHECK_FALSE(backend.disabled());
    // Underscores and dashes are fine (for an NPC not already silenced).
    voice.npcId = "Jim";
    voice.voiceId = "Voice_01-b";
    CHECK(backend.synthesize("hi", voice).ok);
    CHECK(fake.requests().size() == 1);
}

// --- The audio that comes back ---------------------------------------------------

TEST_CASE("raw PCM is returned as a 16-bit mono WAV at the configured sample rate") {
    FakeElevenLabs fake;
    fake.setSampleCount(2205);
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsOutputFormat = "pcm_24000";
    ElevenLabsBackend backend(config);
    CHECK(backend.sampleRate() == 24000);

    const TtsResult result = backend.synthesize("hello", margeVoice());
    REQUIRE(result.ok);
    const auto info = parseWavHeader(result.wav);
    REQUIRE(info.has_value());
    CHECK(info->sampleRate == 24000);
    CHECK(info->channels == 1);
    CHECK(info->bitsPerSample == 16);
    CHECK(info->dataBytes == 2205 * 2);
    CHECK(result.wav.size() == 44 + 2205 * 2);
    CHECK(result.wav.substr(44) == FakeElevenLabs::pcmOf(2205));
}

TEST_CASE("a wav_* format passes the provider's file through unchanged") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::OkWav);
    fake.setSampleCount(10);
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsOutputFormat = "wav_8000";
    ElevenLabsBackend backend(config);
    const TtsResult result = backend.synthesize("hello", margeVoice());
    REQUIRE(result.ok);
    CHECK(fake.requests().at(0).outputFormat == "wav_8000");
    const auto info = parseWavHeader(result.wav);
    REQUIRE(info.has_value());
    CHECK(info->sampleRate == 8000);
    CHECK(info->dataBytes == 20);
}

TEST_CASE("a format nothing here can decode is replaced by pcm_22050 at construction") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsOutputFormat = "mp3_44100_128";
    ElevenLabsBackend backend(config);
    CHECK(backend.sampleRate() == 22050);
    REQUIRE(backend.synthesize("hello", margeVoice()).ok);
    CHECK(fake.requests().at(0).outputFormat == "pcm_22050");
}

TEST_CASE("elevenLabsSampleRate reads the rate out of pcm_* and wav_* and rejects the rest") {
    CHECK(elevenLabsSampleRate("pcm_22050") == 22050);
    CHECK(elevenLabsSampleRate("pcm_44100") == 44100);
    CHECK(elevenLabsSampleRate("wav_16000") == 16000);
    CHECK(elevenLabsSampleRate("mp3_44100_128") == 0);
    CHECK(elevenLabsSampleRate("opus_48000_64") == 0);
    CHECK(elevenLabsSampleRate("ulaw_8000") == 0);
    CHECK(elevenLabsSampleRate("pcm_") == 0);
    CHECK(elevenLabsSampleRate("pcm_abc") == 0);
}

TEST_CASE("a 200 with no bytes declines rather than playing nothing") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Empty200);
    ElevenLabsBackend backend(ttsConfig(fake));
    const TtsResult result = backend.synthesize("hello", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("no audio") != std::string::npos);
}

// --- The error-handling table, as a pure function -------------------------------

TEST_CASE("classifyTtsExchange pins every row of the error-handling table") {
    const std::string quota = R"({"detail":{"code":"quota_exceeded"}})";
    const std::string rate = R"({"detail":{"code":"rate_limit_exceeded"}})";
    CHECK(classifyTtsExchange(false, 0, "") == ElevenLabsOutcome::Retry);  // timeout / no network
    CHECK(classifyTtsExchange(true, 200, "") == ElevenLabsOutcome::Deliver);
    CHECK(classifyTtsExchange(true, 401, "") == ElevenLabsOutcome::Disable);
    CHECK(classifyTtsExchange(true, 402, "") == ElevenLabsOutcome::Disable);
    CHECK(classifyTtsExchange(true, 404, "") == ElevenLabsOutcome::UnknownVoice);
    CHECK(classifyTtsExchange(true, 429, rate) == ElevenLabsOutcome::Retry);
    CHECK(classifyTtsExchange(true, 429, "") == ElevenLabsOutcome::Retry);
    CHECK(classifyTtsExchange(true, 429, quota) == ElevenLabsOutcome::Disable);
    CHECK(classifyTtsExchange(true, 500, "") == ElevenLabsOutcome::Retry);
    CHECK(classifyTtsExchange(true, 503, "") == ElevenLabsOutcome::Retry);
    CHECK(classifyTtsExchange(true, 400, "") == ElevenLabsOutcome::Fallback);
    CHECK(classifyTtsExchange(true, 403, "") == ElevenLabsOutcome::Fallback);
    CHECK(classifyTtsExchange(true, 422, "") == ElevenLabsOutcome::Fallback);
}

TEST_CASE("elevenLabsCharacterLimit follows the models page and is conservative for unknowns") {
    CHECK(elevenLabsCharacterLimit("eleven_flash_v2_5") == 40000);
    CHECK(elevenLabsCharacterLimit("eleven_flash_v2") == 30000);
    CHECK(elevenLabsCharacterLimit("eleven_multilingual_v2") == 10000);
    CHECK(elevenLabsCharacterLimit("eleven_v3") == 5000);
    CHECK(elevenLabsCharacterLimit("some_future_model") == 5000);
}

// --- Each outcome through the real backend --------------------------------------

TEST_CASE("HTTP 401 disables the backend for the rest of the session") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http401);
    ElevenLabsBackend backend(ttsConfig(fake));

    const TtsResult first = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(first.ok);
    CHECK(first.wav.empty());
    CHECK(backend.disabled());

    const TtsResult second = backend.synthesize("hi again", margeVoice());
    CHECK_FALSE(second.ok);
    CHECK(fake.requests().size() == 1);  // the second never went out
    CHECK(backend.usage().requests == 0);  // a rejected call is not a billed one
}

TEST_CASE("HTTP 402 (out of credit) disables the backend for the session") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http402);
    ElevenLabsBackend backend(ttsConfig(fake));
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    CHECK(backend.disabled());
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    CHECK(fake.requests().size() == 1);
}

TEST_CASE("a 429 whose code names the quota disables rather than retries") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http429Quota);
    ElevenLabsBackend backend(ttsConfig(fake));
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    CHECK(backend.disabled());
    CHECK(fake.requests().size() == 1);
}

TEST_CASE("a 429 rate limit is retried once and then the line is silent") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http429);
    ElevenLabsBackend backend(ttsConfig(fake));
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(fake.requests().size() == 2);
    CHECK_FALSE(backend.disabled());  // per line, not per session
}

TEST_CASE("a 500 is retried once and then the line is silent") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http500);
    ElevenLabsBackend backend(ttsConfig(fake));
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("HTTP 500") != std::string::npos);
    CHECK(fake.requests().size() == 2);
    CHECK_FALSE(backend.disabled());
}

TEST_CASE("a 500 followed by a 200 delivers the audio") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::FailOnceThenOk);
    ElevenLabsBackend backend(ttsConfig(fake));
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK(result.ok);
    CHECK(fake.requests().size() == 2);
    CHECK(backend.usage().requests == 1);  // only the successful exchange is billed
}

TEST_CASE("an unknown voice silences that NPC for the session and logs it once") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::VoiceNotFound);
    ElevenLabsBackend backend(ttsConfig(fake));

    const TtsResult first = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(first.ok);
    CHECK(first.error.find("Marge Holloway") != std::string::npos);
    CHECK_FALSE(backend.disabled());  // other NPCs keep their voices

    const TtsResult second = backend.synthesize("hi again", margeVoice());
    CHECK_FALSE(second.ok);
    CHECK(fake.requests().size() == 1);  // no second request for that NPC

    // Another NPC whose voice the fake also rejects still gets its own request.
    TtsVoice jim = margeVoice();
    jim.npcId = "Jim";
    jim.voiceId = "voiceB";
    CHECK_FALSE(backend.synthesize("hi", jim).ok);
    CHECK(fake.requests().size() == 2);
}

TEST_CASE("a line over the model's character limit is silent without a request") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    TtsVoice voice = margeVoice();
    voice.modelId = "eleven_v3";  // 5,000-character limit
    const TtsResult result = backend.synthesize(std::string(5001, 'a'), voice);
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("exceeds") != std::string::npos);
    CHECK(fake.requests().empty());
    CHECK_FALSE(backend.disabled());
    // Exactly at the limit still goes out.
    CHECK(backend.synthesize(std::string(5000, 'a'), voice).ok);
    CHECK(fake.requests().size() == 1);
}

TEST_CASE("an empty line is silent without a request") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    CHECK_FALSE(backend.synthesize("", margeVoice()).ok);
    CHECK(fake.requests().empty());
}

TEST_CASE("an unreachable provider is retried once and then the line is silent") {
    int deadPort = 0;
    {
        FakeElevenLabs probe;  // grab a free port, then shut it down
        deadPort = probe.port();
    }
    FakeElevenLabs unused;
    TtsConfig config = ttsConfig(unused);
    config.elevenLabsBaseUrl = "http://127.0.0.1:" + std::to_string(deadPort);
    ElevenLabsBackend backend(config);
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("could not reach") != std::string::npos);
    CHECK_FALSE(backend.disabled());
}

TEST_CASE("a 400 (e.g. text_too_long from the provider) is silent with no retry") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http400);
    ElevenLabsBackend backend(ttsConfig(fake));
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("text_too_long") != std::string::npos);
    CHECK(fake.requests().size() == 1);
}

TEST_CASE("plain http to anything but the machine itself is refused") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsBaseUrl = "http://example.invalid:" + std::to_string(fake.port());
    ElevenLabsBackend backend(config);
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("plain http") != std::string::npos);
    CHECK(fake.requests().empty());
}

TEST_CASE("an https host that is not ElevenLabs or loopback is refused without a request") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    for (const char* url : {"https://example.com", "https://api.elevenlabs.io.evil.test",
                            "https://notelevenlabs.io", "http://127.0.0.1.evil.test"}) {
        config.elevenLabsBaseUrl = url;
        ElevenLabsBackend backend(config);
        const TtsResult result = backend.synthesize("hi", margeVoice());
        CHECK_FALSE(result.ok);
        CHECK(result.error.find("refusing to send the key") != std::string::npos);
        CHECK_FALSE(backend.disabled());
    }
    CHECK(fake.requests().empty());
}

TEST_CASE("HTTP 401 is logged once across repeated requests") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http401);
    ElevenLabsBackend backend(ttsConfig(fake));
    CerrCapture log;
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    CHECK_FALSE(backend.synthesize("hi again", margeVoice()).ok);
    CHECK(log.count("HTTP 401") == 1);
    CHECK(log.count("test-key-never-real") == 0);
}

TEST_CASE("a transient failure after a success is logged again") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    CerrCapture log;
    fake.setMode(FakeElevenLabs::Mode::Http400);
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    fake.setMode(FakeElevenLabs::Mode::Ok);
    CHECK(backend.synthesize("hi", margeVoice()).ok);
    fake.setMode(FakeElevenLabs::Mode::Http400);
    CHECK_FALSE(backend.synthesize("hi", margeVoice()).ok);
    CHECK(log.count("HTTP 400") == 2);
}

TEST_CASE("the key never appears in an error string, whatever the server echoes") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    fake.setEchoInErrors(config.apiKey);
    for (const auto mode : {FakeElevenLabs::Mode::Http401, FakeElevenLabs::Mode::Http400,
                            FakeElevenLabs::Mode::Http500, FakeElevenLabs::Mode::VoiceNotFound}) {
        fake.setMode(mode);
        ElevenLabsBackend backend(config);
        const TtsResult result = backend.synthesize("hi", margeVoice());
        CHECK_FALSE(result.ok);
        CHECK(result.error.find(config.apiKey) == std::string::npos);
    }
}

// --- Cost visibility -----------------------------------------------------------

TEST_CASE("usage counts code points of every successful request") {
    FakeElevenLabs fake;
    ElevenLabsBackend backend(ttsConfig(fake));
    REQUIRE(backend.synthesize("hello", margeVoice()).ok);           // 5
    REQUIRE(backend.synthesize("caf\xC3\xA9 time", margeVoice()).ok);  // 9 code points, 10 bytes
    const TtsUsage u = backend.usage();
    CHECK(u.requests == 2);
    CHECK(u.charactersSent == 14);
}

TEST_CASE("the character budget silences the session once it would be exceeded") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.characterBudget = 10;
    ElevenLabsBackend backend(config);

    CHECK(backend.synthesize("12345", margeVoice()).ok);  // 5 of 10
    CHECK_FALSE(backend.disabled());
    CHECK_FALSE(backend.synthesize("1234567", margeVoice()).ok);  // 12 > 10: not sent
    CHECK(backend.disabled());
    CHECK(fake.requests().size() == 1);
}

TEST_CASE("reaching the budget exactly disables after the line that reaches it") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.characterBudget = 5;
    ElevenLabsBackend backend(config);
    CHECK(backend.synthesize("12345", margeVoice()).ok);
    CHECK(backend.disabled());
    CHECK_FALSE(backend.synthesize("x", margeVoice()).ok);
    CHECK(fake.requests().size() == 1);
}

// --- Through TtsClient: cache, voice map, worker --------------------------------

TEST_CASE("a repeated line is a cache hit: no API call, identical bytes, not billed") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    REQUIRE(client.enabled());

    const SpeechResult first = waitForResult(client, client.submit("Marge Holloway", "Fresh loaves!"));
    REQUIRE(first.ok);
    CHECK_FALSE(first.cacheHit);
    CHECK(first.npcId == "Marge Holloway");
    REQUIRE(parseWavHeader(first.wav).has_value());

    const SpeechResult second =
        waitForResult(client, client.submit("Marge Holloway", "Fresh loaves!"));
    REQUIRE(second.ok);
    CHECK(second.cacheHit);
    CHECK(second.wav == first.wav);

    CHECK(fake.requests().size() == 1);
    CHECK(client.cacheHits() == 1);
    CHECK(client.backend()->usage().requests == 1);
    CHECK(client.backend()->usage().charactersSent == 13);
}

TEST_CASE("the cache never contains the key") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    REQUIRE(waitForResult(client, client.submit("Marge Holloway", "Fresh loaves!")).ok);

    std::size_t files = 0;
    for (const auto& entry : fs::directory_iterator(config.cacheDir)) {
        ++files;
        std::ifstream in(entry.path(), std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(bytes.find(config.apiKey) == std::string::npos);
        CHECK(entry.path().string().find(config.apiKey) == std::string::npos);
    }
    CHECK(files == 1);
}

TEST_CASE("a corrupt cache entry is replaced by a fresh synthesis, not played") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    const std::string key =
        TtsCache::key("Fresh loaves!", "voiceA", config.elevenLabsModel, config.elevenLabsOutputFormat);
    {
        TtsCache cache(config.cacheDir, 1024 * 1024);
        REQUIRE(cache.put(key, "<html>definitely not audio</html>" + std::string(40, ' ')));
    }
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    const SpeechResult result = waitForResult(client, client.submit("Marge Holloway", "Fresh loaves!"));
    REQUIRE(result.ok);
    CHECK_FALSE(result.cacheHit);
    CHECK(parseWavHeader(result.wav).has_value());
    CHECK(fake.requests().size() == 1);
    CHECK(client.cacheHits() == 0);
    // The slot now holds the real audio.
    TtsCache cache(config.cacheDir, 1024 * 1024);
    const auto stored = cache.get(key);
    REQUIRE(stored.has_value());
    CHECK(*stored == result.wav);
}

TEST_CASE("a backend that throws yields a silent result and leaves the worker alive") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ThrowingBackend>());
    CerrCapture log;
    const SpeechResult first = waitForResult(client, client.submit("Marge Holloway", "hello"));
    CHECK_FALSE(first.ok);
    CHECK(first.error.find("boom") != std::string::npos);
    const SpeechResult second = waitForResult(client, client.submit("Marge Holloway", "again"));
    CHECK_FALSE(second.ok);
    CHECK(second.npcId == "Marge Holloway");
    CHECK(log.count("synthesis threw") == 1);
}

TEST_CASE("destroying the client during a retry backoff does not wait out the pause") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http500);
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsRetryBackoffMs = 30000;
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    const auto start = std::chrono::steady_clock::now();
    {
        TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
        REQUIRE(client.submit("Marge Holloway", "hello") != 0);
        // Let the worker reach the backoff before tearing down.
        const auto deadline = start + std::chrono::seconds(5);
        while (fake.requests().empty() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        REQUIRE(fake.requests().size() == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
    CHECK(fake.requests().size() == 1);  // the retry was skipped
}

TEST_CASE("NPCs without a voices.cfg entry are never queued") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    CHECK(client.submit("Jim", "hello") == 0);
    CHECK(client.submit("Marge Holloway", "") == 0);
    CHECK(client.submit("Marge Holloway", "hello") != 0);
}

TEST_CASE("a voice entry for another backend is treated as no voice") {
    FakeElevenLabs fake;
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"kokoro", "af_heart", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    CHECK(client.submit("Marge Holloway", "hello") == 0);
}

TEST_CASE("a client with no backend is disabled and costs nothing") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.provider = "none";
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, nullptr);
    CHECK_FALSE(client.enabled());
    CHECK(client.submit("Marge Holloway", "hello") == 0);
    CHECK(client.drainReady().empty());
    CHECK_FALSE(fs::exists(config.cacheDir));
}

TEST_CASE("a failed synthesis surfaces as a silent result, not a crash") {
    FakeElevenLabs fake;
    fake.setMode(FakeElevenLabs::Mode::Http500);
    const TtsConfig config = ttsConfig(fake);
    CacheDirGuard guard{config.cacheDir};
    VoiceMap voices;
    voices["Marge Holloway"] = VoiceEntry{"elevenlabs", "voiceA", ""};
    TtsClient client(config, voices, std::make_unique<ElevenLabsBackend>(config));
    const SpeechResult result = waitForResult(client, client.submit("Marge Holloway", "hello"));
    CHECK_FALSE(result.ok);
    CHECK(result.wav.empty());
    CHECK_FALSE(result.error.empty());
    CHECK(fs::is_empty(config.cacheDir));  // failures are never cached
}

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
TEST_CASE("an https base URL in a build without OpenSSL is silent without a request") {
    FakeElevenLabs fake;
    TtsConfig config = ttsConfig(fake);
    config.elevenLabsBaseUrl = "https://api.elevenlabs.io";
    ElevenLabsBackend backend(config);
    const TtsResult result = backend.synthesize("hi", margeVoice());
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("OpenSSL") != std::string::npos);
    CHECK(fake.requests().empty());
}
#endif
