#include "ConversationStore.hpp"

#include <iostream>

#include "json.hpp"
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

}  // namespace

ConversationStore::ConversationStore(const std::filesystem::path& dbPath) {
    std::error_code ec;
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path(), ec);  // best effort
    }
    if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[llm_npc] could not open " << dbPath
                  << " — NPCs will not remember this session\n";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    if (!exec(db_,
              "CREATE TABLE IF NOT EXISTS conversations ("
              "  npc_id TEXT PRIMARY KEY,"
              "  summary TEXT NOT NULL DEFAULT '',"
              "  transcript_json TEXT NOT NULL DEFAULT '[]',"
              "  updated_at TEXT NOT NULL DEFAULT '')")) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

ConversationStore::~ConversationStore() {
    if (db_) sqlite3_close(db_);
}

NpcMemory ConversationStore::load(const std::string& npcId) const {
    NpcMemory memory;
    if (!db_) return memory;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT summary, transcript_json FROM conversations "
                           "WHERE npc_id = ?1",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return memory;
    }
    sqlite3_bind_text(stmt, 1, npcId.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (const unsigned char* s = sqlite3_column_text(stmt, 0)) {
            memory.summary = reinterpret_cast<const char*>(s);
        }
        const unsigned char* t = sqlite3_column_text(stmt, 1);
        const nlohmann::json turns = nlohmann::json::parse(
            t ? reinterpret_cast<const char*>(t) : "[]", nullptr,
            /*allow_exceptions=*/false);
        if (turns.is_array()) {
            for (const auto& turn : turns) {
                memory.transcript.push_back(ChatTurn{turn.value("role", "user"),
                                                     turn.value("content", "")});
            }
        } else {
            std::cerr << "[llm_npc] corrupt transcript for \"" << npcId
                      << "\" — starting fresh\n";
        }
    }
    sqlite3_finalize(stmt);
    return memory;
}

bool ConversationStore::save(const std::string& npcId, const std::string& summary,
                             const std::vector<ChatTurn>& transcript) {
    if (!db_) return false;

    nlohmann::json turns = nlohmann::json::array();
    for (const auto& turn : transcript) {
        turns.push_back({{"role", turn.role}, {"content", turn.content}});
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "INSERT INTO conversations (npc_id, summary, transcript_json,"
                           "                           updated_at) "
                           "VALUES (?1, ?2, ?3, datetime('now')) "
                           "ON CONFLICT(npc_id) DO UPDATE SET"
                           "  summary = excluded.summary,"
                           "  transcript_json = excluded.transcript_json,"
                           "  updated_at = excluded.updated_at",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[llm_npc] sqlite prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    sqlite3_bind_text(stmt, 1, npcId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, summary.c_str(), -1, SQLITE_TRANSIENT);
    const std::string json = turns.dump();
    sqlite3_bind_text(stmt, 3, json.c_str(), -1, SQLITE_TRANSIENT);
    const bool okStep = sqlite3_step(stmt) == SQLITE_DONE;
    if (!okStep) {
        std::cerr << "[llm_npc] sqlite save failed: " << sqlite3_errmsg(db_) << "\n";
    }
    sqlite3_finalize(stmt);
    return okStep;
}

std::vector<std::string> ConversationStore::ids() const {
    std::vector<std::string> out;
    if (!db_) return out;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT npc_id FROM conversations ORDER BY npc_id", -1,
                           &stmt, nullptr) != SQLITE_OK) {
        return out;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (const unsigned char* s = sqlite3_column_text(stmt, 0)) {
            out.emplace_back(reinterpret_cast<const char*>(s));
        }
    }
    sqlite3_finalize(stmt);
    return out;
}

}  // namespace llm_npc
