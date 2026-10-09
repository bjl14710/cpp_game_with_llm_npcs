// Schema-validity verdicts for NPC replies (DirectiveCheck.hpp).
// Plan: .claude/plans/npc-dialogue-measurement.md, step 2.
//
// SCAFFOLD — every case is skipped until checkReply() is implemented. A
// doctest::skip() is a RUNTIME skip, so these bodies still have to COMPILE;
// they are written against the real interface, not sketched.
//
// What these pin, and why it is not covered by tests/test_npc_action.cpp:
// that suite pins what the GAME does with a reply (tags stripped, last mood
// wins, brackets never reach the transcript). This suite pins what the
// MEASUREMENT sees, which is deliberately stricter — the forgiveness that
// makes parseDirectives correct for gameplay is exactly what destroys the
// evidence a schema-validity rate is counting.
#include <algorithm>
#include <string>

#include "DirectiveCheck.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {
// A reply that obeys the contract: prose plus exactly one known mood tag.
const char* kCleanReply = "Tuesday? I was at the bakery. [[MOOD: neutral]]";
}  // namespace

TEST_CASE("a well-formed reply is valid and keeps its prose"
          * doctest::skip()) {
    const DirectiveVerdict v = checkReply(kCleanReply);
    CHECK(v.valid);
    CHECK(v.reasons.empty());
    CHECK(v.spokenText == "Tuesday? I was at the bakery.");
    CHECK(v.directives.hasMood);
    CHECK(v.directives.mood == NpcMood::Neutral);
    CHECK(v.moodTagCount == 1);
    CHECK(v.actionTagCount == 0);
}

TEST_CASE("two mood tags is a violation the game itself cannot see"
          * doctest::skip()) {
    // parseDirectives takes the last recognized mood and strips both, so the
    // game behaves fine and the transcript looks clean. That is precisely why
    // the check counts tags before they are stripped.
    const DirectiveVerdict v =
        checkReply("[[MOOD: happy]] Fine, I suppose. [[MOOD: sad]]");
    CHECK_FALSE(v.valid);
    CHECK(v.moodTagCount == 2);
    CHECK(std::find(v.reasons.begin(), v.reasons.end(),
                    std::string("duplicate_mood")) != v.reasons.end());
    // The parse result still matches the game's: last recognized mood wins.
    CHECK(v.directives.mood == NpcMood::Sad);
}

TEST_CASE("a reply with no mood tag at all is invalid" * doctest::skip()) {
    const DirectiveVerdict v = checkReply("I have nothing to say to you.");
    CHECK_FALSE(v.valid);
    CHECK(v.moodTagCount == 0);
    CHECK(std::find(v.reasons.begin(), v.reasons.end(),
                    std::string("missing_mood")) != v.reasons.end());
}

TEST_CASE("an unknown mood keyword counts as unknown, not as a mood"
          * doctest::skip()) {
    const DirectiveVerdict v = checkReply("Hm. [[MOOD: wistful]]");
    CHECK_FALSE(v.valid);
    CHECK(v.unknownTagCount == 1);
    CHECK(v.moodTagCount == 0);
    CHECK(std::find(v.reasons.begin(), v.reasons.end(),
                    std::string("unknown_mood")) != v.reasons.end());
}

TEST_CASE("two action tags is a violation" * doctest::skip()) {
    const DirectiveVerdict v = checkReply(
        "Right, come along. [[ACTION: follow]] [[ACTION: stop]] [[MOOD: neutral]]");
    CHECK_FALSE(v.valid);
    CHECK(v.actionTagCount == 2);
    CHECK(std::find(v.reasons.begin(), v.reasons.end(),
                    std::string("duplicate_action")) != v.reasons.end());
}

TEST_CASE("an empty reply is invalid rather than vacuously clean"
          * doctest::skip()) {
    // A model that returns only a tag leaves nothing on screen. Counting that
    // as valid would let a broken model score a perfect validity rate.
    const DirectiveVerdict v = checkReply("[[MOOD: neutral]]");
    CHECK_FALSE(v.valid);
    CHECK(std::find(v.reasons.begin(), v.reasons.end(),
                    std::string("empty_reply")) != v.reasons.end());
}

TEST_CASE("non-directive brackets are left alone and are not residue"
          * doctest::skip()) {
    // parseDirectives deliberately ignores "[sigh]" and "[[1]]". The residue
    // check must agree, or every stage direction a model writes becomes a
    // schema violation.
    const DirectiveVerdict v = checkReply("[sigh] Fine. [[MOOD: sad]]");
    CHECK(v.valid);
    CHECK(v.spokenText == "[sigh] Fine.");
}

TEST_CASE("the verdict serializes to the documented JSON shape"
          * doctest::skip()) {
    // persona_prompt --parse and dialogue_turns.directives_json both carry
    // this; the Python harness parses it. The keys are a contract.
    const std::string json = verdictToJson(checkReply(kCleanReply));
    CHECK(json.find("\"valid\":true") != std::string::npos);
    CHECK(json.find("\"mood\":\"neutral\"") != std::string::npos);
    CHECK(json.find("\"mood_tags\":1") != std::string::npos);
}

TEST_CASE("the shared tag scanner did not change parseDirectives' behaviour"
          * doctest::skip()) {
    // Step 2 rewrites parseDirectives on top of forEachDirectiveTag. This is
    // the canary: the game-facing result for a messy reply must be byte-identical
    // to what tests/test_npc_action.cpp already pins.
    std::string reply = "[[ACTION:call police]] Someone help! [[MOOD:surprised]]";
    const Directives d = parseDirectives(reply);
    CHECK(reply == "Someone help!");
    CHECK(d.action == NpcAction::CallPolice);
    CHECK(d.mood == NpcMood::Surprised);
}
