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
#include <iostream>
#include <sstream>
#include <string>

#include "NpcAction.hpp"
#include "PersonaLoader.hpp"
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

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--parse") return parseMode();

    if (argc < 2) {
        std::cerr << "usage: persona_prompt <file.persona> [memory-text]\n"
                  << "       persona_prompt --parse   (reply on stdin)\n";
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
