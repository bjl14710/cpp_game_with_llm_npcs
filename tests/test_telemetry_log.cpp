// Dialogue-turn telemetry capture (TelemetryLog.hpp) — LOOP A.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 4.
//
// SCAFFOLD — every case is skipped until TelemetryLog is implemented. The
// bodies still compile; they are written against the real interface.
//
// The store-failure cases matter more here than the happy path. Telemetry is
// the one subsystem in this game that no player benefits from, so its licence
// to exist is that it can never cost them anything: an unwritable saves/ must
// produce a no-op, not a crash and not a stall.
#include <filesystem>
#include <string>

#include "TelemetryLog.hpp"
#include "doctest.h"

using namespace llm_npc;
namespace fs = std::filesystem;

namespace {

// A unique scratch directory per case, removed by the caller.
fs::path tempDir(const std::string& stem) {
    const fs::path dir = fs::temp_directory_path() / ("telemetry_" + stem);
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

// A fully populated turn, so a test asserting one field is not silently
// asserting a default.
DialogueTurn sampleTurn() {
    DialogueTurn turn;
    turn.ts = 1'750'000'000;
    turn.sessionId = "abc123";
    turn.npcId = "Marge";
    turn.personaFile = "personas/marge.persona";
    turn.model = "qwen3:8b";
    turn.provider = "ollama";
    turn.promptVersion = "v1";
    turn.promptHash = "0123456789abcdef";
    turn.traitIds = {"gossipy", "guarded"};
    turn.roleId = "killer";
    turn.secretOpen = true;
    turn.secretToken = "mill";
    turn.gossipFactIds = {"fact_a", "fact_b"};
    turn.playerLine = "Where were you Tuesday evening?";
    turn.rawOutput = "At home, same as always. [[MOOD: neutral]]";
    turn.spokenText = "At home, same as always.";
    turn.schemaValid = true;
    turn.promptTokens = 812;
    turn.replyTokens = 24;
    turn.latencyMs = 2700;
    turn.ttftMs = 410;
    return turn;
}

}  // namespace

TEST_CASE("a turn round-trips and returns a usable turn id" * doctest::skip()) {
    const fs::path dir = tempDir("roundtrip");
    TelemetryLog log(dir / "telemetry.sqlite3");
    REQUIRE(log.ok());
    const std::int64_t id = log.append(sampleTurn());
    CHECK(id > 0);
    CHECK(log.countThisSession() == 1);
    fs::remove_all(dir);
}

TEST_CASE("an unwritable path degrades to no-ops instead of failing"
          * doctest::skip()) {
    // The contract ConversationStore, RatingLog, LineBank, Storyline and
    // Cutscene all hold. Telemetry must never be able to break a session.
    TelemetryLog log("/proc/definitely/not/writable/telemetry.sqlite3");
    CHECK_FALSE(log.ok());
    CHECK(log.append(sampleTurn()) == 0);
    CHECK_FALSE(log.rate(1, 1));
}

TEST_CASE("quotes and newlines in player and model text survive intact"
          * doctest::skip()) {
    // This is the one store whose input is entirely untrusted text: a player
    // types anything and a model writes anything. Concatenated SQL would
    // corrupt or drop these rows, so the binding is worth a test of its own.
    const fs::path dir = tempDir("quoting");
    TelemetryLog log(dir / "telemetry.sqlite3");
    REQUIRE(log.ok());
    DialogueTurn turn = sampleTurn();
    turn.playerLine = "she said \"it's the mill\"; DROP TABLE dialogue_turns--";
    turn.rawOutput = "Line one.\nLine two. [[MOOD: sad]]";
    CHECK(log.append(turn) > 0);
    fs::remove_all(dir);
}

TEST_CASE("a rating attaches to an existing turn and is rejected for an "
          "unknown one" * doctest::skip()) {
    const fs::path dir = tempDir("rating");
    TelemetryLog log(dir / "telemetry.sqlite3");
    REQUIRE(log.ok());
    const std::int64_t id = log.append(sampleTurn());
    CHECK(log.rate(id, 1));
    CHECK(log.rate(id, -1));  // re-rating replaces
    CHECK(log.rate(id, 0));   // clearing is allowed
    CHECK_FALSE(log.rate(id + 9999, 1));
    fs::remove_all(dir);
}

TEST_CASE("a banked reply is recorded as banked" * doctest::skip()) {
    // The flag exists so latency statistics can exclude these rows: a banked
    // reply's timing measures LineBank's deliberate line_bank_cps pacing, not
    // a model, and averaging the two produces a number about nothing.
    const fs::path dir = tempDir("banked");
    TelemetryLog log(dir / "telemetry.sqlite3");
    REQUIRE(log.ok());
    DialogueTurn turn = sampleTurn();
    turn.banked = true;
    turn.promptTokens = -1;  // no model call, so no counts
    turn.replyTokens = -1;
    CHECK(log.append(turn) > 0);
    fs::remove_all(dir);
}

TEST_CASE("unreported token counts stay -1 and are never inferred"
          * doctest::skip()) {
    // An OpenAI-compatible server that omits `usage` must produce -1, not a
    // character count wearing a token count's name.
    const fs::path dir = tempDir("tokens");
    TelemetryLog log(dir / "telemetry.sqlite3");
    REQUIRE(log.ok());
    DialogueTurn turn = sampleTurn();
    turn.promptTokens = -1;
    turn.replyTokens = -1;
    turn.promptChars = 3'204;
    turn.replyChars = 96;
    CHECK(log.append(turn) > 0);
    fs::remove_all(dir);
}

TEST_CASE("two sessions in one database stay distinguishable"
          * doctest::skip()) {
    const std::string first = newSessionId();
    const std::string second = newSessionId();
    CHECK_FALSE(first.empty());
    CHECK(first != second);
}
