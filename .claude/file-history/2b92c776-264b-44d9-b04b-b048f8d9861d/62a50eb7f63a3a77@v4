// Prints the exact system prompt the game sends for a persona file — the
// single source of truth for the model benchmark (tools/bench_npc_models.py),
// so the harness can never drift from what NPCs actually send.
//
// Usage: persona_prompt <file.persona> [memory-text]
#include <iostream>

#include "PersonaLoader.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: persona_prompt <file.persona> [memory-text]\n";
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
