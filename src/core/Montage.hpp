#pragma once

#include <string>
#include <vector>

#include "Mystery.hpp"
#include "WorldState.hpp"

namespace llm_npc {

// "The clues that mattered" — the contract the win cutscene is built on
// (issue #233, plan .claude/plans/win-cutscene.md).
//
// The cutscene system reads the LIVE world and cannot replay past state. There
// is no recording of what the town looked like at 21:40 on day one, and
// building one would be large. It does not need one: **the places still
// exist.** A flashback is a montage of camera cuts to the LOCATIONS where the
// clues were, each with a caption:
//
//     Bakery Corner  — "She said she was home all evening."
//     Coffee Row     — "Ray saw her here at 9:40."
//     The Plaza      — "The clock says otherwise."
//
// Real locations, real chain, zero new machinery.

// One beat of the argument, in the order the argument runs.
struct ClueStep {
    std::string zoneId;   // where the montage cuts to; a Zones.hpp id
    std::string caption;  // <= 140 chars, fact-shaped
    // The fact this step corresponds to on the bus, or empty for evidence that
    // was never committed. Empty is not an error: it means "this clue existed
    // in the world but nobody ever turned it into knowledge", which is exactly
    // what an undiscovered clue is.
    std::string factId;
    // AUTHORED chain order, 1-based — NOT discovery order.
    //
    // win-cutscene.md calls this the requirement that is cheap to honour now
    // and expensive to retrofit: a template producing an unordered bag of
    // clues cannot drive a montage that reads as reasoning. StorylineClue
    // already carries it and the parser deliberately never re-sorts, so this
    // just carries it through.
    int order = 0;
};

// The ground-truth chain for one match, in authored order.
//
// HOST-ONLY. It is built from MysterySetup, so it is the answer sheet in
// narrative form and must not reach a client before MatchOver. It deliberately
// does NOT mark which step is the decisive one — the montage plays the
// argument, and an argument with the conclusion highlighted is a spoiler.
std::vector<ClueStep> solutionChain(const MysterySetup& setup);

// The chain split by what one agent actually learned.
struct MontagePlan {
    std::vector<ClueStep> found;   // the "I was right" beat
    std::vector<ClueStep> missed;  // the "there was more" beat
};

// Splits `chain` into what `agent` knows and what they never found.
//
// Both lists keep chain order independently, so each beat still reads as an
// argument rather than as a list of things that happened to be nearby.
//
// A step with no factId can never be "found": there is nothing on the bus to
// have learned. That is the honest answer — an undiscovered clue belongs in
// `missed`, which is the beat that exists to show it.
MontagePlan buildMontage(const std::vector<ClueStep>& chain,
                         const WorldState& state, const std::string& agent);

}  // namespace llm_npc
