// Typographic punctuation folded to what the renderer can draw (bug 2).
//
// THE REPORTED BUG WAS "don?t" IN DIALOGUE, and the obvious diagnosis is
// wrong. It is not a lossy encoding conversion anywhere in the pipeline — the
// UTF-8 bytes survive the HTTP read, the JSON parse, SQLite and every
// std::string in between. raylib substitutes '?' at DRAW time for any
// codepoint the font lacks:
//
//     // raylib/src/rtext.c — GetGlyphIndex
//     if (font.glyphs[i].value == 63) fallbackIndex = i;   // 63 == '?'
//
// So a literal '?' is exactly what a missing glyph looks like in this engine.
#include <string>

#include "AsciiText.hpp"
#include "doctest.h"

using llm_npc::toRenderableAscii;

namespace {

// Named rather than inlined, because a hex escape swallows any hex digit that
// follows it: "em\xE2\x80\x94dash" parses \x94d as one escape and does not
// compile. Naming them sidesteps that entirely and reads better.
constexpr const char* kLSQuo = "\xE2\x80\x98";  // U+2018
constexpr const char* kRSQuo = "\xE2\x80\x99";  // U+2019 the apostrophe
constexpr const char* kLDQuo = "\xE2\x80\x9C";  // U+201C
constexpr const char* kRDQuo = "\xE2\x80\x9D";  // U+201D
constexpr const char* kEnDash = "\xE2\x80\x93";  // U+2013
constexpr const char* kEmDash = "\xE2\x80\x94";  // U+2014
constexpr const char* kEllipsis = "\xE2\x80\xA6";  // U+2026
constexpr const char* kNbsp = "\xC2\xA0";  // U+00A0
constexpr const char* kLGuil = "\xC2\xAB";  // U+00AB
constexpr const char* kRGuil = "\xC2\xBB";  // U+00BB
constexpr const char* kEAcute = "\xC3\xA9";  // U+00E9

std::string s(const char* a) { return std::string(a); }

}  // namespace

TEST_CASE("the reported case: a curly apostrophe becomes a straight one") {
    CHECK(toRenderableAscii("don" + s(kRSQuo) + "t") == "don't");
    CHECK(toRenderableAscii("I" + s(kRSQuo) + "ve seen worse") == "I've seen worse");
}

TEST_CASE("the whole punctuation set a model reaches for, not just apostrophes") {
    // The brief is right that apostrophes are only what you notice first.
    CHECK(toRenderableAscii(s(kLDQuo) + "quoted" + kRDQuo) == "\"quoted\"");
    CHECK(toRenderableAscii(s(kLSQuo) + "single" + kRSQuo) == "'single'");
    CHECK(toRenderableAscii("em" + s(kEmDash) + "dash") == "em-dash");
    CHECK(toRenderableAscii("en" + s(kEnDash) + "dash") == "en-dash");
    CHECK(toRenderableAscii("wait" + s(kEllipsis)) == "wait...");
    CHECK(toRenderableAscii("a" + s(kNbsp) + "space") == "a space");
    CHECK(toRenderableAscii(s(kLGuil) + "guillemets" + kRGuil) == "\"guillemets\"");
}

TEST_CASE("every folded result is pure ASCII the font can draw") {
    const std::string kitchenSink = "She said " + s(kLDQuo) + "don" + kRSQuo + "t" +
                                    kRDQuo + " " + kEmDash + " then left" + kEllipsis +
                                    " or" + kNbsp + "so" + kEnDash + "they say.";
    const std::string folded = toRenderableAscii(kitchenSink);
    CAPTURE(folded);
    for (const char c : folded) {
        CHECK(static_cast<unsigned char>(c) <= 126u);
        CHECK(static_cast<unsigned char>(c) >= 32u);
    }
}

TEST_CASE("repeated occurrences all fold, not just the first") {
    CHECK(toRenderableAscii("it" + s(kRSQuo) + "s the cat" + kRSQuo + "s owner" +
                            kRSQuo + "s") == "it's the cat's owner's");
}

TEST_CASE("plain ASCII is returned unchanged, byte for byte") {
    // The overwhelmingly common case must not be perturbed.
    const std::string plain = "Marge said: \"I don't know.\" Table #3 -- 21:40.";
    CHECK(toRenderableAscii(plain) == plain);
    CHECK(toRenderableAscii("").empty());
}

TEST_CASE("ACCENTED LETTERS ARE LEFT ALONE, and still render as '?'") {
    // Deliberate, and the honest half of this fix. There is no lossless ASCII
    // for an accented letter; folding it would mangle a resident's name to
    // paper over a font problem. These still show as '?' until the font atlas
    // lands (#236), and this case exists so that is a recorded decision rather
    // than something the next person rediscovers.
    const std::string accented = "Ad" + s(kEAcute) + "wal" + kEAcute + " saw it";
    CHECK(toRenderableAscii(accented) == accented);

    bool anyNonAscii = false;
    for (const char c : toRenderableAscii(accented)) {
        if (static_cast<unsigned char>(c) > 126u) anyNonAscii = true;
    }
    CHECK(anyNonAscii);  // i.e. the font will substitute '?' for these
}
