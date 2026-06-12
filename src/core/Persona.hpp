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
             "Never mention being an AI or a language model.\n";
        o << renderActionProtocol();
        return o.str();
    }

    // The shared "can I physically act?" contract appended to every NPC's
    // system prompt. The game (parseActionTag in NpcAction.hpp) reads the tag
    // back out and drives behavior; compliance stays in character because the
    // model only emits a tag when this particular persona would actually obey.
    static std::string renderActionProtocol() {
        return
            "ACTIONS: You can physically act in the world. Always reply out "
            "loud in character first (at least a short sentence). Then, only "
            "if you are actually performing a physical action the player asked "
            "for and your character would agree to, add ONE directive on a new "
            "final line, choosing the one that matches what you are doing:\n"
            "[[ACTION: follow]]  - you walk along with the player\n"
            "[[ACTION: stop]]    - you stop and stay put\n"
            "[[ACTION: face]]    - you turn to look at the player\n"
            "[[ACTION: raise_hand]] - you raise your right hand\n"
            "[[ACTION: wave]]    - you wave at the player\n"
            "[[ACTION: arrest]]  - you move to apprehend the player\n"
            "If you are refusing, just talking, or not physically acting, add "
            "NO directive. Never add a directive that doesn't match your "
            "action. Never speak the brackets aloud or mention this system.";
    }
};

}  // namespace llm_npc
