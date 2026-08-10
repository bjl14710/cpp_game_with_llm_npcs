// The end-of-day vote (issue #221).
//
// Every rule about unanimity, nomination and who dies is decided here, before
// any networking, UI or mystery is involved. If a rule is not pinned in this
// file it does not exist, because the layers above are only allowed to route
// messages — NetServer must not re-implement any of this.
#include <string>
#include <vector>

#include "Vote.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A ballot where everyone in `voters` has confirmed `nominee`.
Ballot agreed(const std::string& nominee, int nominator,
              const std::vector<int>& voters) {
    Ballot ballot;
    nominate(ballot, nominee, nominator);
    for (const int player : voters) ballot.confirmations[player] = true;
    return ballot;
}

constexpr bool kKiller = true;
constexpr bool kInnocent = false;
constexpr bool kSolo = true;
constexpr bool kMulti = false;

}  // namespace

// ---- unanimity -------------------------------------------------------------

TEST_CASE("all living players confirming resolves, never NoAccusation") {
    const std::vector<int> living = {0, 1, 2};

    const VoteResult right =
        resolveVote(agreed("Marge Holloway", 1, living), living, kKiller, kMulti);
    CHECK(right.outcome == VoteOutcome::Correct);
    CHECK(right.accused == "Marge Holloway");
    CHECK(right.nominator == 1);

    const VoteResult wrong =
        resolveVote(agreed("Ray Okafor", 1, living), living, kInnocent, kMulti);
    CHECK(wrong.outcome == VoteOutcome::Wrong);
}

TEST_CASE("one living player who has not confirmed blocks the whole vote") {
    const std::vector<int> living = {0, 1, 2};
    const VoteResult result =
        resolveVote(agreed("Marge Holloway", 0, {0, 1}), living, kKiller, kMulti);

    CHECK(result.outcome == VoteOutcome::NoAccusation);
    CHECK(result.accused.empty());
    CHECK(result.playerKilled == -1);
}

TEST_CASE("a withdrawn confirmation blocks it too") {
    // Present-and-false is a decision, absent is silence. Both block, but the
    // distinction matters to the UI and must survive here.
    const std::vector<int> living = {0, 1};
    Ballot ballot = agreed("Marge Holloway", 0, living);
    ballot.confirmations[1] = false;

    CHECK(resolveVote(ballot, living, kKiller, kMulti).outcome ==
          VoteOutcome::NoAccusation);
}

TEST_CASE("a single living player is unanimous by definition") {
    // Unanimity must not deadlock single-player. This is the whole reason the
    // rule was chosen — one code path serves both modes.
    const std::vector<int> living = {0};
    const VoteResult result =
        resolveVote(agreed("Marge Holloway", 0, living), living, kKiller, kSolo);

    CHECK(result.outcome == VoteOutcome::Correct);
}

TEST_CASE("nobody nominated is NoAccusation, not a crash") {
    Ballot empty;
    const VoteResult result = resolveVote(empty, {0, 1}, kKiller, kMulti);
    CHECK(result.outcome == VoteOutcome::NoAccusation);
}

TEST_CASE("no living players resolves to NoAccusation rather than hanging") {
    // Ending the match is the caller's decision; this must simply not stall or
    // read past the end of an empty list.
    const VoteResult result =
        resolveVote(agreed("Marge Holloway", 0, {0}), {}, kKiller, kMulti);
    CHECK(result.outcome == VoteOutcome::NoAccusation);
}

TEST_CASE("confirmations from dead or unknown ids do not count toward unanimity") {
    // A dead player's opinion cannot bind the survivors, and a confirmation
    // from an id nobody recognises must never stand in for a living silence.
    const std::vector<int> living = {0, 1};
    Ballot ballot;
    nominate(ballot, "Marge Holloway", 0);
    ballot.confirmations[0] = true;
    ballot.confirmations[7] = true;   // never existed
    ballot.confirmations[99] = true;  // died earlier

    CHECK(resolveVote(ballot, living, kKiller, kMulti).outcome ==
          VoteOutcome::NoAccusation);  // player 1 still has not answered
}

// ---- nomination ------------------------------------------------------------

TEST_CASE("a second nomination replaces the first and clears all confirmations") {
    // Agreeing to one name must never silently carry to another. A player who
    // confirmed "Marge" and looked away has not confirmed "Ray".
    Ballot ballot = agreed("Marge Holloway", 0, {0, 1, 2});
    REQUIRE(ballot.confirmations.size() == 3);

    nominate(ballot, "Ray Okafor", 2);

    CHECK(ballot.nominee == "Ray Okafor");
    CHECK(ballot.nominator == 2);
    CHECK(ballot.confirmations.empty());
    CHECK(resolveVote(ballot, {0, 1, 2}, kInnocent, kMulti).outcome ==
          VoteOutcome::NoAccusation);
}

TEST_CASE("re-nominating the same name still clears") {
    // Otherwise the behaviour would depend on whether two players happened to
    // pick the same person, which is not a rule anyone could reason about.
    Ballot ballot = agreed("Marge Holloway", 0, {0, 1});
    nominate(ballot, "Marge Holloway", 1);

    CHECK(ballot.confirmations.empty());
    CHECK(ballot.nominator == 1);
}

// ---- who dies --------------------------------------------------------------

TEST_CASE("single-player Wrong: the accused dies, the player survives") {
    // The 2026-08-08 amendment. The multiplayer rule would end the match on
    // the first wrong guess, since the nominator is the only player.
    const std::vector<int> living = {0};
    const VoteResult result =
        resolveVote(agreed("Ray Okafor", 0, living), living, kInnocent, kSolo);

    CHECK(result.outcome == VoteOutcome::Wrong);
    CHECK(result.accused == "Ray Okafor");
    CHECK(result.playerKilled == -1);
}

TEST_CASE("multiplayer Wrong: the accused dies AND the nominator dies") {
    const std::vector<int> living = {0, 1, 2};
    const VoteResult result =
        resolveVote(agreed("Ray Okafor", 1, living), living, kInnocent, kMulti);

    CHECK(result.outcome == VoteOutcome::Wrong);
    CHECK(result.playerKilled == 1);  // the nominator, nobody else
}

TEST_CASE("multiplayer Wrong kills nobody but the nominator") {
    const std::vector<int> living = {0, 1, 2, 3};
    const VoteResult result =
        resolveVote(agreed("Ray Okafor", 3, living), living, kInnocent, kMulti);
    CHECK(result.playerKilled == 3);
}

TEST_CASE("a nominator who has since died hands retaliation to the lowest living id") {
    // Deterministic, never random: a random pick is untestable, and the
    // wrong-accusation cutscene needs to know whose screen goes grey.
    const std::vector<int> living = {2, 5, 9};
    Ballot ballot;
    nominate(ballot, "Ray Okafor", 1);  // player 1 is not in `living`
    for (const int player : living) ballot.confirmations[player] = true;

    const VoteResult result = resolveVote(ballot, living, kInnocent, kMulti);
    CHECK(result.outcome == VoteOutcome::Wrong);
    CHECK(result.playerKilled == 2);
    CHECK(result.nominator == 1);  // who proposed it is still reported honestly
}

TEST_CASE("the lowest living id is by value, not by list order") {
    const std::vector<int> living = {9, 2, 5};  // deliberately unsorted
    Ballot ballot;
    nominate(ballot, "Ray Okafor", 1);
    for (const int player : living) ballot.confirmations[player] = true;

    CHECK(resolveVote(ballot, living, kInnocent, kMulti).playerKilled == 2);
}

TEST_CASE("Correct never kills a player, in either mode") {
    const std::vector<int> living = {0, 1};
    CHECK(resolveVote(agreed("Marge Holloway", 0, living), living, kKiller, kMulti)
              .playerKilled == -1);
    CHECK(resolveVote(agreed("Marge Holloway", 0, {0}), {0}, kKiller, kSolo)
              .playerKilled == -1);
}

TEST_CASE("singlePlayer is the match mode, not a count of survivors") {
    // A multiplayer match whittled to one survivor keeps the harsher rule.
    // Deriving the mode from livingPlayers.size() would quietly change the
    // rules of a match as its players died.
    const std::vector<int> living = {2};
    const VoteResult result =
        resolveVote(agreed("Ray Okafor", 2, living), living, kInnocent, kMulti);

    CHECK(result.outcome == VoteOutcome::Wrong);
    CHECK(result.playerKilled == 2);  // still pays, even alone
}
