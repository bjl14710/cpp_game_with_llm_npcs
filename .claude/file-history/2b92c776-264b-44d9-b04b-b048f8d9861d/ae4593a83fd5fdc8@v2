#pragma once

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

#include "WorldState.hpp"

namespace llm_npc {

// One journal row: a fact the player was personally told, plus whether it
// clashes with another entry on the same subject.
struct JournalEntry {
    const KnownFact* fact = nullptr;
    bool conflicting = false;
};

// The journal is a pure READER of the gossip fact store: facts the player
// knows whose source is not the player (what they were TOLD — never what
// they told others, never gossip they weren't part of), grouped by
// subject and time-ordered within each group. Entries whose subject
// appears with more than one distinct content are flagged conflicting on
// BOTH sides ("two different stories about the same thing").
inline std::vector<JournalEntry> journalEntries(const WorldState& state) {
    std::vector<JournalEntry> entries;
    for (const KnownFact* fact : state.factsKnownBy("player")) {
        if (fact->source == "player") continue;
        entries.push_back({fact, false});
    }
    std::stable_sort(entries.begin(), entries.end(),
                     [](const JournalEntry& a, const JournalEntry& b) {
                         if (a.fact->subject != b.fact->subject) {
                             return a.fact->subject < b.fact->subject;
                         }
                         return a.fact->learnedAtSeconds < b.fact->learnedAtSeconds;
                     });
    // Conflict pass: same subject, more than one distinct content.
    for (std::size_t i = 0; i < entries.size(); ++i) {
        for (std::size_t j = i + 1; j < entries.size(); ++j) {
            if (entries[j].fact->subject != entries[i].fact->subject) break;
            if (entries[j].fact->content != entries[i].fact->content) {
                entries[i].conflicting = true;
                entries[j].conflicting = true;
            }
        }
    }
    return entries;
}

// "HH:MM" for a world-time stamp (journal attribution lines).
inline std::string clockLabel(double worldSeconds) {
    const int minutes = static_cast<int>(worldSeconds / 60.0) % (24 * 60);
    const int h = minutes / 60;
    const int m = minutes % 60;
    std::string out = (h < 10 ? "0" : "") + std::to_string(h) + ":";
    out += (m < 10 ? "0" : "") + std::to_string(m);
    return out;
}

}  // namespace llm_npc
