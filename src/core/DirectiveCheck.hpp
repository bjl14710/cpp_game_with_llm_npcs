#pragma once

#include <string>
#include <vector>

#include "NpcAction.hpp"

namespace llm_npc {

// SCAFFOLD — interface and contract are settled here; the bodies are TODO.
// Plan: .claude/plans/npc-dialogue-measurement.md, step 2.
//
// "Did this reply obey the contract?" — one answer, two callers.
//
// The game speaks in prose carrying [[MOOD: x]] and [[ACTION: y]] tags, not
// JSON. Telemetry (TelemetryLog) and the evaluation harness
// (tools/eval_dialogue.py, through `persona_prompt --parse`) both need to
// classify a reply as contract-obeying or not, and they must never disagree.
// That is why this is a core function rather than a rule written twice.
//
// WHY THIS CANNOT JUST CALL parseDirectives AND LOOK AT THE RESULT.
// parseDirectives is deliberately forgiving: duplicate mood tags are absorbed
// ("last recognized mood wins"), unknown keywords are erased, and single- and
// double-bracket forms both parse. Every one of those is correct for gameplay
// — a player must never see a raw tag — and every one of them destroys the
// evidence a measurement needs. Two MOOD tags in one reply is a schema
// violation that parseDirectives makes invisible by design.
//
// So the check COUNTS tags before they are stripped. It must do that by
// sharing NpcAction.hpp's bracket walk, not by copying it — see the TODO on
// checkReply for the required refactor. A second bracket parser in this
// codebase is the same drift tools/eval_lines.py gate 1 already ruled out.

// The verdict on one raw reply.
struct DirectiveVerdict {
    // False when any rule below was broken. This is the `schema_valid` column
    // in dialogue_turns and the schema-validity rate in the scorecard.
    bool valid = false;

    // Machine-readable reasons, one per broken rule, in the order checked:
    //   "empty_reply"      nothing but whitespace once tags are stripped
    //   "missing_mood"     no MOOD tag at all (the contract says every reply)
    //   "duplicate_mood"   two or more recognized MOOD tags
    //   "unknown_mood"     a MOOD tag whose keyword no synonym maps
    //   "duplicate_action" two or more recognized ACTION tags
    //   "unknown_action"   an ACTION tag whose keyword no synonym maps
    //   "residual_tag"     "[[" or "]]" survived into the spoken text
    // Stored as a JSON array so a scorecard can group by reason rather than
    // reporting one undifferentiated failure count.
    std::vector<std::string> reasons;

    // What a player would actually have seen: the reply with every recognized
    // tag stripped and the whitespace seams collapsed, exactly as Npc.cpp
    // produces it.
    std::string spokenText;

    // What the game would have acted on.
    Directives directives;

    // Raw tag census, kept because the reasons above compress it. A run whose
    // validity rate fell needs to know whether models started emitting two
    // moods or stopped emitting any.
    int moodTagCount = 0;
    int actionTagCount = 0;
    int unknownTagCount = 0;
};

// Classifies one raw model reply. `raw` is the reply exactly as the backend
// returned it, tags intact — never the post-parseDirectives text, which has
// already lost what this measures.
//
// TODO(dialogue-measurement step 2): implement in this order.
//   1. FIRST extract a shared scanner into NpcAction.hpp:
//        template <typename Fn>
//        void forEachDirectiveTag(const std::string& reply, Fn&& onTag);
//      carrying the existing bracket walk (open/inner/close/end, the "[[" and
//      "]]" forms, the kind:keyword split, the space->underscore
//      normalization) and reporting each tag's byte range, kind and keyword
//      WITHOUT mutating anything. Then rewrite parseDirectives in terms of it
//      so the walk exists once. Its behaviour must not change: the existing
//      cases in tests/test_npc_action.cpp are the contract and stay green
//      untouched.
//   2. Count mood/action/unknown tags with that scanner.
//   3. Copy `raw`, run parseDirectives on the copy for spokenText and
//      directives. Do not re-derive either.
//   4. Apply the rules above in the documented order and set `valid`.
//   5. Check residual "[[" / "]]" in spokenText LAST — parseDirectives leaves
//      non-directive brackets ("[sigh]") alone on purpose, so only the doubled
//      forms count as residue.
DirectiveVerdict checkReply(const std::string& raw);

// The verdict as the JSON that goes into dialogue_turns.directives_json and
// into `persona_prompt --parse` output:
//   {"valid":bool,"reasons":[...],"action":"...","mood":"...",
//    "has_mood":bool,"mood_tags":n,"action_tags":n,"unknown_tags":n}
//
// TODO(dialogue-measurement step 2): the action/mood name tables already exist
// in tools/persona_prompt.cpp. Move them here so the tool and the store cannot
// print different names for the same enum, and have persona_prompt call this.
std::string verdictToJson(const DirectiveVerdict& verdict);

}  // namespace llm_npc
