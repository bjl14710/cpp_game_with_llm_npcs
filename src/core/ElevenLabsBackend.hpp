#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>

#include "BaseUrl.hpp"
#include "TtsBackend.hpp"
#include "TtsConfig.hpp"

namespace llm_npc {

// What one HTTP exchange with the ElevenLabs convert endpoint means for the
// line it carried. This is the ERROR HANDLING table of plan
// elevenlabs-tts-backend as a type, so a test can pin every row offline.
enum class ElevenLabsOutcome {
    Deliver,       // 200 with audio bytes
    Retry,         // 429 rate limit, 5xx, timeout, unreachable: once more, then silence
    Fallback,      // any other failure: this line is silent
    UnknownVoice,  // 404 voice_not_found: this NPC is silent for the session, logged once
    Disable,       // 401 / 402 / quota: the backend is off for the session
};

// `reached` is false when no HTTP status came back at all. `body` is only
// consulted for a 429 (a quota code there disables rather than retries) and a
// 404 (anything 404 on this endpoint is an unknown voice).
ElevenLabsOutcome classifyTtsExchange(bool reached, int status, const std::string& body);

// Per-request character limit of a model, from elevenlabs.io/docs/models.
// Unknown model ids get the smallest documented limit (eleven_v3's 5,000) so a
// typo cannot send a line the provider would reject anyway.
std::uint64_t elevenLabsCharacterLimit(const std::string& modelId);

// Sample rate encoded in an `output_format` value such as "pcm_22050" or
// "wav_24000"; 0 for a format that is not raw PCM or WAV (mp3, opus, ulaw...),
// which this backend refuses to request because nothing here can decode it.
std::uint32_t elevenLabsSampleRate(const std::string& outputFormat);

// ElevenLabs text-to-speech, one POST per line
// (POST /v1/text-to-speech/{voice_id}?output_format=..., header xi-api-key).
// Raw PCM replies are wrapped in a WAV header; wav_* replies pass through.
// Declines (TtsResult::ok = false) rather than fails: the fallback for every
// failure is silence, which is what an NPC without a voice gets anyway.
class ElevenLabsBackend : public TtsBackend {
   public:
    explicit ElevenLabsBackend(TtsConfig config);

    TtsResult synthesize(const std::string& text, const TtsVoice& voice) override;
    std::uint32_t sampleRate() const override { return sampleRate_; }
    TtsUsage usage() const override;
    bool disabled() const override;
    void requestStop() override;

   private:
    struct Exchange {
        bool reached = false;
        int status = 0;
        std::string body;
        std::string transportError;
    };

    Exchange send(const ParsedUrl& url, const TtsVoice& voice, const std::string& body) const;
    // A silent result. Logs `why` once per distinct reason so a provider that is
    // down for an hour prints one line, not one per NPC line.
    TtsResult decline(const std::string& why) const;
    void disable(const std::string& why);
    void recordUsage(std::uint64_t characters);

    TtsConfig config_;
    std::uint32_t sampleRate_ = 0;
    // Guards everything below: synthesize() runs on the worker thread while
    // usage() and disabled() are read from the main thread.
    mutable std::mutex mutex_;
    TtsUsage usage_;
    bool disabled_ = false;
    mutable std::string lastDeclineReason_;
    // NPC ids whose voice the API did not know; logged once each, then skipped
    // without a request.
    std::set<std::string> unknownVoiceNpcs_;
    bool loggedTls_ = false;
    // Set by requestStop(); the retry backoff waits on stopCv_ so a shutdown
    // never sits out the full pause.
    bool stopRequested_ = false;
    std::condition_variable stopCv_;
};

}  // namespace llm_npc
