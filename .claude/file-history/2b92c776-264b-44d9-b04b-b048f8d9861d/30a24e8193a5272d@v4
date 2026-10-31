#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "LlmClient.hpp"  // ChatTurn

struct sqlite3;  // vendored external/sqlite3.h; kept out of this header

namespace llm_npc {

// One NPC's persisted memory: the running first-person summary that gets
// injected into the system prompt, plus the raw transcript for debugging.
struct NpcMemory {
    std::string summary;
    std::vector<ChatTurn> transcript;
};

// SQLite-backed persistence for NPC conversations — one row per NPC:
//   conversations(npc_id TEXT PRIMARY KEY, summary TEXT,
//                 transcript_json TEXT, updated_at TEXT)
// Failures are absorbed: a store that failed to open reports ok() == false
// and every operation degrades to "no memory" instead of crashing the game.
class ConversationStore {
   public:
    // Opens (creating file and schema as needed) the database at `dbPath`;
    // parent directories are created too.
    explicit ConversationStore(const std::filesystem::path& dbPath);
    ~ConversationStore();

    ConversationStore(const ConversationStore&) = delete;
    ConversationStore& operator=(const ConversationStore&) = delete;

    // False when the database could not be opened/prepared; the game then
    // runs memoryless for this session.
    bool ok() const { return db_ != nullptr; }

    // Memory for one NPC; empty summary/transcript when absent, unreadable,
    // or corrupt (corrupt rows are logged and skipped, never fatal).
    NpcMemory load(const std::string& npcId) const;

    // Upserts one NPC's memory. False on failure (logged once per session).
    bool save(const std::string& npcId, const std::string& summary,
              const std::vector<ChatTurn>& transcript);

    // Npc ids currently stored (mostly for tests and debugging tools).
    std::vector<std::string> ids() const;

   private:
    sqlite3* db_ = nullptr;
};

}  // namespace llm_npc
