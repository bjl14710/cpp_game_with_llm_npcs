// Match lifecycle: lobby, fixed roster, disconnects, end conditions
// (plan: networked-match, step 1).
//
// All lifecycle cases SKIPPED — the session stubs seat nobody and start
// nothing. The find() cases are LIVE; that lookup ships with the scaffold so
// step 2 can build lobby message encoding against a session that answers
// questions.
//
// EVERY DISCONNECT EDGE CASE IS DECIDED HERE, deliberately. The plan puts them
// in step 1 rather than leaving them to the networking code, because a
// disconnect rule discovered during integration is a rule that gets invented
// under pressure.
#include <vector>

#include "MatchSession.hpp"
#include "doctest.h"

using namespace llm_npc;

// ---- lookup (LIVE) --------------------------------------------------------

TEST_CASE("find returns nullptr on an empty session") {
    const MatchSession session;
    CHECK(session.find(0) == nullptr);
    CHECK(session.find(-1) == nullptr);
}

TEST_CASE("a new session starts in the lobby with nobody in it") {
    const MatchSession session;
    CHECK(session.state() == SessionState::Lobby);
    CHECK(session.members().empty());
    CHECK_FALSE(session.ended());
}

// ---- lobby membership -----------------------------------------------------

TEST_CASE("players can be seated and found" * doctest::skip()) {
    // TODO(netmatch step 1): add four, find each by id, and check join order is
    // preserved — the lobby list is what the UI renders.
}

TEST_CASE("the lobby refuses a fifth player" * doctest::skip()) {
    // TODO(netmatch step 1): kMaxPlayers is 4 including the host. The fifth
    // addPlayer returns false and seats nobody.
}

TEST_CASE("a duplicate player id is refused, not seated twice" *
          doctest::skip()) {
    // TODO(netmatch step 1): two seats with the same id would break every
    // lookup and give one person two votes.
}

TEST_CASE("a player who leaves the lobby frees their seat" * doctest::skip()) {
    // TODO(netmatch step 1): remove in Lobby erases. A disconnect is FINAL —
    // reconnect is out of scope — so the seat never needs reserving.
}

// ---- starting -------------------------------------------------------------

TEST_CASE("a match cannot start with nobody in it" * doctest::skip()) {
    // TODO(netmatch step 1): canStart() and start() both false on an empty
    // lobby.
}

TEST_CASE("a match cannot start until everyone is ready" * doctest::skip()) {
    // TODO(netmatch step 1): three ready and one not is not startable.
}

TEST_CASE("one ready player is enough to start" * doctest::skip()) {
    // TODO(netmatch step 1): at least ONE, not at least two. Single-player
    // starts the same way multiplayer does — the decision taken 2026-08-08 —
    // so the session must not require a second person.
}

TEST_CASE("starting locks the roster and marks everyone alive" *
          doctest::skip()) {
    // TODO(netmatch step 1): state becomes InMatch and every member is alive.
}

// ---- THE LATE-JOIN RULE ---------------------------------------------------

TEST_CASE("a player who connects mid-match is refused" * doctest::skip()) {
    // TODO(netmatch step 1): addPlayer returns false once state is InMatch,
    // even with seats free. The roster is fixed from match start and the lobby
    // is where you get in.
    //
    // The refusal lives HERE rather than in NetServer so it is decided once and
    // tested without a socket. `Welcome` already carries a reason field, so the
    // transport needs nothing new.
}

// ---- disconnects and death ------------------------------------------------

TEST_CASE("a disconnect during a match removes the player from livingPlayers" *
          doctest::skip()) {
    // TODO(netmatch step 1): immediately, so unanimity is recomputed against
    // who remains rather than against who started.
}

TEST_CASE("livingPlayers is ascending" * doctest::skip()) {
    // TODO(netmatch step 1): load-bearing, not tidiness. The retaliation
    // fallback is "lowest living id", so sorted means front() rather than a
    // search that could silently pick differently. Seat out of order and check.
}

TEST_CASE("a dead player keeps their seat but leaves livingPlayers" *
          doctest::skip()) {
    // TODO(netmatch step 1): the dead spectate. They stay in members() because
    // the vote needs a stable id space and a corpse is still a former suspect.
}

TEST_CASE("retaliation falls on the nominator when they are still alive" *
          doctest::skip()) {
    // TODO(netmatch step 1): the ordinary case.
}

TEST_CASE("retaliation falls on the lowest living id when the nominator is gone" *
          doctest::skip()) {
    // TODO(netmatch step 1): THE RULE THE VOTE PLAN DECIDED AND THIS PLAN
    // IMPLEMENTS. Deterministic, never random — a random fallback would be
    // untestable and would make the same match replay differently, which the
    // mystery generator went to some trouble to avoid.
    //
    // Cover both ways the nominator can be gone: disconnected, and dead.
}

TEST_CASE("retaliation returns -1 when nobody is left to pay" *
          doctest::skip()) {
    // TODO(netmatch step 1): not a crash and not player 0 by accident.
}

// ---- ending ---------------------------------------------------------------

TEST_CASE("the match ends when every player disconnects" * doctest::skip()) {
    // TODO(netmatch step 1): rather than running empty. Cheap to get wrong —
    // the server would happily tick a match with no players in it forever.
}

TEST_CASE("endMatch is idempotent" * doctest::skip()) {
    // TODO(netmatch step 1): a win arriving as the last player disconnects must
    // not double-fire.
}

TEST_CASE("returning to the lobby clears ready and revives the dead" *
          doctest::skip()) {
    // TODO(netmatch step 1): the roster survives — the people who just finished
    // a match are the people most likely to start another.
}

TEST_CASE("a session that returned to the lobby can start again" *
          doctest::skip()) {
    // TODO(netmatch step 1): the full cycle. Lobby -> InMatch -> Ended ->
    // Lobby -> InMatch, with the same members.
}

// ---- what this type must NOT know ----------------------------------------

TEST_CASE("the session exposes no mystery state" * doctest::skip()) {
    // TODO(netmatch step 1): a structural assertion, not a behavioural one.
    //
    // MatchSession knows who is playing and never what they are playing for.
    // The moment a killer, a victim or a MysterySetup appears on this type, the
    // server has somewhere convenient to leak it from — and the leak test in
    // step 6 inspects the wire, which is far too late to catch a design
    // mistake made here.
    //
    // Write it as a compile-time check over the public surface if doctest
    // allows, or as a reviewed comment if not — either way, say it out loud.
}
