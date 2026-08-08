#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "TopicMatcher.hpp"

namespace llm_npc {

// Authored NPC replies for recurring conversation topics, served locally so
// a common line costs microseconds instead of a 2.7 s round trip to the
// model (plan: npc-line-bank, step 2; baseline in bench/REPORT.md).
//
// Content lives in banks/<persona>.bank in the same `key = value` shape as
// personas/*.persona and traits/*.trait, because the whole point is that a
// night of generated content reviews as a normal text diff in a PR. See
// banks/README.md for the format.

// How well this NPC knows the player. A banked greeting must not greet a
// returning player like a stranger — that is the one place a cached line can
// contradict persisted ConversationStore memory. Every other topic is
// authored memory-agnostic (enforced by rubric gate 5 in tools/eval_lines.py)
// and declares Any.
//
// A topic declaring Any is servable to either caller; lookup() itself is
// always passed First or Returning, never Any.
enum class Familiarity { Any, First, Returning };

// One topic: the phrasings that reach it and the interchangeable replies
// that answer it.
struct BankedTopic {
    std::string topicId;
    Familiarity familiarity = Familiarity::Any;
    std::vector<std::string> triggers;  // player phrasings
    std::vector<std::string> replies;   // 3-5 variants, [[MOOD:]] tags included
};

// Everything one persona can serve.
struct PersonaBank {
    std::string speakerId;  // matches Persona::name, e.g. "Marge Holloway"
    std::vector<BankedTopic> topics;
};

// Hit accounting, so step 8 can flip `line_bank = on` with a measured number
// in the commit message rather than a claim.
struct BankStats {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
};

// Parses one .bank file. Returns nullopt and appends a named message to
// `errors` for a malformed file — one bad bank must never stop the other nine
// loading, same contract as loadAllPersonas/loadAllTraits.
std::optional<PersonaBank> parseBankFile(const std::filesystem::path& path,
                                         std::vector<std::string>* errors = nullptr);

class LineBank {
   public:
    // Loads every banks/*.bank under `dir`. `threshold` is the confidence a
    // match must clear to serve, from config/llm.cfg's line_bank_threshold.
    //
    // A missing directory, an unreadable file, or a malformed one degrades to
    // "never hits" after one stderr line — the same failure contract as
    // ConversationStore and RatingLog. A broken bank must leave the game no
    // worse than not having one.
    explicit LineBank(std::filesystem::path dir, float threshold);

    // False when nothing loaded; lookup() then always misses.
    bool ok() const { return !banks_.empty(); }

    // The banked reply for this player line, or nullopt to fall through to
    // the live model. Advances rotation state on a hit.
    //
    // Picks the topic via bestTopic() over the speaker's topics filtered by
    // familiarity, then serves the least-recently-used reply variant. No
    // variant repeats until every variant for that topic has been used this
    // session — an NPC that answers "hello" identically twice in one
    // conversation is worse than a slow one.
    std::optional<std::string> lookup(const std::string& speakerId,
                                      Familiarity familiarity,
                                      const std::string& playerLine);

    // Clears per-session no-repeat state. Call when a match or play session
    // ends; rotation is per-session by design, not per-process.
    void resetSession();

    // Parse errors collected at construction, for tests and the nightly job.
    const std::vector<std::string>& errors() const { return errors_; }

    const BankStats& stats() const { return stats_; }

    // Loaded banks, for tests and tooling.
    const std::vector<PersonaBank>& banks() const { return banks_; }

   private:
    std::filesystem::path dir_;
    float threshold_ = 0.f;
    std::vector<PersonaBank> banks_;
    std::vector<std::string> errors_;
    BankStats stats_;

    // Rotation cursor keyed by speakerId + '\x1f' + topicId. Unit separator
    // because a persona name may contain spaces but never a control byte.
    std::unordered_map<std::string, std::size_t> nextVariant_;
};

}  // namespace llm_npc
