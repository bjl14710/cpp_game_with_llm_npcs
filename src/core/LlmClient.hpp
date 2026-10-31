#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "LineBank.hpp"  // LineBank, Familiarity

namespace llm_npc {

class LlmBackend;  // the swappable provider; see LlmBackend.hpp

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
    // Reasoning-model thinking: "" omits the field entirely (models without
    // the capability never see it); "false"/"true" is sent as Ollama's
    // `think`. qwen3-class models need "false" for chat-speed replies —
    // measured 24s -> 2.7s time-to-first-token (bench/REPORT.md).
    std::string think = "";

    // Which backend serves requests: "ollama" (default, local, no TLS) or any
    // OpenAI-compatible cloud provider ("openrouter", "openai", ...).
    std::string provider = "ollama";
    // OpenAI-compatible base URL; empty lets the backend pick its default
    // (OpenRouter). Point it at http://localhost:11434/v1 to exercise the cloud
    // code path against local Ollama with no TLS.
    std::string baseUrl = "";
    // Name of the environment variable that holds the cloud API key.
    std::string apiKeyEnv = "OPENROUTER_API_KEY";
    // The resolved API key (from the env var or config/secrets.cfg). Filled in
    // at load time and never written back to disk.
    std::string apiKey = "";

    // --- Line bank (banks/*.bank; see banks/README.md) --------------------
    // Authored replies for recurring topics, served locally instead of a round
    // trip. Off by default: the game must behave exactly as it always has
    // until a reviewed bank exists.
    bool lineBank = false;
    // Match confidence a player line must clear to serve a banked reply, 0..1.
    // Higher is more conservative — around 0.85 only near-exact phrasings hit,
    // which collapses the feature to openers and small talk with no code change.
    float lineBankThreshold = 0.62f;
    // Characters per second a banked reply is streamed at. It must arrive like
    // a generated one; delivering it in a single frame reads as a different
    // system, which is the one thing the bank must not feel like.
    int lineBankCps = 220;
};

// One request submitted to the LLM.
struct ChatRequest {
    std::uint64_t id = 0;
    std::string systemPrompt;
    std::vector<ChatTurn> history;
    std::string userMessage;
    // Internal requests (model warm-up) never produce a ChatReply or deltas.
    bool internal = false;
    // Non-empty when the line bank already answered this request. The worker
    // then streams this text locally and never calls the backend.
    std::string bankedReply;
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
    //
    // `speakerId` and `familiarity` are consulted only when a line bank is
    // installed. Callers that leave `speakerId` empty — group turns and world
    // generation — bypass the bank BY CONSTRUCTION rather than by a flag
    // someone can forget to set. A banked hit produces the same deltas and the
    // same reply, under the same id, as a generated one.
    std::uint64_t submit(std::string systemPrompt,
                         std::vector<ChatTurn> history,
                         std::string userMessage,
                         const std::string& speakerId = std::string(),
                         Familiarity familiarity = Familiarity::First);

    // Installs the banked-reply source; ownership transfers. Never calling
    // this (or passing nullptr) leaves every request going to the backend,
    // which is what `line_bank = off` in config/llm.cfg produces.
    void setLineBank(std::unique_ptr<LineBank> bank);
    LineBank* lineBank() { return lineBank_.get(); }

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

    // Live model controls, delegated to the backend so the in-game picker can
    // switch models without restarting. availableModels() may hit the network
    // (e.g. Ollama's /api/tags), so call it off the hot path (menu open).
    std::vector<std::string> availableModels();
    std::string model() const;
    void setModel(std::string model);

   private:
    void workerLoop();
    ChatReply processOne(const ChatRequest& req);
    // Emits a banked reply as paced deltas on the worker thread, then returns
    // it as a normal ChatReply.
    ChatReply streamBanked(const ChatRequest& req);
    void enqueue(ChatRequest req);

    LlmConfig config_;
    std::unique_ptr<LlmBackend> backend_;  // the active provider
    std::unique_ptr<LineBank> lineBank_;   // null unless installed

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
