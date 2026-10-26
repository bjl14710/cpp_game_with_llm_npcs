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

// ---- directive tags while streaming (issue #326) ---------------------------

TEST_CASE("a tag split across deltas never reaches streamingText") {
    DialogueSession s;
    s.open(0);
    s.submitted(11);
    for (const char* d : {"Hi [[MO", "OD: happy]]"}) {
        CHECK(s.deltaArrived(11, d));
        CHECK(s.streamingText().find("[[") == std::string::npos);
        CHECK(s.streamingText().find("MOOD") == std::string::npos);
    }
    CHECK(s.streamingText() == "Hi ");
    CHECK(s.lastDisplayDelta().empty());  // the second delta was all tag
}

TEST_CASE("a single-bracket tag is held and dropped; ordinary brackets stream") {
    DialogueSession s;
    s.open(0);
    s.submitted(12);
    CHECK(s.deltaArrived(12, "[sigh] okay "));
    CHECK(s.lastDisplayDelta() == "[sigh] okay ");
    CHECK(s.deltaArrived(12, "[MOO"));
    CHECK(s.streamingText() == "[sigh] okay ");
    CHECK(s.deltaArrived(12, "D: sad] then"));
    CHECK(s.lastDisplayDelta() == " then");
    CHECK(s.streamingText() == "[sigh] okay  then");
}

TEST_CASE("held tag text does not leak into the next request's stream") {
    DialogueSession s;
    s.open(0);
    s.submitted(13);
    s.deltaArrived(13, "Ok [[MOOD: ha");
    CHECK(s.replyArrived(13, true));
    s.submitted(14);
    CHECK(s.deltaArrived(14, "ppy]] fresh"));
    // A stale fragment would have completed a tag and eaten the text.
    CHECK(s.streamingText() == "ppy]] fresh");
}
