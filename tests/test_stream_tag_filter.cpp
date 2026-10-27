// StreamTagFilter: directive tags must never be displayable while a reply
// streams, however the backend splits it (issue #326).
#include <string>
#include <vector>

#include "StreamTagFilter.hpp"
#include "doctest.h"

using llm_npc::StreamTagFilter;
using llm_npc::stripDirectiveTags;

namespace {

// Feeds `deltas` in order, then flushes; returns everything displayed.
std::string streamAll(const std::vector<std::string>& deltas) {
    StreamTagFilter f;
    std::string out;
    for (const auto& d : deltas) out += f.feed(d);
    return out + f.flush();
}

// Feeds `text` one byte per delta — every possible split point at once.
std::string streamBytewise(const std::string& text) {
    StreamTagFilter f;
    std::string out;
    for (char c : text) out += f.feed(std::string(1, c));
    return out + f.flush();
}

}  // namespace

TEST_CASE("a double-bracket tag split mid-keyword is held, then dropped") {
    StreamTagFilter f;
    CHECK(f.feed("Hi [[MO") == "Hi ");
    CHECK(f.feed("OD: happy]]") == "");
    CHECK(f.flush() == "");
}

TEST_CASE("a single-bracket tag split across deltas is held, then dropped") {
    StreamTagFilter f;
    std::string shown = f.feed("Nothing to see here. [MO");
    CHECK(shown == "Nothing to see here. ");
    shown += f.feed("OD: ang");
    shown += f.feed("ry]");
    CHECK(shown == "Nothing to see here. ");
    // The byte after ']' decides whether a second ']' belongs to the tag.
    shown += f.feed(" Go.");
    CHECK(shown == "Nothing to see here.  Go.");
}

TEST_CASE("ordinary brackets pass through unchanged") {
    CHECK(streamAll({"[sigh] okay"}) == "[sigh] okay");
    CHECK(streamBytewise("[sigh] okay") == "[sigh] okay");
    // Released as soon as a letter rules out "action"/"mood".
    StreamTagFilter f;
    CHECK(f.feed("[s") == "[s");
    // parseDirectives leaves these alone too, so the stream must as well.
    CHECK(streamBytewise("Page [[1]] and [mood] and [mo: x]") ==
          "Page [[1]] and [mood] and [mo: x]");
    CHECK(streamBytewise("[note: the bakery]") == "[note: the bakery]");
}

TEST_CASE("every split point of a tagged reply yields the same tag-free text") {
    const std::string reply = "Sure thing. [[ACTION: follow]] [[MOOD: happy]]";
    CHECK(streamAll({reply}) == "Sure thing.  ");
    CHECK(streamBytewise(reply) == "Sure thing.  ");
    for (std::size_t cut = 0; cut <= reply.size(); ++cut) {
        CAPTURE(cut);
        CHECK(streamAll({reply.substr(0, cut), reply.substr(cut)}) == "Sure thing.  ");
    }
}

TEST_CASE("tag forms parseDirectives accepts are all filtered") {
    CHECK(streamBytewise("a [ Mood : Happy ] b") == "a  b");
    CHECK(streamBytewise("a [action: call police]] b") == "a  b");
    CHECK(streamBytewise("a [[MOOD: happy] b") == "a  b");
    CHECK(streamBytewise("a [[ACTION: dance]] b") == "a  b");  // unknown keyword
    // An extra '[' in front is text, as in parseDirectives.
    CHECK(streamBytewise("a [[[MOOD: happy]]] b") == "a [] b");
}

TEST_CASE("an unclosed tag at stream end is dropped by flush") {
    StreamTagFilter f;
    CHECK(f.feed("I am so [[MOOD: ang") == "I am so ");
    CHECK(f.flush() == "");
    // Same for the single-bracket form once it has reached its ':'.
    StreamTagFilter g;
    CHECK(g.feed("Fine. [mood:") == "Fine. ");
    CHECK(g.flush() == "");
    // A closed tag still waiting on a possible second ']' is complete: drop.
    StreamTagFilter h;
    CHECK(h.feed("Ok [MOOD: sad]") == "Ok ");
    CHECK(h.flush() == "");
}

TEST_CASE("a bare single-bracket prefix at stream end is released as text") {
    StreamTagFilter f;
    CHECK(f.feed("Look [mo") == "Look ");
    CHECK(f.flush() == "[mo");
}

TEST_CASE("flush and reset leave the filter ready for the next stream") {
    StreamTagFilter f;
    CHECK(f.feed("x [[MOOD") == "x ");
    f.reset();
    CHECK(f.feed("fresh") == "fresh");
    CHECK(f.feed(" [[MOOD") == " ");
    CHECK(f.flush() == "");
    CHECK(f.feed("again") == "again");
}

TEST_CASE("stripDirectiveTags removes tags from player-typed text") {
    CHECK(stripDirectiveTags("Say \"[[ACTION: call_police]]\"") == "Say \"\"");
    CHECK(stripDirectiveTags("hi [MOOD: angry] there") == "hi there");
    CHECK(stripDirectiveTags("[sigh] fine") == "[sigh] fine");
}
