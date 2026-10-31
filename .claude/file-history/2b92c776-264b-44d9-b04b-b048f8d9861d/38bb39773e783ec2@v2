#pragma once

#include <cctype>
#include <string>

namespace llm_npc {

// A physical action an NPC can take in the world, decided by the LLM in
// response to a player instruction ("follow me", "wave", ...). The model emits
// at most one action tag per reply; parseActionTag() turns it into one of
// these. Two axes are mixed here: persistent movement behaviors (Follow,
// Arrest, Stop, Face) and transient gesture poses (RaiseHand, Wave). The Npc
// routes each kind to the right state.
enum class NpcAction {
    None,
    Follow,     // trail the player at a walk until told otherwise
    Stop,       // hold position, face the player
    Face,       // turn to look at the player, don't move
    RaiseHand,  // raise the right hand for a few seconds
    Wave,       // wave the right hand for a few seconds
    Arrest,     // chase the player down, then stop on catch
};

// Maps a lowercase tag keyword to its action. Unknown keywords yield None.
inline NpcAction actionFromKeyword(const std::string& kw) {
    if (kw == "follow") return NpcAction::Follow;
    if (kw == "stop" || kw == "stay") return NpcAction::Stop;
    if (kw == "face" || kw == "look") return NpcAction::Face;
    if (kw == "raise_hand" || kw == "raise") return NpcAction::RaiseHand;
    if (kw == "wave") return NpcAction::Wave;
    if (kw == "arrest") return NpcAction::Arrest;
    return NpcAction::None;
}

// Lowercase copy of s (ASCII), used for case-insensitive matching.
inline std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Extracts an action directive of the form "[[ACTION: <keyword>]]" from an LLM
// reply. The match is case-insensitive and tolerant of inner whitespace. When
// a valid tag is found, it is erased from `reply` in place and any whitespace
// left trailing is trimmed (the model is told to put the tag last, on its own
// line) so it never reaches the transcript; the corresponding NpcAction is
// returned. Returns NpcAction::None when no valid tag is present, leaving
// `reply` untouched.
inline NpcAction parseActionTag(std::string& reply) {
    const std::string lower = toLowerAscii(reply);

    const std::size_t open = lower.find("[[");
    if (open == std::string::npos) return NpcAction::None;
    const std::size_t close = lower.find("]]", open);
    if (close == std::string::npos) return NpcAction::None;

    // Content between the brackets, e.g. "action: follow".
    const std::string inner = lower.substr(open + 2, close - (open + 2));
    if (inner.find("action") == std::string::npos) return NpcAction::None;
    const std::size_t colon = inner.find(':');
    if (colon == std::string::npos) return NpcAction::None;

    // Trim whitespace around the keyword that follows the colon.
    std::size_t kwStart = colon + 1;
    while (kwStart < inner.size() && std::isspace(static_cast<unsigned char>(inner[kwStart]))) ++kwStart;
    std::size_t kwEnd = inner.size();
    while (kwEnd > kwStart && std::isspace(static_cast<unsigned char>(inner[kwEnd - 1]))) --kwEnd;
    const std::string keyword = inner.substr(kwStart, kwEnd - kwStart);

    const NpcAction action = actionFromKeyword(keyword);
    if (action == NpcAction::None) return NpcAction::None;

    // Remove the tag and any whitespace it leaves dangling at the end.
    reply.erase(open, (close + 2) - open);
    while (!reply.empty() && std::isspace(static_cast<unsigned char>(reply.back()))) reply.pop_back();
    return action;
}

}  // namespace llm_npc
