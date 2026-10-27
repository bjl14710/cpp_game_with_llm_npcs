#pragma once

#include <cctype>
#include <cstddef>
#include <string>

#include "NpcAction.hpp"

namespace llm_npc {

// Removes directive tags ([[ACTION: ...]], [MOOD: ...]) from a reply WHILE it
// streams, so the dialogue box and remote players never see tag syntax in the
// frames before Npc::onReplyArrived strips the finished reply.
//
// What counts as a tag mirrors parseDirectives (NpcAction.hpp) without
// changing it: '[' or '[[', then "action" or "mood" (any case, optional
// spaces), then ':', then anything up to the first ']', plus one optional
// extra ']'. Text that could still become a tag is held back; it is dropped
// once the tag closes and released the moment it cannot be one, so ordinary
// brackets like "[sigh]" or "[[1]]" pass through as soon as their first
// letters rule a tag out. Tags may be split across deltas at any byte.
//
// Unlike parseDirectives, no whitespace seam is collapsed: "Hi [[MOOD: x]]"
// streams as "Hi ". The finished reply replaces the streamed text anyway.
class StreamTagFilter {
   public:
    // Accepts the next streamed fragment and returns the part of the reply
    // that is now safe to display (possibly empty while a tag is held).
    std::string feed(const std::string& delta) {
        std::string buf = held_ + delta;
        held_.clear();
        std::string out;
        std::size_t i = 0;
        while (i < buf.size()) {
            if (buf[i] != '[') {
                out += buf[i++];
                continue;
            }
            std::size_t end = 0;
            const Verdict v = classify(buf, i, end);
            if (v == Verdict::Undecided) {
                held_ = buf.substr(i);
                break;
            }
            if (v == Verdict::Tag) {
                i = end;
            } else {
                out += buf[i++];  // not a tag here; a later '[' may still start one
            }
        }
        return out;
    }

    // Ends the stream and returns whatever held text is displayable. A held
    // fragment that had committed to being a tag (a "[[" opener, or a
    // single-bracket form that reached its ':') is DROPPED, so an unclosed
    // "[[MOOD: ang" never reaches the screen; a bare "[mo" is released as
    // ordinary text. The filter is empty afterwards and ready for reuse.
    std::string flush() {
        std::string out;
        const bool committed = held_.compare(0, 2, "[[") == 0 ||
                               held_.find(':') != std::string::npos;
        if (!committed) out = held_;
        held_.clear();
        return out;
    }

    // Forgets any held text (a new request is starting).
    void reset() { held_.clear(); }

   private:
    enum class Verdict { Tag, NotTag, Undecided };

    // True when `kind` (raw text between the opener and the ':'/']'/end seen
    // so far) can still grow into " action " or " mood ".
    static bool couldBeKind(const std::string& kind) {
        std::size_t b = 0;
        while (b < kind.size() && std::isspace(static_cast<unsigned char>(kind[b]))) ++b;
        std::size_t e = b;
        while (e < kind.size() && !std::isspace(static_cast<unsigned char>(kind[e]))) ++e;
        for (std::size_t k = e; k < kind.size(); ++k) {
            if (!std::isspace(static_cast<unsigned char>(kind[k]))) return false;
        }
        const std::string word = toLowerAscii(kind.substr(b, e - b));
        if (e < kind.size()) return word == "action" || word == "mood";  // word ended
        return std::string("action").compare(0, word.size(), word) == 0 ||
               std::string("mood").compare(0, word.size(), word) == 0;
    }

    // Decides whether the '[' at `open` starts a directive tag, using only
    // the text received so far. On Tag, `end` is one past the tag.
    static Verdict classify(const std::string& s, std::size_t open, std::size_t& end) {
        std::size_t inner = open + 1;
        if (inner >= s.size()) return Verdict::Undecided;
        if (s[inner] == '[') ++inner;  // "[[" form
        const std::size_t stop = s.find_first_of(":]", inner);
        const std::string kind =
            s.substr(inner, (stop == std::string::npos ? s.size() : stop) - inner);
        if (!couldBeKind(kind)) return Verdict::NotTag;
        if (stop == std::string::npos) return Verdict::Undecided;
        if (s[stop] == ']') return Verdict::NotTag;  // closed with no ':'
        std::size_t b = 0;
        while (b < kind.size() && std::isspace(static_cast<unsigned char>(kind[b]))) ++b;
        std::size_t e = kind.size();
        while (e > b && std::isspace(static_cast<unsigned char>(kind[e - 1]))) --e;
        const std::string word = toLowerAscii(kind.substr(b, e - b));
        if (word != "action" && word != "mood") return Verdict::NotTag;  // e.g. "[mo:"
        const std::size_t close = s.find(']', stop + 1);
        if (close == std::string::npos) return Verdict::Undecided;
        end = close + 1;
        // Need one more byte to know whether a second ']' belongs to the tag.
        if (end >= s.size()) return Verdict::Undecided;
        if (s[end] == ']') ++end;
        return Verdict::Tag;
    }

    std::string held_;  // a '['-led fragment that may still become a tag
};

// Returns `text` with every directive tag parseDirectives recognizes removed
// (and its seams collapsed), discarding the directives themselves. Used on
// player-typed lines so a player cannot plant "[[ACTION: call_police]]" in an
// NPC's history or prompt for the model to echo back as a real directive.
inline std::string stripDirectiveTags(const std::string& text) {
    std::string out = text;
    parseDirectives(out);
    return out;
}

}  // namespace llm_npc
