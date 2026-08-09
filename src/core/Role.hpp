#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Trait.hpp"  // TraitExchange — a role's examples are the same shape

namespace llm_npc {

// A storyline role as DATA (plan: role-layer). When an NPC is cast as the
// killer, a witness or a red herring, their behaviour has to change without
// breaking who they are: a gossipy baker who is the killer should still be a
// gossipy baker, just one who lies about Tuesday night.
//
// Mirrors traits/*.trait in every way that matters — same key=value format,
// same skipped-with-a-named-error contract, same TraitExchange for examples.
// Prompt and content work, NOT finetuning.
//
// FOUR THINGS WERE MEASURED against qwen3:8b before this was designed, and
// three of them are the reason the API looks the way it does:
//
//   1. Holding a lie is NOT the problem. Zero confessions and zero fourth-wall
//      breaks across ten guilty turns under direct pressure. The model lies
//      comfortably, so nothing here needs to help it.
//   2. The role block OVERWHELMS the persona. Marge — "warm, gossipy, fiercely
//      proud of her sourdough, motherly" — went hostile on turn 1 to a plain
//      "where were you Tuesday night". That is the failure this plan exists to
//      prevent.
//   3. THE MOOD TAG IDENTIFIES THE KILLER. Innocent Marge laughs when accused
//      ([[MOOD: amused]]); guilty Marge goes [[MOOD: angry]] on turn one. Mood
//      drives the rendered face, so a player could ask all twenty residents one
//      question and watch for the angry one — the mystery solved in twenty
//      questions with zero reasoning. Worse in multiplayer, because
//      NpcMoodUpdate is broadcast to every client.
//   4. Early placement CORRUPTS THE ACTION PROTOCOL. With the role block before
//      "Stay in character", the killer emitted [[ACTION: call_police]] on three
//      of five turns. The murderer summoning the police is both a bug and a
//      second tell.

struct RoleDef {
    std::string id;    // filename stem, e.g. "killer"
    std::string name;  // display name; never shown to a player
    std::vector<std::string> directives;

    // THE ANTI-TELL LINE, and the reason finding 3 does not sink the mode.
    // An explicit instruction to hold baseline warmth, because without one the
    // model defaults to hostile and gives the game away.
    //
    // REQUIRED, not optional: a role file without one must FAIL TO LOAD. An
    // optional field here would be silently omitted by exactly the author who
    // most needs it, and the resulting leak is invisible without running the
    // model.
    std::string demeanour;

    std::vector<TraitExchange> examples;
};

// Every roles/*.role file in `dir`, sorted by filename so load order is
// reproducible. A malformed file is skipped with a named error appended to
// `errors` and the others still load — the same contract loadAllTraits and
// loadAllPersonas hold.
std::vector<RoleDef> loadAllRoles(const std::filesystem::path& dir,
                                  std::vector<std::string>* errors = nullptr);

// The prompt section for a cast NPC.
//
// PLACEMENT IS THE WHOLE DESIGN, and it is measured rather than argued.
// Persona.hpp inserts a section immediately before "ACTIONS: ", giving:
//
//   1. identity, personality, style, knowledge, extra directives
//   2. "Stay in character. Never break the fourth wall…"
//   3. inserted: trait rules and examples -> memory -> gossip -> trait
//      reinforcement -> THIS BLOCK
//   4. ACTIONS: protocol and MOOD: contract
//
// It goes at the END of the inserted section — after trait reinforcement,
// before ACTIONS — for three reasons, one of them measured:
//
//   - Finding 4: earlier placement corrupts the action protocol.
//   - It is a directive, and Persona.hpp's own comment says small models
//     weight trailing instructions most.
//   - It must survive memory dilution, the same reason trait reinforcement
//     sits where it does.
//
// It must NOT go after ACTIONS:. Those contracts are pinned by
// test_npc_action.cpp and stay last; the existing anti-drift test is EXTENDED
// with a role case, never relaxed.
//
// `secret` is the per-match specific ("you were at the mill, not at home");
// the RoleDef is the behavioural frame around it. A null role or an empty
// secret renders an empty string, so a persona with no role must render
// BYTE-IDENTICALLY to today.
std::string renderRoleBlock(const RoleDef* role, const std::string& secret);

// Role by id; nullptr when unknown. Unknown ids are demoted with a log at
// spawn, exactly as unknown traitIds already are.
const RoleDef* findRole(const std::vector<RoleDef>& roles, const std::string& id);

}  // namespace llm_npc
