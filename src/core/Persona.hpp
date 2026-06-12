#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace llm_npc {

// A Persona is the static identity of an NPC. It is rendered into a single
// system-prompt string that is prepended to every chat request.
struct Persona {
    std::string name;                 // "Companion", "Jim", "Jane #47"
    std::string role;                 // "traveling companion", "pharmacist"
    std::vector<std::string> traits;  // "curious", "soft-spoken", ...
    std::string speakingStyle;        // "short sentences, 1-3 per reply"
    std::string knowledgeBoundary;    // what they do / do not know
    std::string extraDirectives;      // free-form constraints

    std::string renderSystemPrompt() const {
        std::ostringstream o;
        o << "You are " << name;
        if (!role.empty()) o << ", a " << role;
        o << ".\n";
        if (!traits.empty()) {
            o << "Personality: ";
            for (size_t i = 0; i < traits.size(); ++i) {
                if (i) o << ", ";
                o << traits[i];
            }
            o << ".\n";
        }
        if (!speakingStyle.empty()) o << "Speaking style: " << speakingStyle << "\n";
        if (!knowledgeBoundary.empty()) o << "Knowledge: " << knowledgeBoundary << "\n";
        if (!extraDirectives.empty()) o << extraDirectives << "\n";
        o << "Stay in character. Never break the fourth wall. "
             "Never mention being an AI or a language model.";
        return o.str();
    }
};

}  // namespace llm_npc
