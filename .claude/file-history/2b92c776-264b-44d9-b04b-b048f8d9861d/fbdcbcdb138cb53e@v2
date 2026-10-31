#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace llm_npc {

// Append-only review files for the trait rating loop (issue #118).
// Liked replies become CANDIDATE example lines the human curates into
// traits/*.trait BY HAND; disliked ones are logged for later review.
// Nothing here ever feeds back into live prompts — that is the design,
// not a limitation (pinned by test).
class RatingLog {
   public:
    // `dir` is created on first append (saves/ratings/). A directory that
    // cannot be written degrades to no-ops after one stderr line, the same
    // contract as the stores.
    explicit RatingLog(std::filesystem::path dir);

    // One JSONL row each: {ts, persona, traits, player, npc}. Returns
    // false when the log is unavailable.
    bool appendCandidate(const std::string& personaName,
                         const std::vector<std::string>& traitIds,
                         const std::string& playerLine, const std::string& npcLine);
    bool appendRejected(const std::string& personaName,
                        const std::vector<std::string>& traitIds,
                        const std::string& playerLine, const std::string& npcLine);

   private:
    bool append(const char* filename, const std::string& personaName,
                const std::vector<std::string>& traitIds,
                const std::string& playerLine, const std::string& npcLine);

    std::filesystem::path dir_;
    bool warned_ = false;
};

}  // namespace llm_npc
