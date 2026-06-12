#include <string>

#include "NpcAction.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("parseActionTag extracts each known keyword") {
    struct Case {
        std::string tag;
        NpcAction expected;
    };
    const Case cases[] = {
        {"[[ACTION: follow]]", NpcAction::Follow},
        {"[[ACTION: stop]]", NpcAction::Stop},
        {"[[ACTION: stay]]", NpcAction::Stop},
        {"[[ACTION: face]]", NpcAction::Face},
        {"[[ACTION: look]]", NpcAction::Face},
        {"[[ACTION: raise_hand]]", NpcAction::RaiseHand},
        {"[[ACTION: wave]]", NpcAction::Wave},
        {"[[ACTION: arrest]]", NpcAction::Arrest},
    };
    for (const auto& c : cases) {
        std::string reply = "Sure thing.\n" + c.tag;
        CHECK(parseActionTag(reply) == c.expected);
        CHECK(reply == "Sure thing.");  // tag and trailing whitespace stripped
    }
}

TEST_CASE("parseActionTag is case-insensitive and tolerates whitespace") {
    std::string reply = "Right away! [[  Action :   FOLLOW  ]]";
    CHECK(parseActionTag(reply) == NpcAction::Follow);
    CHECK(reply == "Right away!");
}

TEST_CASE("parseActionTag leaves prose intact when no tag is present") {
    std::string reply = "I'm just a baker, I can't do that.";
    CHECK(parseActionTag(reply) == NpcAction::None);
    CHECK(reply == "I'm just a baker, I can't do that.");
}

TEST_CASE("parseActionTag rejects unknown keywords without mangling text") {
    std::string reply = "Hmm. [[ACTION: teleport]]";
    CHECK(parseActionTag(reply) == NpcAction::None);
    CHECK(reply == "Hmm. [[ACTION: teleport]]");
}

TEST_CASE("parseActionTag ignores brackets that are not an action directive") {
    std::string reply = "See note [[1]] for details.";
    CHECK(parseActionTag(reply) == NpcAction::None);
    CHECK(reply == "See note [[1]] for details.");
}
