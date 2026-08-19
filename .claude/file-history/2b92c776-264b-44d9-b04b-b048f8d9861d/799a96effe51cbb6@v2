#pragma once

#include <filesystem>
#include <string>

#include "WorldState.hpp"

struct sqlite3;  // vendored external/sqlite3.h; kept out of this header

namespace llm_npc {

// SQLite persistence for the world bus's structured facts and knowledge
// sets — two tables mirroring the in-memory shape:
//   facts(fact_id TEXT PRIMARY KEY, subject, content, source,
//         learned_at REAL)
//   knowledge(agent TEXT, fact_id TEXT, PRIMARY KEY(agent, fact_id))
// Same degradation contract as ConversationStore/CharacterStore: a store
// that failed to open reports ok() == false and gossip simply doesn't
// persist this session.
class FactStore {
   public:
    explicit FactStore(const std::filesystem::path& dbPath);
    ~FactStore();

    FactStore(const FactStore&) = delete;
    FactStore& operator=(const FactStore&) = delete;

    bool ok() const { return db_ != nullptr; }

    // Upserts one fact / one knowledge grant (write-through on commit and
    // propagation — both are tiny rows).
    bool saveFact(const KnownFact& fact);
    bool saveKnowledge(const std::string& agent, const std::string& factId);

    // Rebuilds the fact section of `state` (facts first, then knowledge,
    // so grants never reference a missing fact). Corrupt rows are skipped
    // with a stderr note.
    void loadInto(WorldState& state) const;

   private:
    sqlite3* db_ = nullptr;
};

}  // namespace llm_npc
