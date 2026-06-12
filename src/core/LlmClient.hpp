#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace llm_npc {

// A single turn in a chat history. Role is "user" or "assistant".
struct ChatTurn {
    std::string role;
    std::string content;
};

struct LlmConfig {
    std::string host = "localhost";
    int port = 11434;
    std::string model = "qwen2.5:3b-instruct";
    double temperature = 0.8;
    int requestTimeoutSeconds = 60;
    // Passed to Ollama as keep_alive so the model stays loaded between
    // conversations instead of paying the cold-start cost every time.
    std::string keepAlive = "10m";
};

// One request submitted to the LLM.
struct ChatRequest {
    std::uint64_t id = 0;
    std::string systemPrompt;
    std::vector<ChatTurn> history;
    std::string userMessage;
    // Internal requests (model warm-up) never produce a ChatReply or deltas.
    bool internal = false;
};

// One incremental fragment of an in-flight streamed reply, surfaced to the
// main thread via drainDeltas() so the UI can show words as they arrive.
struct ChatDelta {
    std::uint64_t id = 0;
    std::string text;
};

// One reply pulled off the queue on the main thread.
struct ChatReply {
    std::uint64_t id = 0;
    bool ok = false;
    std::string content;       // model's reply when ok
    std::string errorMessage;  // human-readable error when !ok
};

// Centralized LLM access point. One worker thread serves all NPCs.
// Callers submit() requests from the main thread, drainDeltas() each frame to
// stream partial text, and drainReplies() to receive completed replies. The
// id returned from submit() lets callers match deltas and replies back to the
// request they sent.
class LlmClient {
   public:
    explicit LlmClient(LlmConfig config);
    ~LlmClient();

    LlmClient(const LlmClient&) = delete;
    LlmClient& operator=(const LlmClient&) = delete;

    // Enqueue a chat request. Returns a unique id used to correlate replies.
    std::uint64_t submit(std::string systemPrompt,
                         std::vector<ChatTurn> history,
                         std::string userMessage);

    // Enqueue an internal request that makes Ollama load the model into
    // memory (empty messages + keep_alive). Produces no deltas and no reply;
    // call once at startup so the first real conversation answers fast.
    void warmUp();

    // Pop all streamed text fragments that arrived since the last call.
    // Non-blocking; fragments are in arrival order.
    std::vector<ChatDelta> drainDeltas();

    // Pop all replies that have arrived since the last call. Non-blocking.
    std::vector<ChatReply> drainReplies();

    // True while at least one request is in flight or queued. Useful for UI.
    bool busy() const { return inFlight_.load(); }

    const LlmConfig& config() const { return config_; }

   private:
    void workerLoop();
    ChatReply processOne(const ChatRequest& req);
    void enqueue(ChatRequest req);

    LlmConfig config_;

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> inFlight_{false};

    std::mutex requestMutex_;
    std::condition_variable requestCv_;
    std::queue<ChatRequest> requests_;

    std::mutex replyMutex_;
    std::vector<ChatReply> replies_;
    std::vector<ChatDelta> deltas_;

    std::atomic<std::uint64_t> nextId_{1};
};

}  // namespace llm_npc
