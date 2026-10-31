// Tests for ConversationStore — SQLite-backed NPC memory that must survive
// process restarts (plan: .claude/plans/npc-memory-and-model.md).
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "ConversationStore.hpp"
#include "doctest.h"
#include "sqlite3.h"

namespace fs = std::filesystem;
using llm_npc::ChatTurn;
using llm_npc::ConversationStore;
using llm_npc::NpcMemory;

namespace {

// Unique temp path per test run; removed on destruction.
struct TempDb {
    fs::path path;
    TempDb() {
        path = fs::temp_directory_path() /
               ("llm_npc_store_" + std::to_string(std::rand())) / "conv.sqlite3";
    }
    ~TempDb() { fs::remove_all(path.parent_path()); }
};

}  // namespace

TEST_CASE("ConversationStore round-trips summary and transcript") {
    TempDb tmp;
    ConversationStore store(tmp.path);
    REQUIRE(store.ok());

    const std::vector<ChatTurn> transcript = {
        {"user", "hello Marge"},
        {"assistant", "Well hello there, sugar!"},
    };
    CHECK(store.save("Marge Holloway", "The player greeted me warmly.", transcript));

    const NpcMemory memory = store.load("Marge Holloway");
    CHECK(memory.summary == "The player greeted me warmly.");
    REQUIRE(memory.transcript.size() == 2);
    CHECK(memory.transcript[0].role == "user");
    CHECK(memory.transcript[1].content == "Well hello there, sugar!");
}

TEST_CASE("ConversationStore persists across reopen (restart survival)") {
    TempDb tmp;
    {
        ConversationStore store(tmp.path);
        REQUIRE(store.ok());
        CHECK(store.save("Officer Dana Brooks", "I warned the player once.", {}));
    }  // destroyed — process restart equivalent
    ConversationStore reopened(tmp.path);
    REQUIRE(reopened.ok());
    CHECK(reopened.load("Officer Dana Brooks").summary == "I warned the player once.");
    CHECK(reopened.ids() == std::vector<std::string>{"Officer Dana Brooks"});
}

TEST_CASE("ConversationStore upserts instead of duplicating") {
    TempDb tmp;
    ConversationStore store(tmp.path);
    CHECK(store.save("Gus Romano", "First impression.", {}));
    CHECK(store.save("Gus Romano", "Updated impression.", {{"user", "hi"}}));
    CHECK(store.ids().size() == 1);
    const NpcMemory memory = store.load("Gus Romano");
    CHECK(memory.summary == "Updated impression.");
    CHECK(memory.transcript.size() == 1);
}

TEST_CASE("ConversationStore returns empty memory for unknown NPCs") {
    TempDb tmp;
    ConversationStore store(tmp.path);
    const NpcMemory memory = store.load("Nobody In Particular");
    CHECK(memory.summary.empty());
    CHECK(memory.transcript.empty());
}

TEST_CASE("ConversationStore degrades to memoryless when the path is unusable") {
    // A directory path that cannot be a file: open fails, ok() is false, and
    // every operation is a safe no-op (the game must keep running).
    ConversationStore store(fs::temp_directory_path());
    CHECK_FALSE(store.ok());
    CHECK_FALSE(store.save("Anyone", "s", {}));
    CHECK(store.load("Anyone").summary.empty());
    CHECK(store.ids().empty());
}

TEST_CASE("ConversationStore survives a corrupt transcript row") {
    TempDb tmp;
    {
        ConversationStore store(tmp.path);
        REQUIRE(store.ok());
        CHECK(store.save("Yuki Tanaka", "Met at the fountain.", {{"user", "hey"}}));
    }
    // Corrupt the row directly (hand-edited/legacy-db case) with the raw
    // sqlite API — the store itself can only ever write valid JSON.
    {
        sqlite3* db = nullptr;
        REQUIRE(sqlite3_open(tmp.path.string().c_str(), &db) == SQLITE_OK);
        REQUIRE(sqlite3_exec(db,
                             "UPDATE conversations SET transcript_json = 'not json' "
                             "WHERE npc_id = 'Yuki Tanaka'",
                             nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db);
    }
    ConversationStore store(tmp.path);
    const NpcMemory memory = store.load("Yuki Tanaka");
    CHECK(memory.transcript.empty());                    // corrupt part dropped
    CHECK(memory.summary == "Met at the fountain.");     // intact part kept
}
