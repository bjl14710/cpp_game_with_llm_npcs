#pragma once

#include <cctype>
#include <string>

namespace llm_npc {

// A physical action an NPC can take in the world, decided by the LLM in
// response to a player instruction ("follow me", "wave", ...). The model emits
// action tags in its replies; parseDirectives() turns them into one of these.
// Three kinds are mixed here: persistent movement behaviors (Follow, Arrest,
// Stop, Face, ReturnHome), transient gesture poses (RaiseHand, Wave), and the
// world-level CallPolice signal. The Npc routes each kind to the right state.
enum class NpcAction {
    None,
    Follow,      // trail the player at a walk until told otherwise
    Stop,        // hold position, face the player
    Face,        // turn to look at the player, don't move
    RaiseHand,   // raise the right hand for a few seconds
    Wave,        // wave the right hand for a few seconds
    Arrest,      // police only: chase the player down, then stop on catch
    CallPolice,  // civilians: summon the police to come arrest the player
    ReturnHome,  // not LLM-emittable: walk back to the spawn spot
};

// The NPC's emotional read on the conversation, also emitted by the LLM as a
// tag on every reply. Drives the rendered facial expression.
enum class NpcMood {
    Neutral,
    Happy,
    Angry,
    Sad,
    Embarrassed,
    Surprised,
};

// Everything extracted from one reply's directive tags.
struct Directives {
    NpcAction action = NpcAction::None;
    NpcMood mood = NpcMood::Neutral;
    bool hasMood = false;  // distinguishes "said neutral" from "said nothing"
};

// Maps a normalized (lowercase, '_'-joined) tag keyword to its action.
// Unknown keywords yield None. ReturnHome is internal and not mapped.
inline NpcAction actionFromKeyword(const std::string& kw) {
    if (kw == "follow") return NpcAction::Follow;
    if (kw == "stop" || kw == "stay") return NpcAction::Stop;
    if (kw == "face" || kw == "look") return NpcAction::Face;
    if (kw == "raise_hand" || kw == "raise") return NpcAction::RaiseHand;
    if (kw == "wave") return NpcAction::Wave;
    if (kw == "arrest") return NpcAction::Arrest;
    if (kw == "call_police" || kw == "police") return NpcAction::CallPolice;
    return NpcAction::None;
}

// Maps a normalized mood keyword to a mood, with synonyms for the words small
// models actually emit. Unknown keywords yield Neutral with `ok = false`.
inline bool moodFromKeyword(const std::string& kw, NpcMood& out) {
    if (kw == "neutral" || kw == "calm") out = NpcMood::Neutral;
    else if (kw == "happy" || kw == "flattered" || kw == "pleased" ||
             kw == "excited" || kw == "joyful" || kw == "amused") out = NpcMood::Happy;
    else if (kw == "angry" || kw == "annoyed" || kw == "irritated" ||
             kw == "frustrated" || kw == "mad") out = NpcMood::Angry;
    else if (kw == "sad" || kw == "upset" || kw == "hurt" ||
             kw == "disappointed") out = NpcMood::Sad;
    else if (kw == "embarrassed" || kw == "flustered" || kw == "awkward" ||
             kw == "shy" || kw == "nervous") out = NpcMood::Embarrassed;
    else if (kw == "surprised" || kw == "shocked" || kw == "startled") out = NpcMood::Surprised;
    else return false;
    return true;
}

// Lowercase copy of s (ASCII), used for case-insensitive matching.
inline std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Extracts every directive tag from an LLM reply, tolerating the mess a small
// model actually produces: tags at the start, middle, or end; doubled or
// single brackets; duplicate mood tags (the last one wins); unknown keywords.
// All recognized-looking tags ("action"/"mood" kinds) are erased from `reply`
// in place - including unknown keywords, so brackets never reach the
// transcript - and seam whitespace is collapsed. Other bracketed text (e.g.
// "[sigh]" or "[[1]]") is left alone.
inline Directives parseDirectives(std::string& reply) {
    Directives out;

    std::size_t i = 0;
    while (i < reply.size()) {
        if (reply[i] != '[') {
            ++i;
            continue;
        }
        const std::size_t open = i;
        std::size_t inner = open + 1;
        if (inner < reply.size() && reply[inner] == '[') ++inner;  // "[[" form
        const std::size_t close = reply.find(']', inner);
        if (close == std::string::npos) break;  // no closing bracket anywhere
        std::size_t end = close + 1;
        // Consume a second ']' if present so "]]" closes fully.
        if (end < reply.size() && reply[end] == ']') ++end;

        // "kind : keyword" between the brackets.
        const std::string body = toLowerAscii(reply.substr(inner, close - inner));
        const std::size_t colon = body.find(':');
        if (colon == std::string::npos) {
            ++i;
            continue;
        }
        const auto trimRange = [&](std::size_t b, std::size_t e) {
            while (b < e && std::isspace(static_cast<unsigned char>(body[b]))) ++b;
            while (e > b && std::isspace(static_cast<unsigned char>(body[e - 1]))) --e;
            return body.substr(b, e - b);
        };
        const std::string kind = trimRange(0, colon);
        std::string keyword = trimRange(colon + 1, body.size());
        // Normalize inner spaces to underscores ("call police" -> "call_police").
        for (char& c : keyword) {
            if (std::isspace(static_cast<unsigned char>(c))) c = '_';
        }

        if (kind != "action" && kind != "mood") {
            ++i;
            continue;
        }

        if (kind == "action") {
            const NpcAction action = actionFromKeyword(keyword);
            if (action != NpcAction::None) out.action = action;
        } else {
            NpcMood mood = NpcMood::Neutral;
            if (moodFromKeyword(keyword, mood)) {
                out.mood = mood;  // last recognized mood wins
                out.hasMood = true;
            }
        }

        // Strip the tag and collapse the whitespace seam it leaves behind.
        reply.erase(open, end - open);
        while (open < reply.size() && open > 0 &&
               std::isspace(static_cast<unsigned char>(reply[open])) &&
               std::isspace(static_cast<unsigned char>(reply[open - 1]))) {
            reply.erase(open, 1);
        }
        i = open;
    }

    // Tags at the very edges leave dangling whitespace; trim both ends.
    while (!reply.empty() && std::isspace(static_cast<unsigned char>(reply.back()))) reply.pop_back();
    std::size_t lead = 0;
    while (lead < reply.size() && std::isspace(static_cast<unsigned char>(reply[lead]))) ++lead;
    reply.erase(0, lead);

    return out;
}

// Compatibility wrapper: extract just the action (still strips all tags).
inline NpcAction parseActionTag(std::string& reply) {
    return parseDirectives(reply).action;
}

}  // namespace llm_npc
