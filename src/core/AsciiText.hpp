#pragma once

#include <string>

namespace llm_npc {

// Folds typographic punctuation to the ASCII the renderer can actually draw.
//
// WHY THIS EXISTS, precisely, because the obvious diagnosis is wrong.
//
// A model asked for natural dialogue emits a curly apostrophe (U+2019) in
// "don't", and it reaches the screen as "don?t". That looks like a lossy
// encoding conversion — and it is not. The bytes are intact end to end; the
// UTF-8 survives the HTTP read, the JSON parse, SQLite and every std::string
// in between, and `tests/test_cutscene.cpp` asserts a caption keeps its exact
// `\xE2\x80\x94` bytes after a full parse.
//
// The substitution happens in raylib, deliberately, at draw time:
//
//     // raylib/src/rtext.c — GetGlyphIndex
//     int fallbackIndex = 0;      // Get index of fallback glyph '?'
//     if (font.glyphs[i].value == 63) fallbackIndex = i;   // 63 == '?'
//
// raylib substitutes '?' for any codepoint the font lacks, and the built-in
// font carries ASCII 32-126 and nothing else. So a literal '?' is exactly
// what a missing glyph looks like here. (In some other engines a missing
// glyph is a blank box, which is what makes this misleading.)
//
// THIS IS NOT THE REAL FIX. The real fix is a font atlas with a wider
// codepoint range, which is issue #236 and wants an asset pipeline. This
// covers the characters a model actually reaches for in English prose, which
// is all of the ones anybody has reported, with no new assets.
//
// It deliberately does NOT touch accented letters. There is no lossless ASCII
// for "é", and folding it to "e" would mangle a resident's name to hide a font
// problem. Those still render as '?' until #236 lands, and a test says so
// rather than leaving it to be discovered.
inline std::string toRenderableAscii(const std::string& text) {
    // Each entry is a UTF-8 sequence and the ASCII it folds to. Written as
    // explicit bytes rather than literals so the table cannot itself be
    // mangled by an editor helpfully "fixing" the quotes in this file.
    struct Fold {
        const char* utf8;
        const char* ascii;
    };
    static const Fold kFolds[] = {
        {"\xE2\x80\x98", "'"},    // U+2018 left single quote
        {"\xE2\x80\x99", "'"},    // U+2019 right single quote — the apostrophe
        {"\xE2\x80\x9A", "'"},    // U+201A low single quote
        {"\xE2\x80\x9C", "\""},   // U+201C left double quote
        {"\xE2\x80\x9D", "\""},   // U+201D right double quote
        {"\xE2\x80\x9E", "\""},   // U+201E low double quote
        {"\xE2\x80\x93", "-"},    // U+2013 en dash
        {"\xE2\x80\x94", "-"},    // U+2014 em dash
        {"\xE2\x80\x95", "-"},    // U+2015 horizontal bar
        {"\xE2\x80\xA6", "..."},  // U+2026 ellipsis
        {"\xE2\x80\xB2", "'"},    // U+2032 prime
        {"\xC2\xA0", " "},        // U+00A0 non-breaking space
        {"\xC2\xAB", "\""},       // U+00AB left guillemet
        {"\xC2\xBB", "\""},       // U+00BB right guillemet
    };

    std::string out = text;
    for (const Fold& fold : kFolds) {
        const std::string from = fold.utf8;
        std::size_t at = out.find(from);
        while (at != std::string::npos) {
            out.replace(at, from.size(), fold.ascii);
            at = out.find(from, at);
        }
    }
    return out;
}

}  // namespace llm_npc
