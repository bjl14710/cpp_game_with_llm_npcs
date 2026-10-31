#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;  // vendored external/sqlite3.h; kept out of this header

namespace llm_npc {

// SCAFFOLD — schema and interface are settled here; the bodies are TODO.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 4. This is LOOP A.
//
// Passive, always-on capture of every NPC dialogue turn. NOTHING HERE SCORES
// ANYTHING. Loop A records what happened; Loop B (tools/eval_dialogue.py)
// decides whether it was good, against a frozen golden set. Keeping the two
// apart is the whole design: a capture path that also judged would make the
// judgement a function of whatever the player happened to do that evening.
//
// This is DIAGNOSTIC DATA, NOT GAME STATE. It lives in saves/telemetry.sqlite3
// — a separate database from facts.sqlite3, conversations.sqlite3 and the
// character store — so that deleting it can never cost a player anything, and
// so a schema change here needs no migration thought. saves/ is already
// gitignored, so nothing extra is needed to keep it out of commits.
//
// FAILURE CONTRACT: identical to ConversationStore, RatingLog, LineBank,
// Storyline and Cutscene. A database that cannot be opened reports ok() ==
// false, every call degrades to a no-op after one stderr line, and the game is
// unaffected. Telemetry must never be able to break a session.
//
// DEV BUILDS ONLY — but the GUARD IS NOT HERE. This class always compiles, so
// that `make -C tests test` (which globs src/core/*.cpp and knows nothing
// about CMake options) can test it in the Linux container. The
// LLM_NPC_DEV_TOOLS guard belongs at the CALL SITE in src/app/main.cpp. See
// step 5 of the plan.
//
// MULTIPLAYER: a guest's conversations are served host-side, so a guest
// process records nothing. The call site reuses the `!joined` guard the F1/F2
// rating path already applies. No NetMessage type is added for telemetry, and
// a test pins that.

// One dialogue turn, as recorded. Field names match the SQLite columns.
struct DialogueTurn {
    // --- when and where -------------------------------------------------
    std::int64_t ts = 0;         // unix seconds
    std::string sessionId;       // one run of the game; groups a play session
    std::string npcId;           // persona name — the id ConversationStore uses
    std::string personaFile;     // personas/*.persona this NPC was loaded from

    // --- what produced it ------------------------------------------------
    std::string model;           // resolved model tag, e.g. "qwen3:8b"
    std::string provider;        // "ollama" | "openrouter" | ...
    std::string promptVersion;   // kPromptTemplateVersion
    std::string promptHash;      // promptFingerprint() of the rendered prompt
    std::vector<std::string> traitIds;  // active traits, stored as a JSON array

    // --- the secrecy state at that moment ---------------------------------
    // The question Loop B exists to answer needs to know what the model was
    // holding when it answered. `roleId` is empty outside a detective match.
    std::string roleId;
    // True when renderRoleBlock produced a non-empty block — i.e. the secret
    // actually reached the system prompt. This is what "gated open" means
    // here; there is no other gate.
    bool secretOpen = false;
    // The secret text as sent. Recorded because a golden probe asserting
    // "this token must not appear in the reply" needs the token, and because
    // a leak is only provable against what was actually in the prompt.
    // Local, gitignored, diagnostic — see the class comment.
    std::string secretToken;
    // KnownFact ids in the gossip section, as a JSON array. Gossip is the
    // other channel by which an NPC can know something it should not say.
    std::vector<std::string> gossipFactIds;

    // --- the exchange ------------------------------------------------------
    std::string playerLine;
    std::string rawOutput;       // verbatim, tags intact — never post-strip
    std::string spokenText;      // what the player saw
    std::string directivesJson;  // verdictToJson(); see DirectiveCheck.hpp
    bool schemaValid = false;
    std::string schemaErrors;    // JSON array of DirectiveVerdict::reasons

    // --- cost ---------------------------------------------------------------
    // -1 means the backend did not report it. Ollama reports
    // prompt_eval_count/eval_count; OpenAI-compatible servers report `usage`
    // only when they choose to. NEVER substitute a character count here — the
    // char fields below exist so the honest number has somewhere to go.
    int promptTokens = -1;
    int replyTokens = -1;
    int promptChars = 0;
    int replyChars = 0;
    int latencyMs = -1;          // submit to final reply
    int ttftMs = -1;             // submit to first streamed delta

    // --- flags --------------------------------------------------------------
    // Served from banks/*.bank without touching a model. MUST be excluded from
    // every latency statistic: a banked reply's timing measures LineBank's
    // deliberate line_bank_cps pacing, not a model.
    bool banked = false;
    // The player left the conversation before this reply landed. Still a real
    // turn and still recorded; a scorecard may want to exclude it.
    bool abandoned = false;
    // Out-of-band dev rating: 1 (F1, good), -1 (F2, bad), 0 (unrated).
    // Written by a later update(), not at insert time — the rating happens
    // after the reply is already on screen.
    int rating = 0;
};

// SQLite-backed dialogue telemetry — one row per turn:
//   dialogue_turns(turn_id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER,
//                  session_id TEXT, npc_id TEXT, persona_file TEXT,
//                  model TEXT, provider TEXT, prompt_version TEXT,
//                  prompt_hash TEXT, trait_ids TEXT, role_id TEXT,
//                  secret_open INTEGER, secret_token TEXT,
//                  gossip_fact_ids TEXT, player_line TEXT, raw_output TEXT,
//                  spoken_text TEXT, directives_json TEXT,
//                  schema_valid INTEGER, schema_errors TEXT,
//                  prompt_tokens INTEGER, reply_tokens INTEGER,
//                  prompt_chars INTEGER, reply_chars INTEGER,
//                  latency_ms INTEGER, ttft_ms INTEGER, banked INTEGER,
//                  abandoned INTEGER, rating INTEGER)
class TelemetryLog {
   public:
    // Opens (creating file and schema as needed) the database at `dbPath`;
    // parent directories are created too.
    explicit TelemetryLog(const std::filesystem::path& dbPath);
    ~TelemetryLog();

    TelemetryLog(const TelemetryLog&) = delete;
    TelemetryLog& operator=(const TelemetryLog&) = delete;

    // False when the database could not be opened/prepared; every call then
    // no-ops and the session runs without telemetry.
    bool ok() const { return db_ != nullptr; }

    // Appends one turn. Returns its turn_id, or 0 on failure. The id is the
    // handle rate() needs, which is why this returns an id rather than a bool.
    //
    // TODO(dialogue-measurement step 4): implement. Bind every field; do not
    // build SQL by concatenation — player lines and model replies contain
    // quotes, and this is the one store whose input is entirely untrusted
    // text. sqlite3_prepare_v2 + sqlite3_bind_text, as FactStore does.
    std::int64_t append(const DialogueTurn& turn);

    // Attaches an out-of-band dev rating to an already-recorded turn.
    // 1 good, -1 bad, 0 clears. False when the row is unknown or the store is
    // unavailable.
    //
    // TODO(dialogue-measurement step 4): a single UPDATE by turn_id.
    bool rate(std::int64_t turnId, int rating);

    // Turns recorded this session, for the debug overlay's counter.
    //
    // TODO(dialogue-measurement step 4): a plain counter incremented in
    // append(); do NOT run SELECT COUNT(*) per frame.
    std::int64_t countThisSession() const { return appended_; }

   private:
    sqlite3* db_ = nullptr;
    std::int64_t appended_ = 0;
};

// A random-enough session id: the process start time in hex plus four hex
// digits from the address of a stack local. Not a UUID and does not need to
// be — it only has to distinguish this run from the previous one in the same
// database, and adding a UUID dependency for that would fail CLAUDE.md.
//
// TODO(dialogue-measurement step 4): implement.
std::string newSessionId();

}  // namespace llm_npc
