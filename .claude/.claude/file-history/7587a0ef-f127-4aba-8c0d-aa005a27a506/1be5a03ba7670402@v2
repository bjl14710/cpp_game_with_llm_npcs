# Embedded LLMs in Game Applications

> Topic for `/teach`: how LLMs get plugged into games, what the engineering
> tradeoffs are, and how the choices map onto the code we just wrote.

---

## 1. The concept

An **embedded LLM in a game** is a language model whose output is consumed by
the running game process — usually to generate NPC dialogue, narrate events,
or interpret freeform player input. "Embedded" in this context does **not**
necessarily mean the model lives inside the game's binary; it means the game
itself drives inference (vs. the user opening ChatGPT in another window).
There are three integration styles, and the choice shapes everything else:

- **Cloud API call** (OpenAI, Anthropic, Inworld, Convai). The game POSTs JSON
  to a remote service. Easy to start, no install on the player's machine, best
  model quality. Costs money per token, requires internet, leaks user prompts
  to a third party, and adds 200-800 ms of latency per reply on top of
  generation time. Inworld and Convai are middleware that wrap this pattern
  specifically for game NPCs.
- **Sidecar process** (Ollama, LM Studio, llamafile, vLLM). A separate process
  on the player's machine hosts the model and exposes an HTTP API on
  `localhost`. The game stays small; model swaps are config changes. This is
  the easiest "fully local" path and what we picked for milestone 1.
- **In-process / embedded library** (`llama.cpp`, GGML, ONNX Runtime,
  candle-rs, MLC-LLM). The model lives inside the game binary, called directly
  through a C/C++ API. Lowest latency, no separate install for the player,
  full control over sampling and KV-cache management. Highest engineering
  cost: you own GPU detection, model loading, quantization formats, and
  thread management.

Why this pattern at all? Pre-LLM NPCs used **dialogue trees** — finite,
hand-authored graphs of "if player picks option B, NPC says line 47." That
scales linearly with writer-hours and produces conversations that feel
canned after two playthroughs. LLM-driven NPCs trade authorial control for
combinatorial breadth: the same blacksmith can answer a question no writer
ever anticipated, in voice, because the persona prompt constrains *how* he
talks rather than *what* he can say. The tradeoff is loss of guarantees:
the NPC might hallucinate a quest item that doesn't exist, refuse to answer
in character, or — without guardrails — start discussing real-world topics.
**Persona prompting** plus **bounded history** is the cheap mitigation
(what we used); **structured output** (force JSON with allowed actions) and
**retrieval-augmented generation** (RAG, where the NPC's "memory" is a
searched knowledge base) are the heavier ones.

Industry footprint as of 2026: AI Dungeon (2019, the original) ran on
GPT-2/3 cloud calls. *Suck Up!* (2023) shipped with cloud LLM-driven NPCs
behind a paywalled key. Modders shipped Skyrim and Stardew Valley LLM-NPC
mods using Ollama as a local sidecar. Roblox added cloud LLM "assistants"
into their creator tools. Nobody has shipped a AAA title with fully
embedded local LLMs yet, mostly because consumer GPU VRAM still limits how
big a model can ride along with a 6-8 GB game.

---

## 2. Minimal example

The pattern below is the **whole** sidecar-LLM technique in 15 lines. It
posts a chat request to a local Ollama process and prints the reply. The
real game just wraps this in a worker thread and a reply queue.

```cpp
#include "httplib.h"   // single-header HTTP client
#include "json.hpp"    // single-header JSON
using nlohmann::json;

int main() {
    // 1. Build the chat body. "system" sets the persona; "user" is the player line.
    json body = {
        {"model", "qwen2.5:3b-instruct"},
        {"stream", false},
        {"messages", {
            {{"role", "system"}, {"content", "You are a gruff blacksmith. Reply in 1-2 sentences."}},
            {{"role", "user"},   {"content", "Have you seen any travelers today?"}},
        }},
    };

    // 2. POST to Ollama's local HTTP API. No auth, no keys — it's on localhost.
    httplib::Client cli("localhost", 11434);
    auto res = cli.Post("/api/chat", body.dump(), "application/json");

    // 3. Parse the reply. Ollama returns { "message": { "role", "content" }, ... }
    auto parsed = json::parse(res->body);
    std::cout << parsed["message"]["content"].get<std::string>() << "\n";
}
```

What this example **does not** show but a real game needs:
- **Async**: the call above blocks for ~1-3 seconds. A game loop can't sit on
  it; you spin a worker thread and poll a reply queue.
- **History**: each NPC needs its own prior `messages[]` so context carries
  across turns. Without history every reply is amnesiac.
- **Bounding**: history grows forever if you let it. Cap to N recent turns
  or token-count-trim from the oldest.
- **Failure handling**: Ollama might not be running. The HTTP call might
  time out. The reply might be malformed JSON. None of that should crash
  the game.

---

## 3. How this maps onto our project

Every bullet in the "real game needs" list above corresponds to a piece of
code we wrote. The teaching example is what `LlmClient::processOne()` would
look like with all the production concerns ripped out:

| Concept                | Where it lives in our code |
| ---------------------- | -------------------------- |
| HTTP POST to Ollama    | `LlmClient.cpp` → `processOne()` |
| Persona as system prompt | `Persona.hpp` → `renderSystemPrompt()` |
| Per-NPC history        | `Npc.hpp` → `std::vector<ChatTurn> history_` |
| Bounded history (10 turns) | `Npc.cpp` → `trimHistory()` |
| Async via worker thread | `LlmClient.cpp` → `workerLoop()` |
| Game-loop-friendly polling | `LlmClient.cpp` → `drainReplies()` |
| Failure → in-character message | `main.cpp` → "[Wren seems distracted…]" |

The structural decisions all trace back to one principle: **the LLM is an
external dependency that may be slow or absent, so it must be reachable
from any NPC through one chokepoint that owns its own thread.** That's
why `LlmClient` is a single object the whole game shares — not because
"singleton" is fashionable, but because Ollama can only sensibly serve
one request at a time on a single GPU, and centralizing the queue avoids
each NPC re-implementing the same thread-and-mutex dance.

What we deliberately deferred — and why:

- **Streaming**: Ollama supports `"stream": true`, which returns one JSON
  line per token. Lower perceived latency, more complex client. Add when
  the "thinking…" dwell time feels long.
- **Structured output**: we don't constrain the reply format. If we later
  want NPCs to emit `{"line": "...", "give_item": "rope"}`, switch to
  Ollama's `format: "json"` mode and validate against a schema.
- **In-process llama.cpp**: the public `LlmClient::submit / drainReplies`
  interface hides the transport. Swapping the body of `processOne()` from
  HTTP to a direct `llama_eval()` call is a one-file change.

---

## 4. The actual code (annotated against the concepts)

Below is the heart of `LlmClient.cpp` with the concepts from §1-3 called
out inline. (The file in the repo has the same code with lighter
commentary.)

```cpp
// Concept: SIDECAR INTEGRATION — game talks to Ollama via plain HTTP.
// Same shape as the minimal example, but wrapped in production concerns.
ChatReply LlmClient::processOne(const ChatRequest& req) {
    ChatReply reply;
    reply.id = req.id;

    // Concept: PERSONA + HISTORY — every request is (system prompt) +
    // (prior turns) + (new user line). The persona makes the NPC stay in
    // character; the history makes the conversation feel continuous.
    json messages = json::array();
    if (!req.systemPrompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", req.systemPrompt}});
    }
    for (const auto& turn : req.history) {
        messages.push_back({{"role", turn.role}, {"content", turn.content}});
    }
    messages.push_back({{"role", "user"}, {"content", req.userMessage}});

    // stream=false → one big reply at the end. Switch to true for token-
    // by-token streaming later.
    json body = {
        {"model", config_.model},
        {"messages", messages},
        {"stream", false},
        {"options", {{"temperature", config_.temperature}}},
    };

    // Concept: FAILURE-FRIENDLY TRANSPORT — bounded timeouts so a hung
    // Ollama can't freeze the worker thread.
    httplib::Client cli(config_.host, config_.port);
    cli.set_read_timeout(config_.requestTimeoutSeconds, 0);
    cli.set_connection_timeout(5, 0);

    auto res = cli.Post("/api/chat", body.dump(), "application/json");
    if (!res) {                         // network failure
        reply.errorMessage = "could not reach Ollama";
        return reply;                   // ok stays false; UI handles it
    }
    // ... parse res->body into reply.content ...
    return reply;
}
```

And the **async drain pattern** — the bit that lets a 60 fps game loop
talk to a model that takes seconds to reply:

```cpp
// MAIN THREAD (game loop): submit the request, never wait for it.
companion.ask("Have you seen any travelers?");
ui.setThinking(true);

// MAIN THREAD (next frame and every frame after): pull whatever has landed.
// Concept: ASYNC DRAIN — the worker thread populates a queue; the main
// thread empties it whenever convenient. No locks held across rendering.
for (const auto& reply : client.drainReplies()) {
    auto text = companion.onReplyArrived(reply);
    if (text) ui.appendLine({Kind::Npc, "Wren", *text});
    ui.setThinking(false);
}
```

The worker thread (also in `LlmClient.cpp`) is the classic
condition-variable-driven queue consumer:

```cpp
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
        ChatReply reply = processOne(req);   // the slow part: HTTP to Ollama
        {
            std::lock_guard<std::mutex> lock(replyMutex_);
            replies_.push_back(std::move(reply));
        }
    }
}
```

Two separate mutexes (one for requests, one for replies) is intentional:
producer and consumer don't contend, and the main thread can drain replies
without ever blocking the worker mid-call.

---

## 5. Knowledge check

Try answering these before peeking back at the doc. Good answers explain
the *why*, not just the *what*.

1. **Why does each `Npc` keep its own conversation history rather than
   `LlmClient` keeping a single shared history?**
   *Hint: think about what happens when the player walks from Jim the
   pharmacist over to Jane the pedestrian and starts a new conversation.*

2. **The minimal example in §2 would freeze the game loop for 1-3 seconds
   on every player line. What two pieces of machinery in our `LlmClient`
   prevent that, and what would happen if we removed either one?**

3. **We picked Ollama as a sidecar instead of linking `llama.cpp` directly.
   Name one situation where that decision would be wrong, and one
   situation where it would still be right even after the game ships to
   players.**

*(Sample answers are not included on purpose — write yours down and we can
talk through them.)*

---

## Further reading

- [Ollama API reference](https://github.com/ollama/ollama/blob/main/docs/api.md) — the `/api/chat` endpoint we POST to.
- [`llama.cpp`](https://github.com/ggerganov/llama.cpp) — when you're ready to embed in-process.
- [`cpp-httplib`](https://github.com/yhirose/cpp-httplib) — the single-header HTTP client we vendored.
- [Inworld](https://inworld.ai) and [Convai](https://convai.com) — what the cloud-API approach looks like productized for games.
- [GBNF grammars](https://github.com/ggerganov/llama.cpp/blob/master/grammars/README.md) — how you'd enforce structured output (JSON, dialogue choices) when you graduate beyond freeform replies.
