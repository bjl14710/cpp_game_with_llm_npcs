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

TEST_CASE("parseActionTag strips unknown keywords but maps them to None") {
    // Brackets must never leak to the transcript, even for keywords we don't
    // recognize; the action simply stays None.
    std::string reply = "Hmm. [[ACTION: teleport]]";
    CHECK(parseActionTag(reply) == NpcAction::None);
    CHECK(reply == "Hmm.");
}

TEST_CASE("parseActionTag ignores brackets that are not an action directive") {
    std::string reply = "See note [[1]] for details.";
    CHECK(parseActionTag(reply) == NpcAction::None);
    CHECK(reply == "See note [[1]] for details.");
}

TEST_CASE("parseDirectives handles the mess a small model emits") {
    SUBCASE("tags at the start and middle, duplicated mood: last one wins") {
        std::string reply = "[[MOOD: happy]] Lovely day! [[ACTION: wave]] [[MOOD: neutral]]";
        const Directives d = parseDirectives(reply);
        CHECK(d.action == NpcAction::Wave);
        CHECK(d.hasMood);
        CHECK(d.mood == NpcMood::Neutral);
        CHECK(reply == "Lovely day!");
    }
    SUBCASE("adjacent tags without whitespace") {
        std::string reply = "Fine.[[MOOD: annoyed]][[ACTION: raise_hand]]";
        const Directives d = parseDirectives(reply);
        CHECK(d.action == NpcAction::RaiseHand);
        CHECK(d.mood == NpcMood::Angry);  // synonym mapping
        CHECK(reply == "Fine.");
    }
    SUBCASE("single-bracket tag still parses and strips") {
        std::string reply = "[ACTION: wave] [[MOOD: happy]]";
        const Directives d = parseDirectives(reply);
        CHECK(d.action == NpcAction::Wave);
        CHECK(d.mood == NpcMood::Happy);
        CHECK(reply.empty());
    }
    SUBCASE("mood synonyms map onto the six moods") {
        struct Case { const char* word; NpcMood want; };
        const Case cases[] = {
            {"flattered", NpcMood::Happy},   {"irritated", NpcMood::Angry},
            {"flustered", NpcMood::Embarrassed}, {"shocked", NpcMood::Surprised},
            {"hurt", NpcMood::Sad},          {"calm", NpcMood::Neutral},
        };
        for (const auto& c : cases) {
            std::string reply = std::string("Oh. [[MOOD: ") + c.word + "]]";
            const Directives d = parseDirectives(reply);
            CHECK(d.hasMood);
            CHECK(d.mood == c.want);
            CHECK(reply == "Oh.");
        }
    }
    SUBCASE("unknown mood keyword is stripped but reports no mood") {
        std::string reply = "Hm. [[MOOD: bamboozled]]";
        const Directives d = parseDirectives(reply);
        CHECK_FALSE(d.hasMood);
        CHECK(reply == "Hm.");
    }
    SUBCASE("call_police keyword, with space or underscore") {
        std::string a = "Help! [[ACTION: call_police]]";
        CHECK(parseDirectives(a).action == NpcAction::CallPolice);
        std::string b = "Help! [[ACTION: call police]]";
        CHECK(parseDirectives(b).action == NpcAction::CallPolice);
    }
    SUBCASE("no tags at all") {
        std::string reply = "Just words.";
        const Directives d = parseDirectives(reply);
        CHECK(d.action == NpcAction::None);
        CHECK_FALSE(d.hasMood);
        CHECK(reply == "Just words.");
    }
}
