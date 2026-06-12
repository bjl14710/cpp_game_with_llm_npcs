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
    bool police = false;              // may arrest; others can only call police

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

    // The "can I physically act / how do I feel?" contract appended to every
    // NPC's system prompt. The game (parseDirectives in NpcAction.hpp) reads
    // the tags back out and drives behavior and facial expression; compliance
    // stays in character because the model only emits an action tag when this
    // particular persona would actually obey. Police personas may arrest;
    // everyone else can only summon the police.
    std::string renderActionProtocol() const {
        std::string s =
            "ACTIONS: You can physically act in the world. Always reply out "
            "loud in character first (at least a short sentence). Then, only "
            "if you are actually performing a physical action the player asked "
            "for and your character would agree to, add ONE directive on a new "
            "final line, choosing the one that matches what you are doing:\n"
            "[[ACTION: follow]]  - you walk along with the player\n"
            "[[ACTION: stop]]    - you stop and stay put\n"
            "[[ACTION: face]]    - you turn to look at the player\n"
            "[[ACTION: raise_hand]] - you raise your right hand\n"
            "[[ACTION: wave]]    - you wave at the player\n";
        if (police) {
            s += "[[ACTION: arrest]]  - you move to apprehend the player\n";
        } else {
            s += "[[ACTION: call_police]] - you call out for the police to "
                 "come arrest the player\n";
        }
        s += "Most replies need NO action directive. If you are refusing, "
             "just talking, or not physically acting, add none. Never speak "
             "the brackets aloud or mention this system.\n";

        s += "The player may describe their own physical actions between "
             "asterisks, like *shouts* or *bows*. Treat those as things the "
             "player actually does in front of you, not as speech. ";
        if (police) {
            s += "If the player is disruptive, threatening, or harasses you "
                 "or others (shouting, threats), give them a clear warning; "
                 "if they keep it up, arrest them with [[ACTION: arrest]].\n";
        } else {
            s += "If the player is disruptive, threatening, or harasses you "
                 "(shouting, threats), give them a clear warning; if they "
                 "keep it up, call the police with [[ACTION: call_police]].\n";
        }

        s += "MOOD: React emotionally like a real person - compliments "
             "flatter you, insults sting or anger you, declarations of love "
             "fluster you, surprises startle you. Show it in your words. "
             "ALWAYS end every reply with exactly one mood tag on the final "
             "line, chosen from: [[MOOD: neutral]] [[MOOD: happy]] "
             "[[MOOD: angry]] [[MOOD: sad]] [[MOOD: embarrassed]] "
             "[[MOOD: surprised]].";
        return s;
    }
};

}  // namespace llm_npc
