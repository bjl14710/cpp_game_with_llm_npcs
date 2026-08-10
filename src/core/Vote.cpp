#include "Vote.hpp"

#include <algorithm>

namespace llm_npc {

void nominate(Ballot& ballot, const std::string& nominee, int nominator) {
    ballot.nominee = nominee;
    ballot.nominator = nominator;
    ballot.confirmations.clear();
}

VoteResult resolveVote(const Ballot& ballot, const std::vector<int>& livingPlayers,
                       bool nomineeIsKiller, bool singlePlayer) {
    VoteResult result;

    // Nobody named, or nobody left to name them. Either way the day rolls over
    // with no accusation; ending a match is the caller's decision, not this
    // function's.
    if (ballot.nominee.empty() || livingPlayers.empty()) return result;

    // Unanimity over the LIVING, counted from the living list rather than from
    // the confirmation map. Counting the map would let a confirmation from a
    // player who has since died stand in for one who is still alive and
    // silent — and a dead player's opinion cannot bind the survivors.
    for (const int player : livingPlayers) {
        const auto it = ballot.confirmations.find(player);
        if (it == ballot.confirmations.end() || !it->second) return result;
    }

    result.accused = ballot.nominee;
    result.nominator = ballot.nominator;
    result.outcome = nomineeIsKiller ? VoteOutcome::Correct : VoteOutcome::Wrong;

    if (result.outcome != VoteOutcome::Wrong || singlePlayer) {
        // Single-player pays for a wrong accusation with the day and the
        // accused's life, but not the player's. The multiplayer rule would end
        // the match on the first wrong guess, since the nominator is the only
        // player — one attempt, which is not a game.
        //
        // No attempt counter is needed anywhere: one vote per day with
        // dayLimit = 3 already IS three attempts, tunable through the existing
        // match setting.
        return result;
    }

    // Multiplayer, and they got it wrong: the nominator dies with the accused.
    //
    // If they have since died, retaliation falls on the LOWEST LIVING id.
    // Deterministic on purpose — a random pick would be untestable, and the
    // wrong-accusation cutscene needs to know in advance whose screen goes
    // grey. Never let the match stall for want of someone to blame.
    const bool nominatorAlive =
        std::find(livingPlayers.begin(), livingPlayers.end(), ballot.nominator) !=
        livingPlayers.end();
    result.playerKilled =
        nominatorAlive ? ballot.nominator
                       : *std::min_element(livingPlayers.begin(), livingPlayers.end());
    return result;
}

}  // namespace llm_npc
