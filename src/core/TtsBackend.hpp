#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "TtsConfig.hpp"

namespace llm_npc {

// The voice one line should be spoken in. `npcId` travels only so a failure
// can be logged against the character it belongs to.
struct TtsVoice {
    std::string npcId;
    std::string voiceId;
    std::string modelId;
};

// Outcome of one synthesis: a complete WAV file (16-bit PCM, mono, at the
// configured sample rate) in `wav` on success, or a human-readable `error`.
// Every failure is a decline: the caller's fallback is silence, which is what
// the game does for every NPC without a voice.
struct TtsResult {
    bool ok = false;
    std::string wav;
    std::string error;
};

// Characters billed for `text`: UTF-8 code points, not bytes.
std::uint64_t ttsCharacterCount(const std::string& text);

// Characters sent to the provider over the session. Cache hits never reach a
// backend and so are never counted here.
struct TtsUsage {
    std::uint64_t requests = 0;
    std::uint64_t charactersSent = 0;
};

// A provider that turns text into a WAV buffer. Implementations own the HTTP
// and wire-format details; TtsClient owns the worker thread, the cache and the
// result queue, and calls synthesize() from the worker.
class TtsBackend {
   public:
    virtual ~TtsBackend() = default;

    // Synthesizes one line. Never throws; declines with ok=false.
    virtual TtsResult synthesize(const std::string& text, const TtsVoice& voice) = 0;

    // Sample rate of the WAV buffers this backend produces.
    virtual std::uint32_t sampleRate() const = 0;

    virtual TtsUsage usage() const = 0;

    // True once a 401, 402 or the character budget has silenced the session.
    virtual bool disabled() const = 0;

    // Asks a synthesize() in flight on another thread to give up early: wake
    // from any retry backoff and skip the retry. Called by TtsClient before it
    // joins its worker. The default does nothing.
    virtual void requestStop() {}
};

// Builds the backend named by config.provider, or nullptr for "none" (the
// default) and for any unknown provider, which is logged once. A null backend
// means the game never opens an audio device: byte for byte today's behaviour.
std::unique_ptr<TtsBackend> makeTtsBackend(const TtsConfig& config);

}  // namespace llm_npc
