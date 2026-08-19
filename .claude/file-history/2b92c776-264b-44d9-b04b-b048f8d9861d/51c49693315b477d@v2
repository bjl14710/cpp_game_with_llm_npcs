#include "FactStore.hpp"

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

}  // namespace

FactStore::FactStore(const std::filesystem::path& dbPath) {
    std::error_code ec;
    if (dbPath.has_parent_path()) {
        std::filesystem::create_directories(dbPath.parent_path(), ec);  // best effort
    }
    if (sqlite3_open(dbPath.string().c_str(), &db_) != SQLITE_OK) {
        std::cerr << "[llm_npc] could not open " << dbPath
                  << " — gossip will not persist this session\n";
        sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    if (!exec(db_,
              "CREATE TABLE IF NOT EXISTS facts ("
              "  fact_id TEXT PRIMARY KEY,"
              "  subject TEXT NOT NULL DEFAULT '',"
              "  content TEXT NOT NULL DEFAULT '',"
              "  source TEXT NOT NULL DEFAULT '',"
              "  learned_at REAL NOT NULL DEFAULT 0)") ||
        !exec(db_,
              "CREATE TABLE IF NOT EXISTS knowledge ("
              "  agent TEXT NOT NULL,"
              "  fact_id TEXT NOT NULL,"
              "  PRIMARY KEY (agent, fact_id))")) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

FactStore::~FactStore() {
    if (db_) sqlite3_close(db_);
}

bool FactStore::saveFact(const KnownFact& fact) {
    if (!db_) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "INSERT OR IGNORE INTO facts (fact_id, subject, content,"
                           "  source, learned_at) VALUES (?1, ?2, ?3, ?4, ?5)",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, fact.factId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fact.subject.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fact.content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, fact.source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, fact.learnedAtSeconds);
    const bool okStep = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return okStep;
}

bool FactStore::saveKnowledge(const std::string& agent, const std::string& factId) {
    if (!db_) return false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "INSERT OR IGNORE INTO knowledge (agent, fact_id) "
                           "VALUES (?1, ?2)",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, agent.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, factId.c_str(), -1, SQLITE_TRANSIENT);
    const bool okStep = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return okStep;
}

void FactStore::loadInto(WorldState& state) const {
    if (!db_) return;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT fact_id, subject, content, source, learned_at "
                           "FROM facts ORDER BY rowid",
                           -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            KnownFact fact;
            const auto column = [&](int i) {
                const unsigned char* s = sqlite3_column_text(stmt, i);
                return s ? std::string(reinterpret_cast<const char*>(s)) : std::string();
            };
            fact.factId = column(0);
            fact.subject = column(1);
            fact.content = column(2);
            fact.source = column(3);
            fact.learnedAtSeconds = sqlite3_column_double(stmt, 4);
            if (fact.factId.empty() || fact.subject.empty() || fact.content.empty()) {
                std::cerr << "[llm_npc] corrupt fact row skipped\n";
                continue;
            }
            state.addFact(fact);
        }
        sqlite3_finalize(stmt);
    }
    stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT agent, fact_id FROM knowledge", -1, &stmt,
                           nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* agent = sqlite3_column_text(stmt, 0);
            const unsigned char* factId = sqlite3_column_text(stmt, 1);
            if (agent && factId) {
                state.grantKnowledge(reinterpret_cast<const char*>(agent),
                                     reinterpret_cast<const char*>(factId));
            }
        }
        sqlite3_finalize(stmt);
    }
}

}  // namespace llm_npc
