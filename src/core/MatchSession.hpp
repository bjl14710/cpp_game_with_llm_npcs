#pragma once

#include <string>
#include <vector>

#include "NetMessage.hpp"  // kMaxPlayers

namespace llm_npc {

// Who is in a match and what state it is in (plan: networked-match, step 1).
//
// PURE LIFECYCLE. No sockets, no messages, no UI — this is the thing the server
// consults to answer "who is still playing?", and it is testable with no
// transport at all. Message types and the lobby screen are step 2; wiring the
// vote through this is step 5.
//
//   Lobby --(host starts)--> InMatch --(win / loss / all gone)--> Ended --> Lobby
//
// The SERVER owns this. Clients render what they are told and never infer
// state — a client that computes its own phase is the bug this whole plan is
// arranged to prevent.
//
// THE HOST IS TRUSTED, and the plan says so out loud. A host can cheat. All the
// anti-cheat effort goes into making sure no NON-HOST client can obtain the
// answer, which is why nothing about the mystery lives in this type: the
// session knows who is playing, never what they are playing for.

enum class SessionState {
    Lobby,    // gathering players; membership can change
    InMatch,  // roster is FIXED; late joins are refused
    Ended,    // outcome applied; waiting to return to the lobby
};

// One seat. `playerId` is the same id NetServer assigns; the host is 0.
struct LobbyMember {
    int playerId = -1;
    std::string name;
    bool ready = false;
    // Cleared when the player dies to retaliation or the accusation. Dead
    // players spectate; they do not leave the roster, because the vote needs a
    // stable id space and a corpse is still a former suspect.
    bool alive = true;
};

class MatchSession {
   public:
    // ---- lobby ------------------------------------------------------------

    // Seats a player. Returns false when the lobby is full (kMaxPlayers) or the
    // session is not in Lobby.
    //
    // THAT SECOND CASE IS THE LATE-JOIN REFUSAL, and it belongs here rather
    // than in NetServer so it is decided in one testable place. `Welcome`
    // already carries a refusal reason, so the transport needs no new field.
    bool addPlayer(int playerId, std::string name);

    // Removes a player in the lobby, or marks them gone mid-match. A
    // disconnect is FINAL — reconnect is out of scope, so this never has to
    // reserve the seat.
    void removePlayer(int playerId);

    void setReady(int playerId, bool ready);

    // At least one player, and everyone present is ready.
    bool canStart() const;

    // Locks the roster and moves to InMatch. Returns false when canStart() is
    // false, so a caller cannot start an empty or unready match by accident.
    bool start();

    // ---- during a match ---------------------------------------------------

    // Ids of players still alive, ascending. Ascending because the retaliation
    // fallback is "lowest living id" and a sorted list makes that the front
    // element rather than a search.
    std::vector<int> livingPlayers() const;

    // Marks a player dead. They stay in the roster and spectate.
    void markDead(int playerId);

    // Who pays when a wrong accusation resolves.
    //
    // Normally the nominator. When the nominator has disconnected or died
    // before resolution it is the LOWEST LIVING PLAYER ID — deterministic and
    // never random, which is what makes it testable. Returns -1 when nobody is
    // left to pay.
    int retaliationTargetFor(int nominatorId) const;

    // True once the match cannot continue: someone won, someone lost, or every
    // player has gone.
    bool ended() const { return state_ == SessionState::Ended; }

    // Ends the match early — a win, a loss, or the host quitting. Idempotent.
    void endMatch();

    // Back to Lobby, clearing ready flags and reviving the dead. The roster
    // survives: the people who just finished a match are the people most likely
    // to start another.
    void returnToLobby();

    // ---- reads ------------------------------------------------------------

    SessionState state() const { return state_; }
    const std::vector<LobbyMember>& members() const { return members_; }
    const LobbyMember* find(int playerId) const;

   private:
    SessionState state_ = SessionState::Lobby;
    // Insertion order, which is join order. Small and fixed at kMaxPlayers, so
    // a vector beats a map and keeps iteration order obvious in tests.
    std::vector<LobbyMember> members_;
};

}  // namespace llm_npc
