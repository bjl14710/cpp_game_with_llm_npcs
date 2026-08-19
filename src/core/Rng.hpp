#pragma once

#include <cstddef>

namespace llm_npc {

// A deterministic, platform-stable, header-free xorshift32.
//
// Promoted here when casting (issue #188) became the THIRD consumer.
// CharacterParts.cpp has the original, written for randomizeLook with the
// rationale that still applies: "std::mt19937 would work too, but this keeps
// the contract obvious and header-free". Mystery.cpp copied it for
// generateMystery. A third copy was worse than a header.
//
// CharacterParts.cpp keeps its own copy deliberately — it is unrelated to this
// change and switching it would mean re-proving that every existing look still
// generates identically.
//
// THE GOTCHA travels with it: xorshift cannot start at 0, so callers construct
// as `Rng rng{seed ? seed : 1u}`. Seed 0 is legitimate caller input everywhere
// this is used, so that guard is load-bearing rather than a nicety.
struct Rng {
    unsigned state;

    unsigned next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform pick in [0, n).
    std::size_t pick(std::size_t n) { return n ? next() % n : 0; }

    // A fraction in [0, 1) with 1/kUnitSteps granularity.
    //
    // Integer-then-divide rather than a direct cast of `next()`: the integer
    // stays exact on every platform and the single division that follows is
    // IEEE-defined. That is what keeps the cross-platform half of the
    // determinism contract.
    double unit() {
        constexpr unsigned kUnitSteps = 100000u;
        return static_cast<double>(next() % kUnitSteps) / kUnitSteps;
    }
};

}  // namespace llm_npc
