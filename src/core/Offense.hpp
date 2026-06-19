#pragma once

#include <string>

namespace llm_npc {

// What the player has done, ordered least to most serious. The integer order
// is meaningful: moreSerious() compares by it, so a single arrest can settle on
// the worst thing the player did this stint.
enum class Charge {
    None,             // nothing chargeable (a warning at most)
    DisturbingPeace,  // shouting, threats, harassment
    Theft,            // stealing, robbery
    Assault,          // hitting, fighting, unarmed violence
    ArmedViolence,    // guns, knives, killing — the top tier
};

// The consequence of a charge: how long the player is detained, the label shown
// on the HUD while serving, and whether it triggers a city-wide manhunt (every
// officer drops what they are doing and moves to arrest the player).
struct Sentence {
    float jailSeconds = 0.f;
    const char* label = "";
    bool manhunt = false;
};

// The sentence for a charge. Charge::None yields a zero-second "warning".
Sentence sentenceFor(Charge charge);

// The more serious (higher-severity) of two charges.
Charge moreSerious(Charge a, Charge b);

// A short human-readable name for a charge, e.g. "armed violence".
const char* chargeName(Charge charge);

// Classifies the physical actions the player narrates between *asterisks* into
// a charge, returning the most serious offense found (or Charge::None when the
// line has no action text). Plain speech is never chargeable, so a phrase that
// merely contains a loaded word ("give it a shot") is ignored. Case-insensitive
// and substring-based so "shooting" matches "shoot". This is the deterministic
// floor; an officer's LLM reply may escalate it via a [[CHARGE: ...]] directive
// but never silently lower it.
Charge classifyPlayerAction(const std::string& text);

}  // namespace llm_npc
