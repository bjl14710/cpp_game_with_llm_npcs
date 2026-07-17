// End-to-end memory tests for the "talk, walk away, come back" flow a player
// actually exercises. These drive the REAL stack — Npc + LlmClient + the
// Ollama wire protocol (FakeOllama) + ConversationStore — and assert on the
// exact request bytes each NPC sends the model, so "the NPC remembers" means
// "the remembered text is provably in the prompt the model receives", not a
// proxy for it.
//
// Two layers are covered:
//   1. In-session history: each Npc carries its own bounded transcript, so
//      talking to a second NPC never disturbs the first, and returning to the
//      first still sends everything it was told.
//   2. Cross-session memory: leaving a conversation summarizes it into a
//      first-person note (the summary request mirrors src/app/main.cpp's
//      requestSummary), ConversationStore persists it, and a fresh session
//      folds that note back into the system prompt.
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include "ConversationStore.hpp"
#include "FakeOllama.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "doctest.h"

using namespace llm_npc;
using llm_npc_test::FakeOllama;
namespace fs = std::filesystem;

namespace {

Persona persona(const std::string& name, const std::string& role) {
    Persona p;
    p.name = name;
    p.role = role;
    return p;
}

// One ask -> reply -> onReplyArrived round trip. Returns the assistant text the
// NPC accepted and the raw request body the NPC sent for THIS turn (the first
// body that appeared during the call), so a test can inspect exactly what the
// model was shown.
struct Turn {
    std::string text;
    std::string body;
};

Turn exchange(Npc& npc, LlmClient& client, FakeOllama& fake, const std::string& line) {
    const std::size_t before = fake.requestBodies().size();
    const auto id = npc.ask(line);
    std::string text;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool got = false;
    while (!got && std::chrono::steady_clock::now() < deadline) {
        for (const auto& reply : client.drainReplies()) {
            if (reply.id != id) continue;
            text = npc.onReplyArrived(reply).value_or("");
            got = true;
            break;
        }
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto bodies = fake.requestBodies();
    std::string body = before < bodies.size() ? bodies[before] : std::string{};
    return {text, body};
}

// Mirrors src/app/main.cpp requestSummary(): builds the transcript from the
// NPC's history, folds in any prior memory, and asks the model for an updated
// first-person note. Returns the note plus the request body (so the test can
// confirm the summarizer was actually shown the conversation). Keep in sync
// with main.cpp — a drift here failing the test is the intended signal.
Turn summarize(Npc& npc, LlmClient& client, FakeOllama& fake) {
    std::string transcript;
    for (const auto& t : npc.history()) {
        transcript += (t.role == "user" ? "Player: " : npc.persona().name + ": ");
        transcript += t.content + "\n";
    }
    std::string user = "You are " + npc.persona().name +
                       ". Here is your conversation with the player so far:\n" +
                       transcript;
    if (!npc.memory().empty()) {
        user += "You previously remembered: " + npc.memory() + "\n";
    }
    user += "In at most 3 sentences, first person, write the updated note you "
            "keep about this player: who they are, what happened, anything to "
            "remember next time. Reply with the note only.";

    const std::size_t before = fake.requestBodies().size();
    const auto id = client.submit(
        "You write concise first-person memory notes for a game character. "
        "No preamble, no directives, just the note.",
        {}, std::move(user));
    std::string note;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    bool got = false;
    while (!got && std::chrono::steady_clock::now() < deadline) {
        for (const auto& reply : client.drainReplies()) {
            if (reply.id != id) continue;
            if (reply.ok) note = reply.content;
            got = true;
            break;
        }
        if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const auto bodies = fake.requestBodies();
    std::string body = before < bodies.size() ? bodies[before] : std::string{};
    return {note, body};
}

fs::path freshTempDir(const std::string& stem) {
    const fs::path dir = fs::temp_directory_path() / stem;
    std::error_code ec;
    fs::remove_all(dir, ec);
    return dir;
}

}  // namespace

TEST_CASE("Returning to an NPC after talking to another still sends the first "
          "NPC everything it was told") {
    FakeOllama fake;
    fake.setReply("Lovely to meet you! [[MOOD: happy]]");
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});

    Npc baker(persona("Marge", "baker"), client);
    Npc barista(persona("Theo", "barista"), client);

    // 1) Tell the baker a specific, memorable fact.
    exchange(baker, client, fake, "Hi Marge, I'm Sam and my dog is named Biscuit.");

    // 2) Go have an unrelated chat with a different NPC.
    const Turn baristaTurn =
        exchange(barista, client, fake, "Hey Theo, what's good today?");

    // 3) Come back to the baker and ask about the fact.
    const Turn back = exchange(baker, client, fake, "Do you remember my dog's name?");

    // The baker's return request carries the whole earlier exchange, so the
    // model can answer from it.
    CHECK(back.body.find("Biscuit") != std::string::npos);
    CHECK(back.body.find("Sam") != std::string::npos);
    REQUIRE(baker.history().size() == 4);  // two full exchanges, intact

    // Memory is per-NPC: the unrelated barista never saw the baker's secret.
    CHECK(baristaTurn.body.find("Biscuit") == std::string::npos);
    CHECK(barista.history().size() == 2);
}

TEST_CASE("A summarized conversation is persisted and reloaded into the next "
          "session's prompt") {
    const fs::path dbDir = freshTempDir("llm_npc_memflow_x");
    const fs::path dbPath = dbDir / "conversations.sqlite3";

    // ---- Session 1: talk, then leave (which summarizes + persists). ----
    {
        FakeOllama fake;
        fake.setReply("Right away! [[MOOD: neutral]]");
        LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
        Npc baker(persona("Marge", "baker"), client);

        exchange(baker, client, fake,
                 "I'm Sam. I'm allergic to walnuts, so no walnut bread for me.");

        // The player walks away: summarize this conversation into a note.
        fake.setReply("Sam is a regular who is allergic to walnuts; keep walnut "
                      "bread away from them.");
        const Turn note = summarize(baker, client, fake);

        // The summarizer was actually shown the conversation it must condense.
        CHECK(note.body.find("walnut") != std::string::npos);
        CHECK(note.body.find("Sam") != std::string::npos);
        REQUIRE_FALSE(note.text.empty());

        ConversationStore store(dbPath);
        REQUIRE(store.ok());
        REQUIRE(store.save("Marge", note.text, baker.history()));
    }

    // ---- Session 2: fresh process, fresh NPC, memory loaded from disk. ----
    {
        FakeOllama fake;
        fake.setReply("Of course I remember you! [[MOOD: happy]]");
        LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});

        ConversationStore store(dbPath);
        REQUIRE(store.ok());
        const NpcMemory loaded = store.load("Marge");
        REQUIRE_FALSE(loaded.summary.empty());

        Npc baker(persona("Marge", "baker"), client);
        baker.setMemory(loaded.summary);  // exactly what main.cpp does at boot

        // A brand-new conversation (empty history) still carries the walnut
        // allergy, because the remembered note rides in the system prompt.
        const Turn greet = exchange(baker, client, fake, "Morning! Anything fresh?");
        CHECK(greet.body.find("walnut") != std::string::npos);
        REQUIRE(baker.history().size() == 2);  // history itself started clean
    }

    std::error_code ec;
    fs::remove_all(dbDir, ec);
}
