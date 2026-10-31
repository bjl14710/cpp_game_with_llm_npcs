#pragma once

#include <string>

#include "NpcAction.hpp"

namespace llm_npc {

// The arresting officer's one hand-written line, chosen by her mood (plan:
// .claude/plans/cutscene-authored-scenes.md; design "Cutscenes.dc.html",
// Part 4 "Arrest").
//
// WHY A LOOKUP AND NOT AN LLM CALL. The arrest fires the instant an officer
// catches you, and a cutscene that has to wait on a model is a cutscene that
// starts late or not at all. Three hand-written lines cost nothing, never
// fail, and never say something the character would not.
//
// WHY THREE LINES FOR SIX MOODS. The design specified "three variants chosen
// by NpcMood". NpcMood carries six values, so four of them map onto the
// neutral line. That mapping is a DECISION, not an oversight -- Sad,
// Embarrassed and Surprised are not moods an officer performs at an arrest,
// and inventing lines for them would put words in her mouth that the design
// never asked for. tests/test_arrest_line.cpp pins it so a later reader does
// not "fix" the gap.
//
// Lines are ASCII only. The built-in bitmap font has glyphs for 32-126 and
// renders anything else as '?' -- see src/core/AsciiText.hpp, which documents
// the mechanism at length.

// Returns the line for `mood`. Total: every NpcMood value yields a non-empty
// string, so a caller never has to handle a missing line.
inline std::string arrestLineFor(NpcMood mood) {
    // TODO(#cutscene-1): return the three lines below, per the plan's table.
    //
    //   Angry -> "You can ask your questions from a bench. Think about the tone."
    //   Happy -> "No hard feelings. Sit down, cool off, we'll talk after."
    //   Neutral, Sad, Embarrassed, Surprised
    //         -> "You can ask your questions from a bench. Thirty seconds."
    //
    // Write it as a switch over NpcMood with no `default:` label, so that
    // adding a seventh mood later is a COMPILER WARNING here rather than a
    // silent fallthrough to the neutral line. That is the whole reason this
    // is a switch and not a map.
    (void)mood;
    return std::string();
}

}  // namespace llm_npc
