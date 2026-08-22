// Prints the exact system prompt the game sends for a persona file — the
// single source of truth for the model benchmark (tools/bench_npc_models.py),
// so the harness can never drift from what NPCs actually send.
//
// --parse exists for the same reason on the other side of the exchange:
// tools/eval_lines.py must judge a candidate reply with the REAL
// parseDirectives, not a Python reimplementation of it.
// tools/bench_npc_models.py duplicates the mood keyword list in Python and
// its own comment admits it has to be hand-synced; this avoids repeating that.
//
// Usage:
//   persona_prompt <file.persona> [memory-text]   print the system prompt
//   persona_prompt --parse                        read a reply on stdin, print
//                                                 the parsed verdict as JSON
//   persona_prompt --state <state.json>           print the FULL prompt for a
//                                                 scripted game state
//
// --state exists because the plain mode cannot express a cast member. It calls
// renderSystemPrompt(memory) only: no traits, no gossip and -- the part that
// matters -- no role block, so there is no way to ask what a KILLER's prompt
// looks like. Every secrecy probe depends on setting one up
// (tools/role_leak_probe.py, tools/eval_dialogue.py), and the alternative is
// re-implementing renderRoleBlock plus the five-section composition order in
// Python, which Role.hpp calls "the whole design" and pins by test. Placement
// is exactly what those probes measure, so a second implementation of it would
// not be a shortcut -- it would be measuring the wrong thing.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "NpcAction.hpp"
#include "PersonaLoader.hpp"
#include "Role.hpp"
#include "Trait.hpp"
#include "json.hpp"

namespace {

const char* actionName(llm_npc::NpcAction action) {
    switch (action) {
        case llm_npc::NpcAction::None: return "none";
        case llm_npc::NpcAction::Follow: return "follow";
        case llm_npc::NpcAction::Stop: return "stop";
        case llm_npc::NpcAction::Face: return "face";
        case llm_npc::NpcAction::RaiseHand: return "raise_hand";
        case llm_npc::NpcAction::Wave: return "wave";
        case llm_npc::NpcAction::Arrest: return "arrest";
        case llm_npc::NpcAction::CallPolice: return "call_police";
        case llm_npc::NpcAction::ReturnHome: return "return_home";
    }
    return "none";
}

const char* moodName(llm_npc::NpcMood mood) {
    switch (mood) {
        case llm_npc::NpcMood::Neutral: return "neutral";
        case llm_npc::NpcMood::Happy: return "happy";
        case llm_npc::NpcMood::Angry: return "angry";
        case llm_npc::NpcMood::Sad: return "sad";
        case llm_npc::NpcMood::Embarrassed: return "embarrassed";
        case llm_npc::NpcMood::Surprised: return "surprised";
    }
    return "neutral";
}

// Reads a reply on stdin and reports what the game would actually make of it:
// the text a player sees once tags are stripped, plus the directives parsed
// out. Reading from stdin rather than argv keeps newlines and shell
// metacharacters in reply text from mattering.
int parseMode() {
    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    std::string reply = buffer.str();

    const llm_npc::Directives directives = llm_npc::parseDirectives(reply);

    nlohmann::json out;
    out["text"] = reply;  // tags stripped — exactly what reaches the transcript
    out["action"] = actionName(directives.action);
    out["mood"] = moodName(directives.mood);
    out["has_mood"] = directives.hasMood;
    std::cout << out.dump() << "\n";
    return 0;
}

// Renders the exact system prompt the game would send for a scripted state.
//
// The state file names the persona, the traits to resolve, the remembered
// summary, the gossip block, and this match's role and secret:
//
//   {"persona": "personas/marge.persona",
//    "traits": ["gossipy"],
//    "memory": "",
//    "gossip": "",
//    "role": "killer",
//    "secret": "You were at the mill, not at home.",
//    "traits_dir": "traits", "roles_dir": "roles"}
//
// Every field is optional except `persona`; an omitted role renders a prompt
// BYTE-IDENTICAL to the plain mode's, which is the role layer's own contract
// (Role.hpp: "a persona with no role must render BYTE-IDENTICALLY to today").
//
// Failures exit non-zero with a named reason and print NO prompt. A partial
// prompt would silently weaken every probe built on it -- a missing trait
// would read as a measurement rather than as a broken fixture.
int stateMode(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "error: cannot read state file " << path << "\n";
        return 1;
    }
    nlohmann::json state;
    try {
        in >> state;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "error: malformed state JSON: " << e.what() << "\n";
        return 1;
    }
    if (!state.contains("persona") || !state["persona"].is_string()) {
        std::cerr << "error: state needs a \"persona\" path\n";
        return 1;
    }

    const llm_npc::PersonaParseResult parsed =
        llm_npc::parsePersonaFile(state["persona"].get<std::string>());
    if (!parsed.ok) {
        std::cerr << "error: " << parsed.error << "\n";
        return 1;
    }
    llm_npc::Persona persona = parsed.value.persona;
    // Traits named in the state REPLACE the persona's own, so a probe can hold
    // everything else fixed and vary one trait. Omitting the key keeps the
    // persona's authored list.
    if (state.contains("traits")) {
        persona.traitIds = state["traits"].get<std::vector<std::string>>();
    }

    const std::string traitsDir = state.value("traits_dir", "traits");
    const std::string rolesDir = state.value("roles_dir", "roles");

    // Bound to named vectors before anything points into them. findRole
    // returns a pointer INTO its argument, and Role.hpp:104 records that
    // `findRole(loadAllRoles(dir), id)` dangling the moment the statement ends
    // is how a test failed the first time it ran -- not a hypothetical.
    std::vector<std::string> traitErrors;
    const std::vector<llm_npc::TraitDef> traits =
        llm_npc::loadAllTraits(traitsDir, &traitErrors);
    for (const std::string& err : traitErrors) {
        std::cerr << "warning: trait: " << err << "\n";
    }

    // Resolve exactly as Npc::resolvedTraits does, and REFUSE on an unknown
    // id. The game demotes those with a log because a player must not lose an
    // NPC over a typo; a measurement has the opposite duty.
    std::vector<const llm_npc::TraitDef*> resolved;
    for (const std::string& id : persona.traitIds) {
        const llm_npc::TraitDef* found = nullptr;
        for (const llm_npc::TraitDef& trait : traits) {
            if (trait.id == id) { found = &trait; break; }
        }
        if (found == nullptr) {
            std::cerr << "error: unknown trait id \"" << id << "\" in " << traitsDir
                      << "\n";
            return 1;
        }
        resolved.push_back(found);
    }

    std::string roleBlock;
    const std::string roleId = state.value("role", "");
    const std::string secret = state.value("secret", "");
    if (!roleId.empty()) {
        std::vector<std::string> roleErrors;
        const std::vector<llm_npc::RoleDef> roles =
            llm_npc::loadAllRoles(rolesDir, &roleErrors);
        for (const std::string& err : roleErrors) {
            std::cerr << "warning: role: " << err << "\n";
        }
        const llm_npc::RoleDef* role = llm_npc::findRole(roles, roleId);
        if (role == nullptr) {
            std::cerr << "error: unknown role id \"" << roleId << "\" in " << rolesDir
                      << "\n";
            return 1;
        }
        roleBlock = llm_npc::renderRoleBlock(role, secret);
    }

    // The same overload Npc::ask calls, in the same argument order. This line
    // is the whole point of the mode: one source of truth for prompt assembly.
    std::cout << persona.renderSystemPrompt(state.value("memory", ""),
                                            state.value("gossip", ""), resolved,
                                            roleBlock);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--parse") return parseMode();
    if (argc >= 3 && std::string(argv[1]) == "--state") return stateMode(argv[2]);

    if (argc < 2) {
        std::cerr << "usage: persona_prompt <file.persona> [memory-text]\n"
                  << "       persona_prompt --parse           (reply on stdin)\n"
                  << "       persona_prompt --state <file>    (scripted state)\n";
        return 2;
    }
    const llm_npc::PersonaParseResult result = llm_npc::parsePersonaFile(argv[1]);
    if (!result.ok) {
        std::cerr << "error: " << result.error << "\n";
        return 1;
    }
    std::cout << result.value.persona.renderSystemPrompt(argc >= 3 ? argv[2] : "");
    return 0;
}
