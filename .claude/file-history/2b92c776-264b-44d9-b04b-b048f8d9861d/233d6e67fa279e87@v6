#include "LlmClient.hpp"

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
                                std::string userMessage) {
    ChatRequest req;
    req.id = nextId_.fetch_add(1);
    req.systemPrompt = std::move(systemPrompt);
    req.history = std::move(history);
    req.userMessage = std::move(userMessage);

    const std::uint64_t id = req.id;
    enqueue(std::move(req));
    return id;
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

ChatReply LlmClient::processOne(const ChatRequest& req) {
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
