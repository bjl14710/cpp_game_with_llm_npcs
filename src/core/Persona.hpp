#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "Trait.hpp"

namespace llm_npc {

// A Persona is the static identity of an NPC. It is rendered into a single
// system-prompt string that is prepended to every chat request.
struct Persona {
    std::string name;                 // "Companion", "Jim", "Jane #47"
    std::string role;                 // "traveling companion", "pharmacist"
    std::vector<std::string> traits;  // "curious", "soft-spoken", ...
    // STRUCTURED traits (issue #116): ids into the traits/*.trait library,
    // from repeated `trait =` persona keys. Additive — the free-text
    // `traits` adjectives above stay as flavor. Composition order is a
    // TESTED contract: identity < trait rules+examples < memory < trait
    // reinforcement < action protocol.
    std::vector<std::string> traitIds;
    std::string speakingStyle;        // "short sentences, 1-3 per reply"
    std::string knowledgeBoundary;    // what they do / do not know
    std::string extraDirectives;      // free-form constraints
    bool police = false;              // may arrest; others can only call police
    bool armed = false;               // carries a weapon; retaliates when attacked

    // Renders the system prompt with a persisted first-person memory from
    // earlier sessions woven in, so the NPC picks up where things left off.
    // An empty memory renders identically to renderSystemPrompt().
    std::string renderSystemPrompt(const std::string& memory) const {
        return renderSystemPrompt(memory, "");
    }

    // Memory plus town gossip: the facts THIS character has heard (from the
    // shared world bus), so knowledge stays per-NPC. Both sections insert
    // before the action protocol — directive instructions stay last (small
    // models weight trailing instructions most). Empty strings render
    // identically to the plain prompt.
    std::string renderSystemPrompt(const std::string& memory,
                                   const std::string& gossip) const {
        return renderSystemPrompt(memory, gossip, {});
    }

    // Full assembly with resolved trait definitions (unknown traitIds were
    // demoted with a log at spawn; render simply composes what it is
    // handed). Rules and examples land BEFORE the memory summary, and a
    // short reinforcement AFTER it, so a long remembered history cannot
    // dilute the personality (the anti-drift contract, pinned by test).
    std::string renderSystemPrompt(const std::string& memory,
                                   const std::string& gossip,
                                   const std::vector<const TraitDef*>& traitDefs) const {
        return renderSystemPrompt(memory, gossip, traitDefs, "");
    }

    // Full assembly plus this match's STORYLINE ROLE (plan: role-layer).
    //
    // `roleBlock` comes from renderRoleBlock() and is empty for every NPC that
    // has not been cast — which is all of them outside a detective match, so an
    // empty block must render byte-identically to the overload above. A test
    // compares the two as exact strings.
    //
    // PLACEMENT IS THE WHOLE DESIGN, and it is measured rather than argued. The
    // role block goes LAST inside the inserted section: after trait
    // reinforcement, immediately before "ACTIONS: ". Three reasons:
    //
    //   - MEASURED: with the block placed earlier — before "Stay in character" —
    //     a killer emitted [[ACTION: call_police]] on three of five
    //     interrogation turns against qwen3:8b. A murderer summoning the police
    //     is both a bug and a second tell. Late placement produced none.
    //   - It is a directive, and small models weight trailing instructions most
    //     (the same reasoning that puts the action protocol last).
    //   - It must survive memory dilution, for the same reason trait
    //     reinforcement does.
    //
    // It must NOT go after "ACTIONS: ". Those contracts are pinned by
    // test_npc_action.cpp and stay last; this extends the assembly order with
    // one more position and relaxes nothing.
    std::string renderSystemPrompt(const std::string& memory,
                                   const std::string& gossip,
                                   const std::vector<const TraitDef*>& traitDefs,
                                   const std::string& roleBlock) const {
        std::string prompt = renderSystemPrompt();
        std::string section = renderTraitBlock(traitDefs);
        if (!memory.empty()) {
            section += "What you remember from earlier meetings with this player: " +
                       memory + "\n";
        }
        if (!gossip.empty()) {
            section += "Things you have heard around town (bring them up when "
                       "relevant): " + gossip + "\n";
        }
        section += renderTraitReinforcement(traitDefs);
        section += roleBlock;  // last in the section — see the comment above
        if (section.empty()) return prompt;
        const auto at = prompt.find("ACTIONS: ");
        if (at == std::string::npos) return prompt + section;
        prompt.insert(at, section);
        return prompt;
    }

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
