// Does the detective mechanic actually work end to end? (No: see below.)
//
// This file exists to force one decision before four plans are issued against
// opposite answers to it. It runs against MERGED code only — no new production
// code, no unbuilt dependency, no cutscene system.
//
// THE FINDING
// -----------
// `Journal.hpp` detects a contradiction as "one subject, more than one
// content", and its inner loop BREAKS the moment the subject changes
// (Journal.hpp:41). So two facts can only ever conflict if they share a
// subject.
//
// `seedMysteryFacts` keys a witness fact on the SPEAKER —
// `normalizeSubject(witness.agent + " testimony")` (Mystery.cpp:202). And
// `castStoryline` deals a unique resident to every slot (Storyline.cpp:356-384),
// so two witnesses are always two different agents, always two different
// subjects.
//
// Therefore: **two residents can never contradict each other.** The journal
// flags nothing, in every match, no matter what the storyline authors.
//
// Worse, `validateStoryline` REQUIRES every template to contain a
// contradiction and defines it as two witnesses in the same zone within a half
// hour saying different things (Storyline.cpp:296-306) — with a comment calling
// it "the closest structural proxy for the subject collision seedMysteryFacts
// produces". It is not a proxy. It measures a thing that cannot produce a
// collision. That rule is pinned by tests/test_storyline.cpp, so the wrong
// invariant is merged AND tested.
//
// Both comments are mine, from #179 and #187.
//
// THE DECISION THIS FORCES
// ------------------------
// Is a testimony fact's subject the SPEAKER or the TOPIC?
//
//   speaker (today)  "marge_holloway_testimony"
//                    Catches one person changing their story across a match.
//                    Cannot catch two people disagreeing.
//
//   topic (proposed) "marge_holloway_whereabouts"
//                    alibis-and-testimony.md:92-101 assumes this. Two residents
//                    claiming different things about where Marge was collide,
//                    and the journal flags both sides — which is the mechanic
//                    the whole mode is built on.
//
// The topic keying is almost certainly right: the plan's goal sentence is
// "interrogate three residents about the same evening, get three different
// accounts, and see the journal flag two of them as contradicting each other".
// That is unreachable under speaker keying.
//
// It is a real decision though, not a typo, so this file CHARACTERISES the
// current behaviour rather than asserting the desired one. The desired
// assertions are the skipped cases at the bottom — un-skip them in the commit
// that changes the keying.
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

// Two residents giving DIFFERENT accounts of the same moment — the exact shape
// validateStoryline demands, and the exact shape the mode is built on.
MysterySetup setupWithDisagreeingWitnesses() {
    MysterySetup setup = generateMystery(roster(), 4242u);
    setup.witnesses.push_back(
        {"Ray Okafor", "bakery_block", "Marge leaving by the alley door", 21.5});
    setup.witnesses.push_back(
        {"Yuki Tanaka", "bakery_block", "nobody came or went all evening", 21.6});
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
    // The setup half works exactly as intended.
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithDisagreeingWitnesses();

    WorldState state;
    seedMysteryFacts(state, setup, cast);

    // One death fact plus one per witness.
    CHECK(state.facts().size() == 3);
}

TEST_CASE("CHARACTERISATION: two residents disagreeing are NOT flagged") {
    // THE BUG, pinned so it cannot be fixed by accident and so the fix has a
    // test that visibly flips.
    //
    // Ray says Marge left by the alley door. Yuki says nobody came or went.
    // These are flatly incompatible accounts of one moment, granted to one
    // reader — and the journal reports no conflict at all.
    const std::vector<Persona> cast = roster();
    const MysterySetup setup = setupWithDisagreeingWitnesses();

    WorldState state;
    seedMysteryFacts(state, setup, cast);

    // Give the player both accounts, as interrogating both residents would.
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    REQUIRE(entries.size() == 3);

    // Speaker keying: two subjects, so the conflict pass never compares them.
    CHECK(conflictingCount(entries) == 0);

    // And here is why, stated as an assertion rather than a comment.
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
        {"Ray Okafor", "bakery_block", "Marge leaving by the alley", 21.5});
    setup.witnesses.push_back(
        {"Ray Okafor", "coffee_block", "Marge at the coffee counter", 21.5});

    WorldState state;
    seedMysteryFacts(state, setup, cast);
    for (const KnownFact& fact : state.facts()) {
        state.grantKnowledge("player", fact.factId);
    }

    const std::vector<JournalEntry> entries = journalEntries(state);
    CHECK(conflictingCount(entries) == 2);  // both sides flagged
}

TEST_CASE("a storyline that PASSES validateStoryline still flags nothing" *
          doctest::skip()) {
    // TODO(keying): the second half of the finding, and the more damning one.
    //
    // validateStoryline REQUIRES a contradiction and defines it as two
    // witnesses, same zone, within a half hour, different observations
    // (Storyline.cpp:296-306). Build such a template, cast it, seed it, and
    // this is what happens: the template is valid, the facts are on the bus,
    // and journalEntries flags nothing — because castStoryline gave the two
    // witness slots to two different residents.
    //
    // Skipped because it needs a full parse+validate+cast round trip and the
    // characterisation above already makes the point at a third of the size.
    // Un-skip it with the fix.
}

// ---- what must become true (un-skip with the keying change) ---------------

TEST_CASE("DESIRED: two residents disagreeing about one person are flagged" *
          doctest::skip()) {
    // TODO(keying): the goal sentence of alibis-and-testimony.md — "interrogate
    // three residents about the same evening, get three different accounts, and
    // see the journal flag two of them as contradicting each other".
    //
    // Under topic keying both facts land on `marge_holloway_whereabouts` with
    // different content, and journalEntries sets conflicting = true on BOTH
    // sides. Assert conflictingCount(entries) == 2.
    //
    // Note the subject has to name the person the testimony is ABOUT, which
    // means StorylineWitness needs a field for it — `observed` is free prose
    // today and cannot be parsed into a subject. That is the real cost of the
    // fix and it belongs in the issue, not in this comment.
}

TEST_CASE("DESIRED: the killer's alibi collides with a sighting of the killer" *
          doctest::skip()) {
    // TODO(keying): the payoff case. The killer's own account and a witness's
    // sighting share `<killer>_whereabouts` with different content, so the
    // journal flags the contradiction WITHOUT anything in the game ever
    // marking which one is the lie — alibis-and-testimony.md:99-101.
    //
    // This is the single test that would prove the detective mechanic works.
    // Nothing currently seeds the killer's alibi: MysterySetup has no alibi
    // field and StorylineDef has no way to express one. That gap is the other
    // half of the issue.
}
