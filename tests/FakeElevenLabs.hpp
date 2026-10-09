#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "httplib.h"
#include "json.hpp"

namespace llm_npc_test {

// In-process stand-in for POST /v1/text-to-speech/{voice_id} so the backend's
// request shape and every row of its error-handling table can be tested
// offline. Records each request's voice id, output_format, headers and body.
// Error bodies follow elevenlabs.io/docs/eleven-api/resources/errors:
// {"detail": {"type", "code", "message", "status"}}.
class FakeElevenLabs {
   public:
    enum class Mode {
        Ok,              // 200 with `sampleCount` 16-bit samples of raw PCM
        OkWav,           // 200 with a WAV file (for wav_* formats)
        Http401,         // authentication_error / invalid_api_key
        Http402,         // payment_required / insufficient_credits
        Http429,         // rate_limit_error / rate_limit_exceeded
        Http429Quota,    // 429 whose code is quota_exceeded (legacy deployments)
        Http500,         // internal_error
        FailOnceThenOk,  // first request 500, every later one Ok
        VoiceNotFound,   // 404 not_found / voice_not_found
        Http400,         // validation_error / text_too_long
        Empty200,        // 200 with no body
    };

    FakeElevenLabs() {
        server_.Post(R"(/v1/text-to-speech/([^/?]+))",
                     [this](const httplib::Request& req, httplib::Response& res) {
                         Mode mode;
                         {
                             std::lock_guard<std::mutex> lock(mutex_);
                             Seen seen;
                             seen.voiceId = req.matches[1];
                             seen.outputFormat = req.get_param_value("output_format");
                             seen.body = req.body;
                             seen.headers["xi-api-key"] = req.get_header_value("xi-api-key");
                             seen.headers["content-type"] = req.get_header_value("Content-Type");
                             requests_.push_back(seen);
                             mode = mode_;
                             if (mode == Mode::FailOnceThenOk) {
                                 mode = requests_.size() == 1 ? Mode::Http500 : Mode::Ok;
                             }
                         }
                         respond(mode, res);
                     });
        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        server_.wait_until_ready();
    }

    ~FakeElevenLabs() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    FakeElevenLabs(const FakeElevenLabs&) = delete;
    FakeElevenLabs& operator=(const FakeElevenLabs&) = delete;

    // One recorded request.
    struct Seen {
        std::string voiceId;
        std::string outputFormat;
        std::string body;
        std::map<std::string, std::string> headers;
    };

    int port() const { return port_; }
    std::string baseUrl() const { return "http://127.0.0.1:" + std::to_string(port_); }

    void setMode(Mode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        mode_ = mode;
    }
    // How many 16-bit samples an Ok reply carries.
    void setSampleCount(std::size_t samples) {
        std::lock_guard<std::mutex> lock(mutex_);
        sampleCount_ = samples;
    }
    // Text an error body echoes in its message (to prove masking).
    void setEchoInErrors(std::string text) {
        std::lock_guard<std::mutex> lock(mutex_);
        echo_ = std::move(text);
    }

    std::vector<Seen> requests() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }

    // The raw PCM bytes an Ok reply returns.
    static std::string pcmOf(std::size_t samples) {
        std::string pcm;
        for (std::size_t i = 0; i < samples; ++i) {
            pcm.push_back(static_cast<char>(i & 0xFF));
            pcm.push_back(static_cast<char>((i >> 8) & 0x7F));
        }
        return pcm;
    }

   private:
    void respond(Mode mode, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto error = [&](int status, const char* type, const char* code) {
            res.status = status;
            nlohmann::json body = {{"detail",
                                    {{"type", type},
                                     {"code", code},
                                     {"message", std::string("fake: ") + code + " " + echo_},
                                     {"status", code}}}};
            res.set_content(body.dump(), "application/json");
        };
        switch (mode) {
            case Mode::Http401:
                error(401, "authentication_error", "invalid_api_key");
                return;
            case Mode::Http402:
                error(402, "payment_required", "insufficient_credits");
                return;
            case Mode::Http429:
                error(429, "rate_limit_error", "rate_limit_exceeded");
                return;
            case Mode::Http429Quota:
                error(429, "rate_limit_error", "quota_exceeded");
                return;
            case Mode::Http500:
            case Mode::FailOnceThenOk:
                error(500, "internal_error", "internal_error");
                return;
            case Mode::VoiceNotFound:
                error(404, "not_found", "voice_not_found");
                return;
            case Mode::Http400:
                error(400, "validation_error", "text_too_long");
                return;
            case Mode::Empty200:
                res.status = 200;
                res.set_content("", "audio/pcm");
                return;
            case Mode::OkWav: {
                // Minimal 8 kHz mono WAV header + the samples.
                std::string pcm = pcmOf(sampleCount_);
                std::string wav = "RIFF";
                const auto le32 = [&](std::uint32_t v) {
                    for (int i = 0; i < 4; ++i) wav.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
                };
                const auto le16 = [&](std::uint16_t v) {
                    wav.push_back(static_cast<char>(v & 0xFF));
                    wav.push_back(static_cast<char>(v >> 8));
                };
                le32(36 + static_cast<std::uint32_t>(pcm.size()));
                wav += "WAVEfmt ";
                le32(16); le16(1); le16(1); le32(8000); le32(16000); le16(2); le16(16);
                wav += "data";
                le32(static_cast<std::uint32_t>(pcm.size()));
                wav += pcm;
                res.status = 200;
                res.set_content(wav, "audio/wav");
                return;
            }
            case Mode::Ok:
                break;
        }
        res.status = 200;
        res.set_content(pcmOf(sampleCount_), "audio/pcm");
    }

    httplib::Server server_;
    std::thread thread_;
    int port_ = 0;

    std::mutex mutex_;
    Mode mode_ = Mode::Ok;
    std::size_t sampleCount_ = 2205;  // a tenth of a second at 22.05 kHz
    std::string echo_;
    std::vector<Seen> requests_;
};

}  // namespace llm_npc_test
