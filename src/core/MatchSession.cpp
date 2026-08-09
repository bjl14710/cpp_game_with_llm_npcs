#include "MatchSession.hpp"

#include <algorithm>

namespace llm_npc {

// Scaffolded stubs (plan: networked-match, step 1). Nothing seats, nothing
// starts, and livingPlayers() is empty — so a caller wiring this up early gets
// an obviously dead session rather than a subtly wrong one. Fill these in and
// un-skip tests/test_match_session.cpp in the same commit.
//
// find() and the state reads ship real: they are a linear scan over at most
// four seats, and having them work from the first commit means step 2 can build
// the lobby message encoding against a session that answers questions.

const LobbyMember* MatchSession::find(int playerId) const {
    for (const LobbyMember& member : members_) {
        if (member.playerId == playerId) return &member;
    }
    return nullptr;
}

bool MatchSession::addPlayer(int playerId, std::string name) {
    (void)playerId;
    (void)name;
    // TODO(netmatch step 1): refuse when state_ != Lobby — that IS the
    // late-join rule, and putting it here rather than in NetServer keeps it in
    // one testable place. Refuse when members_.size() >= kMaxPlayers. Refuse a
    // duplicate playerId rather than seating the same person twice.
    return false;
}

void MatchSession::removePlayer(int playerId) {
    (void)playerId;
    // TODO(netmatch step 1): erase in Lobby. Mid-match, the player is GONE
    // rather than dead — they leave livingPlayers() immediately so unanimity is
    // recomputed against who remains, per the vote plan.
    //
    // If that empties livingPlayers(), the match ENDS rather than running with
    // nobody in it.
}

void MatchSession::setReady(int playerId, bool ready) {
    (void)playerId;
    (void)ready;
    // TODO(netmatch step 1): no-op for an unknown id and outside Lobby.
}

bool MatchSession::canStart() const {
    // TODO(netmatch step 1): Lobby, at least one member, and every member
    // ready. "At least one" rather than "at least two" on purpose — a
    // single-player match starts the same way a multiplayer one does, which is
    // the decision taken 2026-08-08.
    return false;
}

bool MatchSession::start() {
    // TODO(netmatch step 1): return false unless canStart(). On success move to
    // InMatch and mark everyone alive. The roster is FIXED from this moment.
    return false;
}

std::vector<int> MatchSession::livingPlayers() const {
    // TODO(netmatch step 1): ascending ids of members who are alive and still
    // connected. ASCENDING is load-bearing, not tidiness — the retaliation
    // fallback is "lowest living id", and a sorted result makes that front()
    // instead of a search that could silently pick differently.
    return {};
}

void MatchSession::markDead(int playerId) {
    (void)playerId;
    // TODO(netmatch step 1): clear `alive`, keep the seat. Ending the match
    // when the last living player dies belongs here too.
}

int MatchSession::retaliationTargetFor(int nominatorId) const {
    (void)nominatorId;
    // TODO(netmatch step 1): the nominator when they are still living;
    // otherwise the lowest living id; -1 when nobody is left.
    //
    // Deterministic and never random. A random fallback would be untestable
    // and would make the same match replay differently, which the mystery
    // generator went to some trouble to avoid.
    return -1;
}

void MatchSession::endMatch() {
    // TODO(netmatch step 1): move to Ended. Idempotent — a win that arrives
    // just as the last player disconnects must not double-fire.
}

void MatchSession::returnToLobby() {
    // TODO(netmatch step 1): back to Lobby, clear ready flags, revive the dead,
    // keep the roster.
}

}  // namespace llm_npc
