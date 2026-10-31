#include "ElevenLabsBackend.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <utility>

#include "WavEncode.hpp"
#include "httplib.h"
#include "json.hpp"

namespace llm_npc {

using nlohmann::json;

namespace {

// Endpoint and header per elevenlabs.io/docs/api-reference/text-to-speech/convert
// and elevenlabs.io/docs/api-reference/authentication (fetched 2026-10-09).
constexpr const char* kConvertPath = "/v1/text-to-speech/";
constexpr const char* kLog = "[llm_npc] tts: ";

// Error bodies are short JSON, but a proxy can hand back a whole HTML page;
// keep the log to one readable line and mask the key should anything echo it.
std::string clipForLog(std::string text, const std::string& apiKey) {
    if (!apiKey.empty()) {
        for (std::size_t at = text.find(apiKey); at != std::string::npos;
             at = text.find(apiKey, at + 3)) {
            text.replace(at, apiKey.size(), "***");
        }
    }
    constexpr std::size_t kMax = 240;
    if (text.size() <= kMax) return text;
    return text.substr(0, kMax) + "...";
}

// The lower-cased host of an origin such as "https://api.elevenlabs.io:443"
// or "http://[::1]:8080": scheme and port stripped, IPv6 brackets kept.
std::string hostOf(const std::string& origin) {
    std::string host = origin;
    for (const char* scheme : {"https://", "http://"}) {
        const std::string prefix = scheme;
        if (host.rfind(prefix, 0) == 0) host = host.substr(prefix.size());
    }
    std::size_t end = std::string::npos;
    if (!host.empty() && host[0] == '[') {
        const std::size_t close = host.find(']');
        if (close != std::string::npos) end = close + 1;
    } else {
        end = host.find(':');
    }
    if (end != std::string::npos) host = host.substr(0, end);
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return host;
}

// The machine itself: the only place plain http may carry the key (the tests'
// fakes listen here).
bool isLoopbackHost(const std::string& host) {
    return host == "127.0.0.1" || host == "localhost" || host == "[::1]";
}

// The key goes to ElevenLabs and nowhere else: api.elevenlabs.io, any
// *.elevenlabs.io, or loopback.
bool isAllowedHost(const std::string& host) {
    constexpr const char* kSuffix = ".elevenlabs.io";
    const std::string suffix = kSuffix;
    return host == "api.elevenlabs.io" || isLoopbackHost(host) ||
           (host.size() > suffix.size() &&
            host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0);
}

// ElevenLabs voice ids are short alphanumeric tokens; anything else would
// change the URL path and must never be sent.
bool isValidVoiceId(const std::string& voiceId) {
    return !voiceId.empty() &&
           std::all_of(voiceId.begin(), voiceId.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '_' || c == '-';
           });
}

// The `code` (or legacy `status`) string of an ElevenLabs error body,
// {"detail": {"type", "code", "message", "status", ...}} per
// elevenlabs.io/docs/eleven-api/resources/errors; empty when absent.
std::string errorCode(const std::string& body) {
    const json parsed = json::parse(body, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return "";
    const auto detail = parsed.find("detail");
    if (detail == parsed.end() || !detail->is_object()) return "";
    for (const char* key : {"code", "status"}) {
        const auto it = detail->find(key);
        if (it != detail->end() && it->is_string()) return it->get<std::string>();
    }
    return "";
}

}  // namespace

ElevenLabsOutcome classifyTtsExchange(bool reached, int status, const std::string& body) {
    if (!reached) return ElevenLabsOutcome::Retry;
    if (status == 200) return ElevenLabsOutcome::Deliver;
    if (status == 401 || status == 402) return ElevenLabsOutcome::Disable;
    if (status == 404) return ElevenLabsOutcome::UnknownVoice;
    if (status == 429) {
        // The documented 429 codes are all rate limits; older deployments put
        // quota exhaustion here too, and a quota does not clear on retry.
        return errorCode(body).find("quota") != std::string::npos ? ElevenLabsOutcome::Disable
                                                                  : ElevenLabsOutcome::Retry;
    }
    if (status >= 500) return ElevenLabsOutcome::Retry;
    return ElevenLabsOutcome::Fallback;
}

std::uint64_t elevenLabsCharacterLimit(const std::string& modelId) {
    // elevenlabs.io/docs/models, character limit table (fetched 2026-10-09).
    if (modelId == "eleven_flash_v2_5") return 40000;
    if (modelId == "eleven_flash_v2") return 30000;
    if (modelId == "eleven_turbo_v2_5" || modelId == "eleven_turbo_v2") return 30000;
    if (modelId == "eleven_multilingual_v2" || modelId == "eleven_v4") return 10000;
    return 5000;
}

std::uint32_t elevenLabsSampleRate(const std::string& outputFormat) {
    std::string rest;
    if (outputFormat.rfind("pcm_", 0) == 0) {
        rest = outputFormat.substr(4);
    } else if (outputFormat.rfind("wav_", 0) == 0) {
        rest = outputFormat.substr(4);
    } else {
        return 0;
    }
    if (rest.empty() || rest.size() > 6) return 0;
    std::uint32_t rate = 0;
    for (const char c : rest) {
        if (c < '0' || c > '9') return 0;
        rate = rate * 10 + static_cast<std::uint32_t>(c - '0');
    }
    return rate;
}

ElevenLabsBackend::ElevenLabsBackend(TtsConfig config)
    : config_(std::move(config)), sampleRate_(elevenLabsSampleRate(config_.elevenLabsOutputFormat)) {
    if (sampleRate_ == 0) {
        std::cerr << kLog << "elevenlabs_output_format \"" << config_.elevenLabsOutputFormat
                  << "\" is not pcm_* or wav_*; only those can be played without a decoder. "
                     "Using pcm_22050\n";
        config_.elevenLabsOutputFormat = "pcm_22050";
        sampleRate_ = 22050;
    }
}

TtsUsage ElevenLabsBackend::usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return usage_;
}

bool ElevenLabsBackend::disabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return disabled_;
}

void ElevenLabsBackend::requestStop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopRequested_ = true;
    }
    stopCv_.notify_all();
}

void ElevenLabsBackend::disable(const std::string& why) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (disabled_) return;
    disabled_ = true;
    std::cerr << kLog << why << "; NPC voices off for the rest of this session\n";
}

TtsResult ElevenLabsBackend::decline(const std::string& why) const {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (why != lastDeclineReason_) {
            lastDeclineReason_ = why;
            std::cerr << kLog << why << "; this line is silent\n";
        }
    }
    TtsResult result;
    result.error = why;
    return result;
}

void ElevenLabsBackend::recordUsage(std::uint64_t characters) {
    std::uint64_t total = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        usage_.requests += 1;
        usage_.charactersSent += characters;
        total = usage_.charactersSent;
    }
    if (config_.characterBudget > 0 && total >= config_.characterBudget) {
        disable("session character budget of " + std::to_string(config_.characterBudget) +
                " reached (sent " + std::to_string(total) + ")");
    }
}

ElevenLabsBackend::Exchange ElevenLabsBackend::send(const ParsedUrl& url, const TtsVoice& voice,
                                                    const std::string& body) const {
    httplib::Client cli(url.origin);
    cli.set_read_timeout(config_.elevenLabsTimeoutSeconds, 0);
    cli.set_write_timeout(config_.elevenLabsTimeoutSeconds, 0);
    cli.set_connection_timeout(10, 0);

    // The key travels in this header and nowhere else: never logged, never in
    // a body, never in an error string.
    const httplib::Headers headers = {{"xi-api-key", config_.apiKey}};
    const std::string path = url.pathPrefix + kConvertPath + voice.voiceId +
                             "?output_format=" + config_.elevenLabsOutputFormat;
    const httplib::Result result = cli.Post(path, headers, body, "application/json");

    Exchange exchange;
    if (!result) {
        exchange.transportError = result.error() == httplib::Error::Success
                                      ? "request aborted"
                                      : to_string(result.error());
        return exchange;
    }
    exchange.reached = true;
    exchange.status = result->status;
    exchange.body = result->body;
    return exchange;
}

TtsResult ElevenLabsBackend::synthesize(const std::string& text, const TtsVoice& voice) {
    if (disabled()) return decline("elevenlabs backend is disabled for this session");
    if (voice.voiceId.empty()) return decline("no voice id for " + voice.npcId);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (unknownVoiceNpcs_.count(voice.npcId)) {
            TtsResult result;
            result.error = "voice for " + voice.npcId + " is unknown to the API";
            return result;  // already logged once
        }
        if (!isValidVoiceId(voice.voiceId)) {
            // Same treatment as a 404: that NPC is silent this session, said once.
            unknownVoiceNpcs_.insert(voice.npcId);
            std::cerr << kLog << "voice id \"" << voice.voiceId << "\" for NPC \"" << voice.npcId
                      << "\" is not [A-Za-z0-9_-]+; that NPC is silent this session\n";
            TtsResult result;
            result.error = "voice id for " + voice.npcId + " is malformed";
            return result;
        }
    }

    const std::string modelId = voice.modelId.empty() ? config_.elevenLabsModel : voice.modelId;
    const std::uint64_t characters = ttsCharacterCount(text);
    if (characters == 0) return decline("empty line for " + voice.npcId);
    if (characters > elevenLabsCharacterLimit(modelId)) {
        return decline("line of " + std::to_string(characters) + " characters exceeds the " +
                       std::to_string(elevenLabsCharacterLimit(modelId)) + " limit of " + modelId);
    }
    if (config_.characterBudget > 0) {
        const std::uint64_t sent = usage().charactersSent;
        if (sent + characters > config_.characterBudget) {
            disable("session character budget of " + std::to_string(config_.characterBudget) +
                    " would be exceeded (sent " + std::to_string(sent) + ")");
            return decline("character budget reached");
        }
    }

    const ParsedUrl url = parseBaseUrl(config_.elevenLabsBaseUrl);
    const std::string host = hostOf(url.origin);
    if (!url.https && !isLoopbackHost(host)) {
        return decline("refusing to send the key over plain http to " + url.origin);
    }
    if (!isAllowedHost(host)) {
        return decline("refusing to send the key to " + url.origin +
                       "; elevenlabs_base_url must be api.elevenlabs.io, *.elevenlabs.io or loopback");
    }
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
    if (url.https) {
        if (!loggedTls_) {
            loggedTls_ = true;
            std::cerr << kLog << "built without OpenSSL, cannot reach " << config_.elevenLabsBaseUrl
                      << "; NPC voices off\n";
        }
        return decline("built without OpenSSL; https provider unreachable");
    }
#endif

    // An NPC line can carry a truncated UTF-8 sequence (a line clipped at a
    // byte boundary); dump() would throw on it, so replace rather than throw.
    const std::string body = json{{"text", text}, {"model_id", modelId}}.dump(
        -1, ' ', false, json::error_handler_t::replace);

    Exchange exchange = send(url, voice, body);
    ElevenLabsOutcome outcome = classifyTtsExchange(exchange.reached, exchange.status, exchange.body);
    if (outcome == ElevenLabsOutcome::Retry) {
        std::cerr << kLog << "transient failure ("
                  << (exchange.reached ? "HTTP " + std::to_string(exchange.status)
                                       : exchange.transportError)
                  << "), retrying once\n";
        bool stopping = false;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopping = stopCv_.wait_for(lock,
                                        std::chrono::milliseconds(config_.elevenLabsRetryBackoffMs),
                                        [this] { return stopRequested_; });
        }
        if (stopping) return decline("shutting down; retry skipped");
        exchange = send(url, voice, body);
        outcome = classifyTtsExchange(exchange.reached, exchange.status, exchange.body);
        if (outcome == ElevenLabsOutcome::Retry) outcome = ElevenLabsOutcome::Fallback;
    }

    switch (outcome) {
        case ElevenLabsOutcome::Disable:
            disable("HTTP " + std::to_string(exchange.status) + " from ElevenLabs (" +
                    clipForLog(errorCode(exchange.body), config_.apiKey) + "): the key in $" +
                    config_.apiKeyEnv + " (or config/secrets.cfg) was rejected or has no credit");
            return decline("elevenlabs key rejected or out of credit");
        case ElevenLabsOutcome::UnknownVoice: {
            std::lock_guard<std::mutex> lock(mutex_);
            unknownVoiceNpcs_.insert(voice.npcId);
            std::cerr << kLog << "voice \"" << voice.voiceId << "\" for NPC \"" << voice.npcId
                      << "\" is unknown to the API (HTTP 404); that NPC is silent this session\n";
            TtsResult result;
            result.error = "voice for " + voice.npcId + " is unknown to the API";
            return result;
        }
        case ElevenLabsOutcome::Fallback:
            return decline(exchange.reached
                               ? "HTTP " + std::to_string(exchange.status) + ": " +
                                     clipForLog(exchange.body, config_.apiKey)
                               : "could not reach " + config_.elevenLabsBaseUrl + " (" +
                                     exchange.transportError + ")");
        case ElevenLabsOutcome::Retry:  // converted to Fallback above
        case ElevenLabsOutcome::Deliver:
            break;
    }

    recordUsage(characters);
    if (exchange.body.empty()) return decline("200 with no audio bytes");

    {
        // A success resets the once-per-reason log, so the same transient
        // failure recurring later is reported again.
        std::lock_guard<std::mutex> lock(mutex_);
        lastDeclineReason_.clear();
    }
    TtsResult result;
    result.ok = true;
    if (config_.elevenLabsOutputFormat.rfind("wav_", 0) == 0) {
        if (!parseWavHeader(exchange.body)) return decline("200 whose body is not a WAV file");
        result.wav = std::move(exchange.body);
    } else {
        result.wav = wavFromPcm16(exchange.body, sampleRate_);
    }
    return result;
}

}  // namespace llm_npc
