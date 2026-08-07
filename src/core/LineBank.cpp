#include "LineBank.hpp"

namespace llm_npc {

// Scaffolded stubs (plan: npc-line-bank, step 2). Construction loads
// nothing, so ok() is false and lookup() always misses — the game runs
// exactly as it does today. Fill these in and un-skip the cases in
// tests/test_line_bank.cpp in the same commit.

std::optional<PersonaBank> parseBankFile(const std::filesystem::path&,
                                         std::vector<std::string>*) {
    return std::nullopt;  // TODO(linebank step 2)
}

LineBank::LineBank(std::filesystem::path dir, float threshold)
    : dir_(std::move(dir)), threshold_(threshold) {
    // TODO(linebank step 2): iterate dir_ for *.bank, parseBankFile each,
    // collect errors, and emit ONE stderr line if the directory itself is
    // missing or unreadable (not one per file — a silent game is better than
    // a noisy one, and the errors() vector carries the detail).
}

std::optional<std::string> LineBank::lookup(const std::string&, Familiarity,
                                            const std::string&) {
    // TODO(linebank step 2): resolve the speaker's bank, filter its topics by
    // familiarity, match with bestTopic(..., threshold_), then serve the
    // least-recently-used variant.
    //
    // Nothing is loaded yet, so there are no topics to match and always
    // missing is the CORRECT answer here, not a placeholder one.
    static_cast<void>(threshold_);  // consumed by bestTopic() in step 2
    ++stats_.misses;
    return std::nullopt;
}

void LineBank::resetSession() {
    nextVariant_.clear();
}

}  // namespace llm_npc
