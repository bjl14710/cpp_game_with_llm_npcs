#include "LlmClient.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "LlmBackend.hpp"

namespace llm_npc {

LlmClient::LlmClient(LlmConfig config)
    : config_(std::move(config)), backend_(makeBackend(config_)) {
    worker_ = std::thread(&LlmClient::workerLoop, this);
}

LlmClient::~LlmClient() {
    {
        // Setting stop_ under the queue mutex serializes with the worker's
        // predicate check: without it, the store+notify can land between the
        // worker evaluating the predicate (false) and blocking on the CV — a
        // lost wakeup that leaves join() hanging forever.
        std::lock_guard<std::mutex> lock(requestMutex_);
        stop_.store(true);
    }
    requestCv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void LlmClient::enqueue(ChatRequest req) {
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        requests_.push(std::move(req));
    }
    requestCv_.notify_one();
}

std::uint64_t LlmClient::submit(std::string systemPrompt,
                                std::vector<ChatTurn> history,
                                std::string userMessage,
                                const std::string& speakerId,
                                Familiarity familiarity) {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.systemPrompt = std::move(systemPrompt);
    req.history = std::move(history);
    req.userMessage = std::move(userMessage);

    // The bank is consulted here, on the calling thread, because the lookup is
    // microseconds of string work — pushing it to the worker would add a thread
    // hop to the one path that exists to be fast. An empty speakerId (group
    // turns, world generation) never reaches the bank at all.
    if (lineBank_ && !speakerId.empty()) {
        if (auto banked = lineBank_->lookup(speakerId, familiarity, req.userMessage)) {
            req.bankedReply = std::move(*banked);
        }
    }

    const std::uint64_t id = req.id;
    enqueue(std::move(req));
    return id;
}

void LlmClient::setLineBank(std::unique_ptr<LineBank> bank) {
    lineBank_ = std::move(bank);
}

void LlmClient::warmUp() {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.internal = true;
    enqueue(std::move(req));
}

std::vector<ChatDelta> LlmClient::drainDeltas() {
    std::vector<ChatDelta> out;
    std::lock_guard<std::mutex> lock(replyMutex_);
    out.swap(deltas_);
    return out;
}

std::vector<ChatReply> LlmClient::drainReplies() {
    std::vector<ChatReply> out;
    std::lock_guard<std::mutex> lock(replyMutex_);
    out.swap(replies_);
    return out;
}

std::vector<std::string> LlmClient::availableModels() { return backend_->listModels(); }

std::string LlmClient::model() const { return backend_->model(); }

void LlmClient::setModel(std::string model) { backend_->setModel(model); }

void LlmClient::workerLoop() {
    while (true) {
        ChatRequest req;
        {
            std::unique_lock<std::mutex> lock(requestMutex_);
            requestCv_.wait(lock, [this] { return stop_.load() || !requests_.empty(); });
            if (stop_.load() && requests_.empty()) return;
            req = std::move(requests_.front());
            requests_.pop();
        }

        inFlight_.store(true);
        ChatReply reply = processOne(req);
        if (!req.internal) {
            std::lock_guard<std::mutex> lock(replyMutex_);
            replies_.push_back(std::move(reply));
        }
        // Drop the flag only after the reply is queued so UI can poll busy()
        // without seeing a brief idle window before the reply lands.
        inFlight_.store(false);
    }
}

ChatReply LlmClient::streamBanked(const ChatRequest& req) {
    ChatReply reply;
    reply.id = req.id;
    reply.ok = true;
    reply.content = req.bankedReply;

    // Six bytes is small enough to look like generation and large enough that
    // a long reply is not thousands of queue pushes.
    constexpr std::size_t kChunk = 6;
    const int cps = config_.lineBankCps > 0 ? config_.lineBankCps : 1;
    const auto perChunk = std::chrono::microseconds(
        static_cast<long long>(1000000.0 * static_cast<double>(kChunk) / cps));

    const std::string& text = req.bankedReply;
    for (std::size_t at = 0; at < text.size();) {
        // Never split a UTF-8 sequence: a continuation byte is 10xxxxxx, and a
        // half-written glyph would flicker in the frame that lands mid-chunk.
        std::size_t end = std::min(at + kChunk, text.size());
        while (end < text.size() &&
               (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
            ++end;
        }
        {
            std::lock_guard<std::mutex> lock(replyMutex_);
            deltas_.push_back(ChatDelta{req.id, text.substr(at, end - at)});
        }
        at = end;
        // The first chunk goes out with no delay, so time-to-first-token stays
        // effectively zero — that is the number this feature exists to beat.
        // On shutdown, stop pacing and flush the rest so the reply is still
        // complete and the destructor's join() does not wait on sleeps.
        if (at < text.size() && !stop_.load()) std::this_thread::sleep_for(perChunk);
    }
    return reply;
}

ChatReply LlmClient::processOne(const ChatRequest& req) {
    if (!req.bankedReply.empty()) return streamBanked(req);

    ChatReply reply;
    reply.id = req.id;

    BackendRequest backendReq;
    backendReq.systemPrompt = req.systemPrompt;
    backendReq.history = req.history;
    backendReq.userMessage = req.userMessage;
    backendReq.internal = req.internal;

    // Each text fragment becomes a ChatDelta tagged with this request's id, so
    // the UI can stream it. Warm-up requests stay silent.
    const auto onDelta = [&](const std::string& delta) {
        if (req.internal) return;
        std::lock_guard<std::mutex> lock(replyMutex_);
        deltas_.push_back(ChatDelta{req.id, delta});
    };
    const BackendResult result = backend_->chat(backendReq, onDelta);
    reply.ok = result.ok;
    reply.content = result.content;
    reply.errorMessage = result.error;
    return reply;
}

}  // namespace llm_npc
