#include "CharacterStore.hpp"

#include <iostream>

#include "sqlite3.h"

namespace llm_npc {

namespace {

// Executes one parameter-less statement; true on success.
bool exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "[llm_npc] sqlite: " << (err ? err : "unknown error") << "\n";
        sqlite3_free(err);
        return false;
    }
    return true;
}

// Upserts one (character_id, value) row into `table`.`column`.
bool upsert(sqlite3* db, const char* table, const char* column,
            const std::string& characterId, const std::string& value) {
    if (!db) return false;
    const std::string sql =
        std::string("INSERT INTO ") + table + " (character_id, " + column +
        ", updated_at) VALUES (?1, ?2, datetime('now')) "
        "ON CONFLICT(character_id) DO UPDATE SET " +
        column + " = excluded." + column + ", updated_at = excluded.updated_at";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[llm_npc] sqlite prepare failed: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, characterId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    const bool okStep = sqlite3_step(stmt) == SQLITE_DONE;
    if (!okStep) {
        std::cerr << "[llm_npc] sqlite save failed: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
    return okStep;
}

// Reads one (character_id → value) cell; false when absent/unreadable.
bool selectOne(sqlite3* db, const char* table, const char* column,
               const std::string& characterId, std::string& value) {
    if (!db) return false;
    const std::string sql = std::string("SELECT ") + column + " FROM " + table +
                            " WHERE character_id = ?1";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, characterId.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (const unsigned char* s = sqlite3_column_text(stmt, 0)) {
            value = reinterpret_cast<const char*>(s);
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

}  // namespace

CharacterStore::CharacterStore(const std::filesystem::path& dbPath) {
    std::error_code ec;
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path(), ec);  // best effort
    }
    if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[llm_npc] could not open " << dbPath
                  << " — created characters will not persist this session\n";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    // Two independent tables joined only by character_id — persona and look
    // never share a row (the ticket's independence requirement, enforced by
    // schema rather than convention).
    if (!exec(db_,
              "CREATE TABLE IF NOT EXISTS character_persona ("
              "  character_id TEXT PRIMARY KEY,"
              "  persona_text TEXT NOT NULL DEFAULT '',"
              "  updated_at TEXT NOT NULL DEFAULT '')") ||
        !exec(db_,
              "CREATE TABLE IF NOT EXISTS character_look ("
              "  character_id TEXT PRIMARY KEY,"
              "  look_json TEXT NOT NULL DEFAULT '',"
              "  updated_at TEXT NOT NULL DEFAULT '')")) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

CharacterStore::~CharacterStore() {
    if (db_) sqlite3_close(db_);
}

bool CharacterStore::savePersona(const std::string& characterId,
                                 const std::string& personaText) {
    return upsert(db_, "character_persona", "persona_text", characterId, personaText);
}

bool CharacterStore::saveLook(const std::string& characterId,
                              const CharacterLook& look) {
    return upsert(db_, "character_look", "look_json", characterId, look.toJson());
}

bool CharacterStore::loadPersona(const std::string& characterId,
                                 std::string& personaText) const {
    return selectOne(db_, "character_persona", "persona_text", characterId,
                     personaText);
}

bool CharacterStore::loadLook(const std::string& characterId,
                              CharacterLook& look) const {
    std::string json;
    if (!selectOne(db_, "character_look", "look_json", characterId, json)) {
        return false;
    }
    if (!CharacterLook::fromJson(json, look)) {
        std::cerr << "[llm_npc] corrupt look for \"" << characterId
                  << "\" — skipping\n";
        return false;
    }
    return true;
}

std::vector<StoredCharacter> CharacterStore::loadAll() const {
    std::vector<StoredCharacter> out;
    if (!db_) return out;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT character_id FROM character_persona "
                           "ORDER BY character_id",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return out;
    }
    std::vector<std::string> ids;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (const unsigned char* s = sqlite3_column_text(stmt, 0)) {
            ids.emplace_back(reinterpret_cast<const char*>(s));
        }
    }
    sqlite3_finalize(stmt);

    for (const std::string& id : ids) {
        StoredCharacter c;
        c.characterId = id;
        if (!loadPersona(id, c.personaText) || c.personaText.empty()) {
            std::cerr << "[llm_npc] character \"" << id
                      << "\" has no persona — skipping\n";
            continue;
        }
        if (!loadLook(id, c.look)) {
            std::cerr << "[llm_npc] character \"" << id
                      << "\" has no readable look — skipping\n";
            continue;
        }
        out.push_back(std::move(c));
    }
    return out;
}

}  // namespace llm_npc
