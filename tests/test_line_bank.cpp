// Scaffolded stubs for the line bank (plan: npc-line-bank, step 2).
// Un-skip each in the commit that implements it.
//
// Tests write their own .bank files to a temp directory rather than reading
// banks/*.bank, so authoring or a nightly regeneration can never break the
// suite. Same approach as tests/test_traits.cpp.
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "LineBank.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

namespace fs = std::filesystem;

// A clean temp bank directory, one per test case name.
fs::path freshBankDir(const char* stem) {
    const fs::path dir = fs::temp_directory_path() / (std::string("llm_npc_bank_") + stem);
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void writeBank(const fs::path& dir, const char* stem, const char* text) {
    std::ofstream out(dir / (std::string(stem) + ".bank"));
    out << text;
}

// Three greeting variants so rotation has something to rotate.
constexpr const char* kMargeBank =
    "persona = Marge Holloway\n"
    "\n"
    "topic = greeting_first\n"
    "  familiarity = first\n"
    "  trigger = hello\n"
    "  trigger = hi there\n"
    "  trigger = good morning\n"
    "  reply = Well, good morning! [[MOOD: happy]]\n"
    "  reply = Morning, love. Sourdough's just out. [[MOOD: happy]]\n"
    "  reply = Hello there! Don't let the warm out. [[MOOD: happy]]\n"
    "\n"
    "topic = greeting_returning\n"
    "  familiarity = returning\n"
    "  trigger = hello\n"
    "  trigger = im back\n"
    "  reply = Back again! The rye or the sourdough today? [[MOOD: happy]]\n";

}  // namespace

TEST_CASE("a well-formed bank file parses into topics and replies" * doctest::skip()) {
    const auto dir = freshBankDir("parse");
    writeBank(dir, "marge", kMargeBank);

    LineBank bank(dir, 0.6f);
    REQUIRE(bank.ok());
    REQUIRE(bank.banks().size() == 1);
    CHECK(bank.banks()[0].speakerId == "Marge Holloway");
    REQUIRE(bank.banks()[0].topics.size() == 2);
    CHECK(bank.banks()[0].topics[0].replies.size() == 3);
    CHECK(bank.errors().empty());
}

TEST_CASE("a missing bank directory degrades to never hitting" * doctest::skip()) {
    // The failure contract shared with ConversationStore and RatingLog: a
    // broken bank leaves the game no worse than not having one.
    LineBank bank(fs::temp_directory_path() / "llm_npc_bank_does_not_exist", 0.6f);
    CHECK_FALSE(bank.ok());
    CHECK_FALSE(bank.lookup("Marge Holloway", Familiarity::First, "hello").has_value());
}

TEST_CASE("one malformed bank is skipped and named, the rest still load" *
          doctest::skip()) {
    const auto dir = freshBankDir("malformed");
    writeBank(dir, "marge", kMargeBank);
    writeBank(dir, "broken", "this file has no persona key and no topics\n");

    LineBank bank(dir, 0.6f);
    CHECK(bank.ok());
    CHECK(bank.banks().size() == 1);
    REQUIRE(bank.errors().size() == 1);
    CHECK(bank.errors()[0].find("broken") != std::string::npos);
}

TEST_CASE("an empty bank file is an error, not a crash" * doctest::skip()) {
    const auto dir = freshBankDir("empty");
    writeBank(dir, "hollow", "");

    LineBank bank(dir, 0.6f);
    CHECK_FALSE(bank.ok());
    CHECK(bank.errors().size() == 1);
}

TEST_CASE("variants rotate and never repeat until all are used" * doctest::skip()) {
    // An NPC that answers "hello" identically twice in one conversation is
    // worse than a slow one — this is the guardrail the hit policy rests on.
    const auto dir = freshBankDir("rotation");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    std::set<std::string> seen;
    for (int i = 0; i < 3; ++i) {
        const auto reply = bank.lookup("Marge Holloway", Familiarity::First, "hello");
        REQUIRE(reply.has_value());
        seen.insert(*reply);
    }
    CHECK(seen.size() == 3);  // all three variants, no repeat
}

TEST_CASE("rotation wraps rather than stalling once variants run out" *
          doctest::skip()) {
    const auto dir = freshBankDir("wrap");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    for (int i = 0; i < 5; ++i) {
        CHECK(bank.lookup("Marge Holloway", Familiarity::First, "hello").has_value());
    }
}

TEST_CASE("a returning player never gets the first-meeting greeting" *
          doctest::skip()) {
    // The one place a banked line can contradict persisted NPC memory.
    const auto dir = freshBankDir("familiarity");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    const auto reply = bank.lookup("Marge Holloway", Familiarity::Returning, "hello");
    REQUIRE(reply.has_value());
    CHECK(reply->find("Back again") != std::string::npos);
}

TEST_CASE("an unknown speaker misses" * doctest::skip()) {
    const auto dir = freshBankDir("speaker");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    CHECK_FALSE(bank.lookup("Officer Brooks", Familiarity::First, "hello").has_value());
}

TEST_CASE("a line below threshold misses and is counted as a miss" *
          doctest::skip()) {
    const auto dir = freshBankDir("threshold");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    CHECK_FALSE(bank.lookup("Marge Holloway", Familiarity::First,
                            "what did you think of the election")
                    .has_value());
    CHECK(bank.stats().misses == 1);
    CHECK(bank.stats().hits == 0);
}

TEST_CASE("resetSession clears no-repeat state" * doctest::skip()) {
    const auto dir = freshBankDir("reset");
    writeBank(dir, "marge", kMargeBank);
    LineBank bank(dir, 0.6f);

    const auto first = bank.lookup("Marge Holloway", Familiarity::First, "hello");
    bank.resetSession();
    const auto afterReset = bank.lookup("Marge Holloway", Familiarity::First, "hello");
    REQUIRE(first.has_value());
    REQUIRE(afterReset.has_value());
    CHECK(*first == *afterReset);  // rotation restarted from the top
}
