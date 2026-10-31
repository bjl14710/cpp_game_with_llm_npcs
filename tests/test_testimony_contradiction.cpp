// Does the detective mechanic work end to end? These are the tests that say so.
//
// This file was written to force a decision, and it did. It kept the answer as
// skipped cases until #214 settled the keying; those cases are live now, and
// they are the load-bearing ones — if they go red, the mode is not a mystery,
// it is a walking simulator with nameplates.
//
// WHAT WAS WRONG
// --------------
// `Journal.hpp` detects a contradiction as "one subject, more than one
// content", and its inner loop breaks the moment the subject changes
// (Journal.hpp:41). Two facts can only ever conflict if they share a subject.
//
// `seedMysteryFacts` used to key a witness fact on the SPEAKER, and
// `castStoryline` deals a unique resident to every slot — so two witnesses
// were always two agents, always two subjects. **Two residents could never
// contradict each other**, in any match, whatever the storyline authored.
//
// `validateStoryline` meanwhile REQUIRED a contradiction, defined as two
// witnesses in the same zone within half an hour saying different things, and
// a comment of mine called that "the closest structural proxy for the subject
// collision seedMysteryFacts produces". It was not a proxy — it measured
// something that could not produce a collision, and tests/test_storyline.cpp
// pinned it. The wrong invariant was merged and tested. Both comments were
// mine, from #179 and #187.
//
// WHAT THE FIX WAS
// ----------------
// A testimony fact's subject is now WHO IT IS ABOUT, not who said it:
//
//   about set    "marge_holloway_whereabouts" — two residents giving different
//                accounts of one person collide, and both sides are flagged.
//   about empty  "<speaker> testimony" — one resident changing their own story
//                is still caught. Kept deliberately; see below.
//
// Both keyings are live and both are tested. Speaker keying was never wrong,
// only insufficient, and trading one case for the other would have been a
// worse bug than the one being fixed.
//
// An alibi needed no new field: it is testimony whose `about` is the speaker.
#include <filesystem>
#include <string>
#include <vector>

#include "Gossip.hpp"
#include "Journal.hpp"
#include "Mystery.hpp"
#include "Storyline.hpp"
#include "WorldState.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

std::vector<Persona> roster() {
    std::vector<Persona> out;
    for (const char* name : {"Marge Holloway", "Ray Okafor", "Yuki Tanaka",
                             "Officer Dana Brooks", "Theo Vance", "Gus Pike"}) {
        Persona p;
        p.name = name;
        p.role = "resident";
        out.push_back(p);
    }
    return out;
}

// Two residents giving different accounts of the same moment, with NO `about`
// on either — so both key on their own speaker subject.
MysterySetup setupWithUnattributedWitnesses() {
    MysterySetup setup = generateMystery(roster(), 4242u);
    setup.witnesses.push_back(
        {"Ray Okafor", "bakery_block", "Marge leaving by the alley door", 21.5, ""});
    setup.witnesses.push_back(
        {"Yuki Tanaka", "bakery_block", "nobody came or went all evening", 21.6, ""});
    return setup;
}

int conflictingCount(const std::vector<JournalEntry>& entries) {
    int n = 0;
    for (const JournalEntry& e : entries) {
        if (e.conflicting) ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("two residents disagreeing produces two facts on the bus") {
    // The setup half always worked; it was the comparison that could not fire.
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithUnattributedWitnesses();

    WorldState state;
    seedMysteryFacts(state, setup, cast);

    // One death fact plus one per witness.
    CHECK(state.facts().size() == 3);
}

TEST_CASE("testimony about nobody in particular does not contradict") {
    // The old bug's shape, kept as the boundary of the fix rather than deleted.
    //
    // Ray says Marge left by the alley door; Yuki says nobody came or went.
    // Flatly incompatible to a reader — and still not flagged, because neither
    // witness declared who they were talking about. That is correct now: an
    // observation with no subject has nothing to be compared against, and
    // guessing a subject out of free prose is how you get false accusations.
    //
    // The authoring answer is `about`, and validateStoryline rejects a template
    // whose witnesses never use it.
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithUnattributedWitnesses();

    WorldState state;
    seedMysteryFacts(state, setup, cast);

    // Give the player both accounts, as interrogating both residents would.
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    REQUIRE(entries.size() == 3);
    CHECK(conflictingCount(entries) == 0);

    // Two speakers, two subjects — stated as an assertion, not a comment.
    std::vector<std::string> subjects;
    for (const JournalEntry& e : entries) subjects.push_back(e.fact->subject);
    CHECK(subjects[0] != subjects[1]);
}

TEST_CASE("the same resident changing their story IS flagged") {
    // Speaker keying is not useless — it catches this. Worth pinning, because
    // whatever the keying becomes, this case should keep working.
    const std::vector<Persona> cast = roster();
    MysterySetup setup = generateMystery(roster(), 4242u);
    setup.witnesses.push_back(
        {"Ray Okafor", "bakery_block", "Marge leaving by the alley", 21.5, ""});
    setup.witnesses.push_back(
        {"Ray Okafor", "coffee_block", "Marge at the coffee counter", 21.5, ""});

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    CHECK(conflictingCount(entries) == 2);  // both sides flagged
}

// ---- what the keying change made true --------------------------------------

TEST_CASE("two residents disagreeing about one person are flagged") {
    // The goal sentence of alibis-and-testimony.md, and the reason `about`
    // exists: "interrogate three residents about the same evening, get three
    // different accounts, and see the journal flag two of them as
    // contradicting each other".
    //
    // Both facts land on `marge_holloway_whereabouts` with different content,
    // so the conflict pass compares them and flags BOTH sides.
    const std::vector<Persona> cast = roster();
    MysterySetup setup = generateMystery(roster(), 4242u);
    setup.witnesses.push_back({"Ray Okafor", "bakery_block",
                               "Marge leaving by the alley door", 21.5,
                               "Marge Holloway"});
    setup.witnesses.push_back({"Yuki Tanaka", "bakery_block",
                               "nobody came or went all evening", 21.6,
                               "Marge Holloway"});

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    REQUIRE(entries.size() == 3);
    CHECK(conflictingCount(entries) == 2);

    // The mechanism, asserted rather than described: one shared subject.
    std::vector<std::string> flagged;
    for (const JournalEntry& e : entries) {
        if (e.conflicting) flagged.push_back(e.fact->subject);
    }
    REQUIRE(flagged.size() == 2);
    CHECK(flagged[0] == flagged[1]);
}

TEST_CASE("the killer's alibi collides with a sighting of the killer") {
    // The payoff case, and the single test that proves the detective mechanic
    // works end to end.
    //
    // The killer's own account and a witness's sighting share
    // `<killer>_whereabouts` with different content, so the journal flags the
    // contradiction WITHOUT anything in the game marking which one is the lie
    // — alibis-and-testimony.md:99-101. The player has to decide.
    const std::vector<Persona> cast = roster();
    MysterySetup setup = generateMystery(roster(), 4242u);
    const std::string killer = setup.killer;
    REQUIRE_FALSE(killer.empty());

    // An alibi is testimony whose subject is the speaker. No alibi field.
    setup.witnesses.push_back(
        {killer, "coffee_block", "nursing a coffee alone", 21.5, killer});
    // Someone puts them somewhere else at the same hour.
    const std::string other = (killer == "Gus Pike") ? "Theo Vance" : "Gus Pike";
    setup.witnesses.push_back(
        {other, "bakery_block", killer + " at the bakery's back door", 21.5, killer});

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    CHECK(conflictingCount(entries) == 2);

    // Nothing in the data says which account is false. If this ever starts
    // failing because a `lie` flag appeared, the game got worse, not better.
    for (const JournalEntry& e : entries) {
        CHECK(e.fact->content.find("lie") == std::string::npos);
        CHECK(e.fact->content.find("false") == std::string::npos);
    }
}

TEST_CASE("a storyline that passes validateStoryline flags a contradiction") {
    // The full round trip, and the half of the finding that was more damning:
    // validateStoryline REQUIRED a contradiction it defined in terms that
    // could never produce one, and tests pinned the demand.
    //
    // Now the validator's rule and the engine's keying are the same rule, so
    // "valid template" and "journal flags something" cannot come apart.
    StorylineDef story;
    story.id = "keying_round_trip";
    story.title = "Keying Round Trip";
    story.minResidents = 4;
    story.roles = {StorylineRole{"culprit", "killer", ""},
                   StorylineRole{"neighbour", "witness", ""},
                   StorylineRole{"rival", "red_herring", ""}};
    story.clues = {StorylineClue{1, "bakery_block", "The back door was unlocked.",
                                 "", true}};
    story.witnesses = {
        StorylineWitness{"neighbour", "bakery_block",
                         "the culprit hurrying out of the alley", 21.5, "culprit"},
        StorylineWitness{"rival", "coffee_block",
                         "nobody near the bakery all evening", 21.6, "culprit"}};

    const std::vector<Persona> cast = roster();
    REQUIRE(validateStoryline(story, static_cast<int>(cast.size())).empty());

    MysterySetup setup = generateMystery(cast, 4242u);
    const StorylineCast dealt = castStoryline(story, cast, setup, 99u);
    REQUIRE(setup.witnesses.size() == 2);
    // Two different residents testifying, which is exactly the case that used
    // to be unflaggable.
    CHECK(setup.witnesses[0].agent != setup.witnesses[1].agent);
    CHECK(setup.witnesses[0].about == dealt.residentFor("culprit"));

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }
    CHECK(conflictingCount(journalEntries(state)) == 2);
}

// ---- the shipped content, not a fixture ------------------------------------

TEST_CASE("every shipped storyline produces a flagged contradiction in play") {
    // The guard that a fixture cannot give: the templates actually in
    // storylines/ are run through the same chain --mystery runs (issue #220),
    // and the journal is checked for what a player would see.
    //
    // A template can satisfy validateStoryline and still fail here if casting,
    // seeding or keying drift apart. That gap is exactly what #214 was, and it
    // survived because every test used a hand-built setup.
    namespace fs = std::filesystem;
    // The test binary runs from tests/ (make) or build/ (ctest); walk up to
    // wherever storylines/ lives.
    fs::path dir = "storylines";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    std::vector<std::string> errors;
    const std::vector<StorylineDef> shipped = loadStorylines(dir, &errors);
    CHECK(errors.empty());
    REQUIRE_FALSE(shipped.empty());

    const std::vector<Persona> cast = roster();
    for (const StorylineDef& story : shipped) {
        CAPTURE(story.id);
        REQUIRE(validateStoryline(story, static_cast<int>(cast.size())).empty());

        // Several seeds: a contradiction that only survives one cast is a
        // template that will silently produce an unsolvable match.
        for (const unsigned seed : {1u, 7u, 4242u, 99991u}) {
            CAPTURE(seed);
            MysterySetup setup = generateMystery(cast, seed);
            castStoryline(story, cast, setup, seed);

            WorldState state;
            seedMysteryFacts(state, setup, cast);
            // A player who has interrogated everyone.
            for (const KnownFact& fact : state.facts()) {
                state.grantKnowledge("player", fact.factId);
            }
            CHECK(conflictingCount(journalEntries(state)) >= 2);
        }
    }
}

TEST_CASE("no shipped storyline leaks the killer into a fact a player can read") {
    // The leak rule, checked against real content rather than a fixture. No
    // fact may name the killer AS the killer; naming them at all is fine and
    // necessary, since testimony is about people.
    namespace fs = std::filesystem;
    fs::path dir = "storylines";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    const std::vector<StorylineDef> shipped = loadStorylines(dir, nullptr);
    const std::vector<Persona> cast = roster();

    for (const StorylineDef& story : shipped) {
        CAPTURE(story.id);
        for (const unsigned seed : {1u, 7u, 4242u, 99991u}) {
            CAPTURE(seed);
            MysterySetup setup = generateMystery(cast, seed);
            castStoryline(story, cast, setup, seed);

            WorldState state;
            seedMysteryFacts(state, setup, cast);
            for (const KnownFact& fact : state.facts()) {
                const std::string& text = fact.content;
                CAPTURE(text);
                for (const char* tell : {"killer", "murderer", "the guilty",
                                         "did it", "is the one"}) {
                    CHECK(text.find(tell) == std::string::npos);
                }
            }
        }
    }
}
