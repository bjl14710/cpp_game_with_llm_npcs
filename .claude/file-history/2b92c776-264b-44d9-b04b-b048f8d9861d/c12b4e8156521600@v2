// Tests for Offense: charge classification, sentences, and [[CHARGE:]] parsing.
#include <string>

#include "NpcAction.hpp"
#include "Offense.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("classifyPlayerAction picks the most serious offense in the action text") {
    CHECK(classifyPlayerAction("hello there, nice shop") == Charge::None);
    CHECK(classifyPlayerAction("*shouts loudly at you*") == Charge::DisturbingPeace);
    CHECK(classifyPlayerAction("*robs the till*") == Charge::Theft);
    CHECK(classifyPlayerAction("*punches the clerk*") == Charge::Assault);
    CHECK(classifyPlayerAction("*shoots someone*") == Charge::ArmedViolence);
    // When several offenses appear in the action, the worst wins.
    CHECK(classifyPlayerAction("*shouts, steals, then shoots*") == Charge::ArmedViolence);
    CHECK(classifyPlayerAction("*yells and pulls a knife*") == Charge::ArmedViolence);
}

TEST_CASE("classifyPlayerAction only charges actions narrated between asterisks") {
    // Plain speech is never an offense, even when it contains a loaded word.
    CHECK(classifyPlayerAction("give it a shot") == Charge::None);
    CHECK(classifyPlayerAction("I'll hit the road, thanks") == Charge::None);
    CHECK(classifyPlayerAction("can I get a shot of espresso?") == Charge::None);
    // Speech around an action still picks up the action.
    CHECK(classifyPlayerAction("watch this *fires a pistol* haha") == Charge::ArmedViolence);
}

TEST_CASE("classifyPlayerAction is case-insensitive and substring-based") {
    CHECK(classifyPlayerAction("*SHOOTING the place up*") == Charge::ArmedViolence);
    CHECK(classifyPlayerAction("*threatening everyone here*") == Charge::DisturbingPeace);
    CHECK(classifyPlayerAction("*a bit of shoplifting*") == Charge::Theft);
}

TEST_CASE("moreSerious returns the higher-severity charge") {
    CHECK(moreSerious(Charge::None, Charge::Theft) == Charge::Theft);
    CHECK(moreSerious(Charge::Assault, Charge::Theft) == Charge::Assault);
    CHECK(moreSerious(Charge::ArmedViolence, Charge::DisturbingPeace) == Charge::ArmedViolence);
    CHECK(moreSerious(Charge::Theft, Charge::Theft) == Charge::Theft);
}

TEST_CASE("sentenceFor scales with severity; only armed violence is a manhunt") {
    CHECK(sentenceFor(Charge::None).jailSeconds == doctest::Approx(0.f));
    CHECK(sentenceFor(Charge::DisturbingPeace).jailSeconds < sentenceFor(Charge::Theft).jailSeconds);
    CHECK(sentenceFor(Charge::Theft).jailSeconds < sentenceFor(Charge::Assault).jailSeconds);
    CHECK(sentenceFor(Charge::Assault).jailSeconds < sentenceFor(Charge::ArmedViolence).jailSeconds);
    CHECK_FALSE(sentenceFor(Charge::Theft).manhunt);
    CHECK_FALSE(sentenceFor(Charge::Assault).manhunt);
    CHECK(sentenceFor(Charge::ArmedViolence).manhunt);
    CHECK(std::string(sentenceFor(Charge::Theft).label) == "theft");
    CHECK(std::string(sentenceFor(Charge::ArmedViolence).label) == "armed violence");
}

TEST_CASE("parseDirectives extracts a [[CHARGE:]] tag and strips every tag") {
    std::string reply =
        "You're under arrest. [[ACTION: arrest]] [[CHARGE: armed_violence]] [[MOOD: angry]]";
    const Directives d = parseDirectives(reply);
    CHECK(d.action == NpcAction::Arrest);
    CHECK(d.charge == Charge::ArmedViolence);
    CHECK(d.mood == NpcMood::Angry);
    CHECK(reply.find('[') == std::string::npos);
    CHECK(reply == "You're under arrest.");
}

TEST_CASE("parseDirectives keeps the most serious of several charge tags") {
    std::string reply =
        "[[CHARGE: theft]] then [[CHARGE: assault]] then [[CHARGE: disturbing_the_peace]]";
    const Directives d = parseDirectives(reply);
    CHECK(d.charge == Charge::Assault);
}

TEST_CASE("parseDirectives leaves charge None when no charge tag is present") {
    std::string reply = "Just talking. [[MOOD: happy]]";
    const Directives d = parseDirectives(reply);
    CHECK(d.charge == Charge::None);
    CHECK(d.mood == NpcMood::Happy);
}
