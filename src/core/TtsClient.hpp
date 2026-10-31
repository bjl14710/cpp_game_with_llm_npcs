#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <thread>

#include "TtsBackend.hpp"
#include "TtsCache.hpp"
#include "TtsConfig.hpp"

namespace llm_npc {

// One finished line pulled off the queue on the main thread. `wav` is a complete
// WAV file (16-bit PCM mono) when ok; silence otherwise.
struct SpeechResult {
    std::uint64_t id = 0;
    std::string npcId;
    bool ok = false;
    bool cacheHit = false;
    std::string wav;
    std::string error;
};

// Non-blocking TTS front door, built the way LlmClient is: submit() from the
// main thread, one worker thread synthesizes or serves the cache, drainReady()
// each frame hands back finished audio. Owns the backend and the cache. Only
// the host constructs one with a backend, so every API call is the host's.
class TtsClient {
   public:
    // `backend` may be null, in which case nothing is ever queued and
    // drainReady() is always empty: the game stays silent at zero cost.
    TtsClient(TtsConfig config, VoiceMap voices, std::unique_ptr<TtsBackend> backend);
    // Joins the worker and prints the session's one-line usage summary.
    ~TtsClient();

    TtsClient(const TtsClient&) = delete;
    TtsClient& operator=(const TtsClient&) = delete;

    // True when a backend is installed and at least one NPC has a voice.
    bool enabled() const { return backend_ != nullptr && !voices_.empty(); }

    // Queues `text` in `npcId`'s configured voice. Returns 0 (nothing queued)
    // when the client is disabled, the NPC has no voices.cfg entry, or the
    // text is empty; otherwise an id that drainReady() echoes back.
    std::uint64_t submit(const std::string& npcId, const std::string& text);

    // Pop every result that finished since the last call. Non-blocking.
    std::vector<SpeechResult> drainReady();

    // Lines served from disk this session (never sent to the API).
    std::uint64_t cacheHits() const { return cacheHits_.load(); }

    // The settings this client was built with (cache dir, provider, budget).
    const TtsConfig& config() const { return config_; }
    // The installed backend, for usage() and disabled(); null when provider is
    // "none".
    const TtsBackend* backend() const { return backend_.get(); }

   private:
    struct Request {
        std::uint64_t id = 0;
        std::string text;
        TtsVoice voice;
    };

    void workerLoop();
    SpeechResult processOne(const Request& req);

    TtsConfig config_;
    VoiceMap voices_;
    std::unique_ptr<TtsBackend> backend_;
    std::unique_ptr<TtsCache> cache_;

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> cacheHits_{0};
    // Worker-thread only: a backend that throws is reported once, not per line.
    bool loggedWorkerException_ = false;

    std::mutex requestMutex_;
    std::condition_variable requestCv_;
    std::queue<Request> requests_;

    std::mutex resultMutex_;
    std::vector<SpeechResult> results_;

    std::atomic<std::uint64_t> nextId_{1};
};

}  // namespace llm_npc
