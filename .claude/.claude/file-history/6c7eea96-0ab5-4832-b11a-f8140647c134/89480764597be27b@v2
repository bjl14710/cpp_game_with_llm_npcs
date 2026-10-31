// Tests for the DialogueSession state machine.
#include "DialogueSession.hpp"
#include "doctest.h"

using llm_npc::DialogueSession;
using State = llm_npc::DialogueSession::State;

TEST_CASE("session starts roaming and opens onto an NPC") {
    DialogueSession s;
    CHECK(s.state() == State::Roaming);
    CHECK_FALSE(s.isOpen());
    CHECK(s.npcIndex() == -1);

    s.open(3);
    CHECK(s.state() == State::Talking);
    CHECK(s.isOpen());
    CHECK(s.npcIndex() == 3);
}

TEST_CASE("happy path: submit, stream deltas, finalize") {
    DialogueSession s;
    s.open(0);
    s.submitted(42);
    CHECK(s.state() == State::WaitingReply);
    CHECK(s.pendingRequestId() == 42);

    CHECK(s.deltaArrived(42, "Hel"));
    CHECK(s.state() == State::Streaming);
    CHECK(s.deltaArrived(42, "lo!"));
    CHECK(s.streamingText() == "Hello!");

    CHECK(s.replyArrived(42, true));
    CHECK(s.state() == State::Talking);  // ready for the next line
    CHECK(s.streamingText().empty());
    CHECK(s.pendingRequestId() == 0);
}

TEST_CASE("deltas with the wrong id are rejected") {
    DialogueSession s;
    s.open(0);
    s.submitted(7);
    CHECK_FALSE(s.deltaArrived(99, "stale"));
    CHECK(s.state() == State::WaitingReply);
    CHECK(s.streamingText().empty());
    CHECK_FALSE(s.replyArrived(99, true));
    CHECK(s.state() == State::WaitingReply);
}

TEST_CASE("deltas are ignored while roaming or merely talking") {
    DialogueSession s;
    CHECK_FALSE(s.deltaArrived(1, "x"));
    s.open(0);
    CHECK_FALSE(s.deltaArrived(1, "x"));  // nothing submitted yet
}

TEST_CASE("error replies also return the session to talking") {
    DialogueSession s;
    s.open(0);
    s.submitted(5);
    s.deltaArrived(5, "partial");
    CHECK(s.replyArrived(5, false));
    CHECK(s.state() == State::Talking);
    CHECK(s.streamingText().empty());  // partial text discarded
}

TEST_CASE("close mid-stream resets everything") {
    DialogueSession s;
    s.open(2);
    s.submitted(9);
    s.deltaArrived(9, "half a sent");
    s.close();
    CHECK(s.state() == State::Roaming);
    CHECK(s.npcIndex() == -1);
    CHECK(s.pendingRequestId() == 0);
    CHECK(s.streamingText().empty());
    // Late traffic for the abandoned request is rejected.
    CHECK_FALSE(s.deltaArrived(9, "ence"));
    CHECK_FALSE(s.replyArrived(9, true));
}

TEST_CASE("submitted is only honored while talking") {
    DialogueSession s;
    s.submitted(1);  // roaming: ignored
    CHECK(s.pendingRequestId() == 0);
    s.open(0);
    s.submitted(2);
    s.submitted(3);  // already waiting: ignored
    CHECK(s.pendingRequestId() == 2);
}
