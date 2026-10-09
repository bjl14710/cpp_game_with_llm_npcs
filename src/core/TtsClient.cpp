#include "TtsClient.hpp"

#include <exception>
#include <iostream>
#include <utility>

#include "WavEncode.hpp"

namespace llm_npc {

TtsClient::TtsClient(TtsConfig config, VoiceMap voices, std::unique_ptr<TtsBackend> backend)
    : config_(std::move(config)), voices_(std::move(voices)), backend_(std::move(backend)) {
    if (!backend_) return;
    cache_ = std::make_unique<TtsCache>(config_.cacheDir, config_.cacheMaxMb * 1024ULL * 1024ULL);
    worker_ = std::thread(&TtsClient::workerLoop, this);
}

TtsClient::~TtsClient() {
    {
        // Set under the queue mutex so the store+notify cannot land between the
        // worker's predicate check and its wait (see LlmClient::~LlmClient).
        std::lock_guard<std::mutex> lock(requestMutex_);
        stop_.store(true);
    }
    requestCv_.notify_all();
    // Wake a worker sitting in a retry backoff so the join is prompt.
    if (backend_) backend_->requestStop();
    if (worker_.joinable()) worker_.join();
    if (!backend_) return;
    const TtsUsage u = backend_->usage();
    if (u.requests == 0 && cacheHits_.load() == 0) return;
    std::cerr << "[llm_npc] tts: session usage: requests=" << u.requests
              << " characters=" << u.charactersSent << " cache_hits=" << cacheHits_.load() << "\n";
}

std::uint64_t TtsClient::submit(const std::string& npcId, const std::string& text) {
    if (!backend_ || text.empty()) return 0;
    const auto it = voices_.find(npcId);
    if (it == voices_.end() || it->second.backend != config_.provider) return 0;

    Request req;
    req.id = nextId_.fetch_add(1);
    req.text = text;
    req.voice.npcId = npcId;
    req.voice.voiceId = it->second.voiceId;
    req.voice.modelId = it->second.modelId.empty() ? config_.elevenLabsModel : it->second.modelId;
    const std::uint64_t id = req.id;
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        requests_.push(std::move(req));
    }
    requestCv_.notify_one();
    return id;
}

std::vector<SpeechResult> TtsClient::drainReady() {
    std::vector<SpeechResult> out;
    std::lock_guard<std::mutex> lock(resultMutex_);
    out.swap(results_);
    return out;
}

void TtsClient::workerLoop() {
    while (true) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait(lock, [this] { return stop_.load() || !requests_.empty(); });
            // Unlike the LLM queue, pending lines are dropped on shutdown: a
            // network round trip per queued line would stall the quit.
            if (stop_.load()) return;
            req = std::move(requests_.front());
            requests_.pop();
        }
        SpeechResult result;
        try {
            result = processOne(req);
        } catch (const std::exception& e) {
            // A backend promises not to throw; if one does, that line is
            // silent and the worker survives. Said once per session.
            result = SpeechResult{};
            result.id = req.id;
            result.npcId = req.voice.npcId;
            result.error = std::string("synthesis threw: ") + e.what();
            if (!loggedWorkerException_) {
                loggedWorkerException_ = true;
                std::cerr << "[llm_npc] tts: synthesis threw (" << e.what()
                          << "); this line is silent\n";
            }
        }
        std::lock_guard<std::mutex> lock(resultMutex_);
        results_.push_back(std::move(result));
    }
}

SpeechResult TtsClient::processOne(const Request& req) {
    SpeechResult result;
    result.id = req.id;
    result.npcId = req.voice.npcId;

    const std::string key = TtsCache::key(req.text, req.voice.voiceId, req.voice.modelId,
                                          config_.elevenLabsOutputFormat);
    if (auto hit = cache_->get(key)) {
        if (parseWavHeader(*hit)) {
            cacheHits_.fetch_add(1);
            std::cerr << "[llm_npc] tts: cache hit for " << req.voice.npcId << " (" << key << ")\n";
            result.ok = true;
            result.cacheHit = true;
            result.wav = std::move(*hit);
            return result;
        }
        // A truncated or foreign file in the slot: never hand it to the audio
        // device; drop it and synthesize afresh.
        std::cerr << "[llm_npc] tts: cache entry " << key << " is not a WAV file; replacing it\n";
        cache_->remove(key);
    }

    TtsResult synthesized = backend_->synthesize(req.text, req.voice);
    result.ok = synthesized.ok;
    result.error = synthesized.error;
    if (synthesized.ok) {
        cache_->put(key, synthesized.wav);
        result.wav = std::move(synthesized.wav);
    }
    return result;
}

}  // namespace llm_npc
