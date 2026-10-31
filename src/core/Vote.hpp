#pragma once

#include <map>
#include <string>
#include <vector>

namespace llm_npc {

// The end-of-day vote: living players nominate one resident as the killer and
// must agree unanimously (issue #221, plan .claude/plans/vote-and-retaliation.md).
//
// WHY UNANIMOUS, in one number: kMaxPlayers = 4. Four voters choosing among
// ~20 suspects makes a 1-1-1-1 split the NORMAL outcome of plurality, not an
// edge case — most days would resolve to "tie" anyway. Unanimity deletes the
// entire tie branch, turns the vote into a negotiation, and degrades perfectly
// to single-player, where one living player is unanimous by definition and the
// same code path serves both modes.

enum class VoteOutcome {
    NoAccusation,  // nobody was named, or not everyone agreed
    Correct,       // the accused is the killer; the players win
    Wrong,         // the accused is innocent, and dies for it
};

// One day's ballot.
struct Ballot {
    std::string nominee;  // NPC persona name; empty means nobody is named
    int nominator = -1;   // player id who proposed it
    // player id -> has confirmed. Absent means "has not answered", which is
    // deliberately distinct from present-and-false ("answered, then withdrew"):
    // both block unanimity, but only the second is a decision.
    std::map<int, bool> confirmations;
};

// The result of resolving one ballot.
struct VoteResult {
    VoteOutcome outcome = VoteOutcome::NoAccusation;
    std::string accused;      // empty unless an accusation was made
    int nominator = -1;       // who proposed it
    int playerKilled = -1;    // -1 unless Wrong AND multiplayer
};

// Replaces the ballot's nomination and CLEARS EVERY CONFIRMATION.
//
// The clearing is the whole point of having a function for this. Agreeing to
// one name must never silently carry over to another — a player who confirmed
// "Marge" and looked away has not confirmed "Ray", and shipping their consent
// to a name they never saw is the worst bug this system could have.
//
// Nominating the same name again still clears: re-nomination is a fresh
// proposal, and treating it as a no-op would make the behaviour depend on
// whether two players happened to pick the same person.
void nominate(Ballot& ballot, const std::string& nominee, int nominator);

// Resolves `ballot` against the players still alive.
//
// TAKES A BOOL, NOT THE SETUP. The killer's identity is not in scope here, so
// there is exactly one place in the codebase that consults voteIsCorrect and a
// reviewer can find it by grepping one name. Everything about who dies and
// whether the town was right is decided from these four arguments.
//
// `singlePlayer` is the MATCH MODE, not `livingPlayers.size() == 1`. A
// multiplayer match whittled down to one survivor keeps the harsher
// retaliation rule; a solo match never had it. Conflating the two would
// quietly change the rules of a multiplayer match as its players died.
//
// Pure: no networking, no MysterySetup, no I/O, no randomness.
VoteResult resolveVote(const Ballot& ballot, const std::vector<int>& livingPlayers,
                       bool nomineeIsKiller, bool singlePlayer);

}  // namespace llm_npc
