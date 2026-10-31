#include "Offense.hpp"

#include <cctype>
#include <initializer_list>

namespace llm_npc {

Sentence sentenceFor(Charge charge) {
    switch (charge) {
        case Charge::None:            return {0.f, "warning", false};
        case Charge::DisturbingPeace: return {8.f, "disturbing the peace", false};
        case Charge::Theft:           return {15.f, "theft", false};
        case Charge::Assault:         return {25.f, "assault", false};
        case Charge::ArmedViolence:   return {40.f, "armed violence", true};
    }
    return {0.f, "warning", false};
}

Charge moreSerious(Charge a, Charge b) {
    return static_cast<int>(a) >= static_cast<int>(b) ? a : b;
}

const char* chargeName(Charge charge) {
    return sentenceFor(charge).label;
}

Charge classifyPlayerAction(const std::string& text) {
    // Only physical actions the player narrates between *asterisks* are
    // chargeable. Plain speech is never an offense, so conversational phrases
    // that merely contain a loaded word ("give it a shot", "hit the road")
    // don't trigger an arrest.
    std::string s;
    for (std::size_t star = text.find('*'); star != std::string::npos;) {
        const std::size_t end = text.find('*', star + 1);
        if (end == std::string::npos) break;  // unmatched '*': ignore the tail
        s += ' ';
        s += text.substr(star + 1, end - star - 1);
        star = text.find('*', end + 1);
    }
    if (s.empty()) return Charge::None;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    const auto mentions = [&s](std::initializer_list<const char*> keywords) {
        for (const char* kw : keywords) {
            if (s.find(kw) != std::string::npos) return true;
        }
        return false;
    };

    // Accumulate the worst category mentioned; the player's text may describe
    // several things, and the most serious one drives the charge.
    Charge charge = Charge::None;
    if (mentions({"shout", "yell", "scream", "threaten", "curse", "swear", "harass",
                  "spit", "vandal", "insult"})) {
        charge = moreSerious(charge, Charge::DisturbingPeace);
    }
    if (mentions({"steal", "stole", "rob", "loot", "mug", "pickpocket", "snatch",
                  "shoplift", "thief", "theft", "burglar"})) {
        charge = moreSerious(charge, Charge::Theft);
    }
    if (mentions({"punch", "hit", "kick", "attack", "beat", "slap", "strangle",
                  "fight", "assault", "shove", "choke"})) {
        charge = moreSerious(charge, Charge::Assault);
    }
    if (mentions({"shoot", "shot", "gun", "pistol", "rifle", "stab", "knife",
                  "kill", "murder", "blast", "firearm"})) {
        charge = moreSerious(charge, Charge::ArmedViolence);
    }
    return charge;
}

}  // namespace llm_npc
