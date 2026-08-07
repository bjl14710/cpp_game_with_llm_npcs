#pragma once

#include <string>
#include <vector>

namespace llm_npc {

// Scores a player's line against a topic's trigger phrasings (plan:
// npc-line-bank, step 1).
//
// Deliberately LEXICAL, not semantic. The paraphrase problem ("got anything
// to eat?" should reach the same topic as "what do you sell?") is solved at
// AUTHORING time instead — tools/refine_lines.py emits 8-12 `trigger =`
// phrasings per topic — which buys most of what an embedding would and lets
// the matcher stay a pure function with no network, no second resident model
// competing for the 16 GB, and no boot cost. That is what makes it
// unit-testable offline and safe inside an unattended 3 a.m. job.
//
// Swapping in an Ollama-embedding scorer later means replacing the body of
// similarity() and nothing else. The trigger for doing that is a MEASURED
// hit rate below 40% on a week of real transcripts, not a hunch.

// One topic's matchable surface: its id and every phrasing that reaches it.
struct TopicTriggers {
    std::string topicId;
    std::vector<std::string> triggers;
};

// The winning topic and how confident the match was, 0..1.
struct TopicMatch {
    std::string topicId;  // empty when nothing cleared the threshold
    float score = 0.f;
};

// TODO(linebank step 1): lowercase, strip punctuation, collapse whitespace
// runs, and drop stopwords ("the", "a", "is", "do", "you", ...). Bytes
// outside ASCII pass through untouched, so a non-English line simply fails
// to match rather than corrupting — falling through to the live model is the
// correct behaviour there, not an error.
std::string normalizeLine(const std::string& raw);

// TODO(linebank step 1): blended similarity of two ALREADY-NORMALIZED
// strings, 0..1. Token-set Jaccard (catches word reordering) averaged with
// character-trigram Jaccard (catches typos and morphology). Neither alone is
// enough: token-set alone misses "sells"/"sell", trigrams alone rank
// "hello there"/"there hello" as identical to an exact match.
float similarity(const std::string& normalizedA, const std::string& normalizedB);

// TODO(linebank step 1): score `playerLine` against every trigger of every
// topic and return the best. Ties break on topicId, ascending — a cache that
// picks differently across two identical runs cannot be tested or reviewed.
// Returns an empty topicId when the best score is below `threshold`.
TopicMatch bestTopic(const std::string& playerLine,
                     const std::vector<TopicTriggers>& topics, float threshold);

}  // namespace llm_npc
