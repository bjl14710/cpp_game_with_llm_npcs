#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "CharacterParts.hpp"

struct sqlite3;  // vendored external/sqlite3.h; kept out of this header

namespace llm_npc {

// One stored created-character, as the spawn path reads it: the two
// INDEPENDENT records (persona text, look) joined only by character_id.
// This struct is a read-side convenience; the records are stored, saved,
// and loadable separately by design.
struct StoredCharacter {
    std::string characterId;
    std::string personaText;  // .persona format; parse with parsePersonaText
    CharacterLook look;
};

// SQLite-backed persistence for player-created characters, two tables:
//   character_persona(character_id TEXT PRIMARY KEY, persona_text TEXT,
//                     updated_at TEXT)
//   character_look   (character_id TEXT PRIMARY KEY, look_json TEXT,
//                     updated_at TEXT)
// Same degradation contract as ConversationStore: a store that failed to
// open reports ok() == false and every operation becomes a no-op/empty so
// the game runs (this session's creations just don't persist).
class CharacterStore {
   public:
    // Opens (creating file, directories, and schema as needed).
    explicit CharacterStore(const std::filesystem::path& dbPath);
    ~CharacterStore();

    CharacterStore(const CharacterStore&) = delete;
    CharacterStore& operator=(const CharacterStore&) = delete;

    bool ok() const { return db_ != nullptr; }

    // Upserts one record each; the two never touch the other's table.
    bool savePersona(const std::string& characterId, const std::string& personaText);
    bool saveLook(const std::string& characterId, const CharacterLook& look);

    // Single-record loads; false when absent or unreadable.
    bool loadPersona(const std::string& characterId, std::string& personaText) const;
    bool loadLook(const std::string& characterId, CharacterLook& look) const;

    // Every character that has BOTH records intact, for the startup spawn
    // pass. Corrupt/malformed rows are logged to stderr and skipped —
    // never fatal, and one bad character doesn't hide the others.
    std::vector<StoredCharacter> loadAll() const;

   private:
    sqlite3* db_ = nullptr;
};

}  // namespace llm_npc
