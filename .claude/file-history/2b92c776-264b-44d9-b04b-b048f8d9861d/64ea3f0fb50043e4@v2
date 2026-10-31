// Tests for FactStore: facts + knowledge round-trip and degradation.
#include <filesystem>
#include <string>

#include "FactStore.hpp"
#include "WorldState.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

struct TempDb {
    std::filesystem::path path;
    explicit TempDb(const char* name) {
        path = std::filesystem::temp_directory_path() /
               (std::string("llm_npc_facttest_") + name + ".sqlite3");
        std::filesystem::remove(path);
    }
    ~TempDb() { std::filesystem::remove(path); }
};

KnownFact sampleFact() {
    KnownFact fact;
    fact.factId = "fact_1";
    fact.subject = "bakery_fire";
    fact.content = "The oven caught fire.";
    fact.source = "player";
    fact.learnedAtSeconds = 36000.0;
    return fact;
}

}  // namespace

TEST_CASE("facts and knowledge round-trip into a fresh WorldState") {
    TempDb db("roundtrip");
    {
        FactStore store(db.path);
        REQUIRE(store.ok());
        CHECK(store.saveFact(sampleFact()));
        CHECK(store.saveKnowledge("Marge", "fact_1"));
        CHECK(store.saveKnowledge("player", "fact_1"));
        // Idempotent upserts (write-through calls repeat these often).
        CHECK(store.saveFact(sampleFact()));
        CHECK(store.saveKnowledge("Marge", "fact_1"));
    }
    FactStore reopened(db.path);
    WorldState state;
    reopened.loadInto(state);
    REQUIRE(state.facts().size() == 1);
    CHECK(state.facts()[0].subject == "bakery_fire");
    CHECK(state.facts()[0].learnedAtSeconds == doctest::Approx(36000.0));
    CHECK(state.knows("Marge", "fact_1"));
    CHECK(state.knows("player", "fact_1"));
    CHECK_FALSE(state.knows("Hal", "fact_1"));
}

TEST_CASE("unopenable database degrades to session-only gossip") {
    const auto dir = std::filesystem::temp_directory_path() / "llm_npc_facttest_dir";
    std::filesystem::create_directories(dir);
    FactStore store(dir);
    CHECK_FALSE(store.ok());
    CHECK_FALSE(store.saveFact(sampleFact()));
    CHECK_FALSE(store.saveKnowledge("Marge", "fact_1"));
    WorldState state;
    store.loadInto(state);  // must not crash
    CHECK(state.facts().empty());
    std::filesystem::remove_all(dir);
}
