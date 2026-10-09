#include "TelemetryLog.hpp"

#include <iostream>

#include "json.hpp"
#include "sqlite3.h"

namespace llm_npc {

TelemetryLog::TelemetryLog(const std::filesystem::path& dbPath) {
    // TODO(dialogue-measurement step 4): follow FactStore's constructor
    // exactly — create_directories on the parent, sqlite3_open, then
    // CREATE TABLE IF NOT EXISTS for the schema in the header. On any
    // failure: one stderr line naming the path, sqlite3_close, db_ = nullptr,
    // and the game runs untelemetered. Never throw, never abort.
    (void)dbPath;
    std::cerr << "[llm_npc] telemetry: scaffold only — no turns will be "
                 "recorded (see TelemetryLog.hpp)\n";
}

TelemetryLog::~TelemetryLog() {
    if (db_) sqlite3_close(db_);
}

std::int64_t TelemetryLog::append(const DialogueTurn& turn) {
    // TODO(dialogue-measurement step 4): bound INSERT; return
    // sqlite3_last_insert_rowid. Serialize traitIds and gossipFactIds with
    // nlohmann::json so the Python side can read them back with json.loads.
    (void)turn;
    return 0;
}

bool TelemetryLog::rate(std::int64_t turnId, int rating) {
    // TODO(dialogue-measurement step 4): UPDATE dialogue_turns SET rating = ?
    // WHERE turn_id = ?.
    (void)turnId;
    (void)rating;
    return false;
}

std::string newSessionId() {
    // TODO(dialogue-measurement step 4): see the header.
    return "";
}

}  // namespace llm_npc
