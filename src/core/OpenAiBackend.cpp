#include "OpenAiBackend.hpp"

#include <string_view>
#include <utility>

#include "OpenAiChunk.hpp"
#include "StreamAssembler.hpp"
#include "httplib.h"
#include "json.hpp"

namespace llm_npc {

using nlohmann::json;

namespace {

// Splits a base URL like "https://openrouter.ai/api/v1" into the
// scheme+host[:port] origin that httplib::Client wants and the leading path
// prefix ("/api/v1"). Defaults to https when no scheme is present.
struct ParsedUrl {
    std::string origin;      // "https://openrouter.ai"
    std::string pathPrefix;  // "/api/v1" (no trailing slash)
    bool https = true;
};

ParsedUrl parseBaseUrl(const std::string& url) {
    ParsedUrl out;
    std::string rest = url;
    if (rest.rfind("http://", 0) == 0) {
        out.https = false;
        rest = rest.substr(7);
    } else if (rest.rfind("https://", 0) == 0) {
        out.https = true;
        rest = rest.substr(8);
    }
    const auto slash = rest.find('/');
    const std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
    out.pathPrefix = slash == std::string::npos ? "" : rest.substr(slash);
    while (!out.pathPrefix.empty() && out.pathPrefix.back() == '/') out.pathPrefix.pop_back();
    out.origin = (out.https ? "https://" : "http://") + hostPort;
    return out;
}

}  // namespace

OpenAiBackend::OpenAiBackend(LlmConfig config) : config_(std::move(config)) {
    if (config_.baseUrl.empty()) config_.baseUrl = "https://openrouter.ai/api/v1";
}

std::string OpenAiBackend::model() const {
    std::lock_guard<std::mutex> lock(modelMutex_);
    return config_.model;
}

void OpenAiBackend::setModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(modelMutex_);
    config_.model = model;
}

std::vector<std::string> OpenAiBackend::listModels() {
    // Cloud catalogs run to hundreds of models and selection is config-driven,
    // so we surface only the configured model rather than a giant menu.
    return {model()};
}

BackendResult OpenAiBackend::chat(const BackendRequest& req, const DeltaCallback& onDelta) {
    BackendResult reply;
    // No keep-alive concept here, so warm-up is a no-op (and would otherwise cost
    // a paid token round-trip).
    if (req.internal) {
        reply.ok = true;
        return reply;
    }

    const ParsedUrl url = parseBaseUrl(config_.baseUrl);

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
    if (url.https) {
        reply.error = "cloud provider needs an OpenSSL build; rebuild with OpenSSL "
                      "to reach " + config_.baseUrl;
        return reply;
    }
#endif

    std::string model;
    {
        std::lock_guard<std::mutex> lock(modelMutex_);
        model = config_.model;
    }

    json messages = json::array();
    if (!req.systemPrompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", req.systemPrompt}});
    }
    for (const auto& turn : req.history) {
        messages.push_back({{"role", turn.role}, {"content", turn.content}});
    }
    messages.push_back({{"role", "user"}, {"content", req.userMessage}});

    json body = {
        {"model", model},
        {"messages", messages},
        {"stream", true},
        {"temperature", config_.temperature},
    };

    httplib::Client cli(url.origin);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_write_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(10, 0);

    int status = 0;
    std::string errorBody;    // body bytes of a non-200 response
    std::string content;      // accumulated assistant text
    std::string streamError;  // first error found inside the stream
    bool sawDone = false;
    StreamAssembler assembler;

    const auto handleLine = [&](const std::string& line) {
        const OpenAiChunk chunk = parseOpenAiChunk(line);
        if (!chunk.error.empty()) {
            if (streamError.empty()) streamError = chunk.error;
            return;
        }
        if (!chunk.delta.empty()) {
            content += chunk.delta;
            onDelta(chunk.delta);
        }
        if (chunk.done) sawDone = true;
    };

    httplib::Request hreq;
    hreq.method = "POST";
    hreq.path = url.pathPrefix + "/chat/completions";
    hreq.set_header("Content-Type", "application/json");
    if (!config_.apiKey.empty()) hreq.set_header("Authorization", "Bearer " + config_.apiKey);
    hreq.set_header("X-Title", "LLM NPC City");  // optional OpenRouter app attribution
    hreq.body = body.dump();
    hreq.response_handler = [&](const httplib::Response& response) {
        status = response.status;
        return true;
    };
    hreq.content_receiver = [&](const char* data, size_t length, uint64_t, uint64_t) {
        if (status != 200) {
            errorBody.append(data, length);
        } else {
            assembler.feed(std::string_view(data, length), handleLine);
        }
        return true;
    };

    const httplib::Result result = cli.send(hreq);
    if (!result) {
        const std::string detail = result.error() == httplib::Error::Success
                                       ? "request aborted"
                                       : to_string(result.error());
        reply.error = "could not reach LLM provider at " + config_.baseUrl + " (" + detail + ")";
        return reply;
    }

    const std::string tail = assembler.takeRemainder();
    if (!tail.empty()) handleLine(tail);

    if (status != 200) {
        reply.error = "provider returned HTTP " + std::to_string(status) +
                      (errorBody.empty() ? "" : ": " + errorBody);
        return reply;
    }
    if (!streamError.empty()) {
        reply.error = streamError;
        return reply;
    }
    // Some providers omit a [DONE]/finish_reason on the final chunk; accept any
    // content that actually streamed.
    if (!sawDone && content.empty()) {
        reply.error = "provider stream ended before any content";
        return reply;
    }
    reply.ok = true;
    reply.content = std::move(content);
    return reply;
}

}  // namespace llm_npc
