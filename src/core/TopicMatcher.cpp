#include "TopicMatcher.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace llm_npc {
namespace {

// economy: a fixed, small stopword set rather than a data file. A bank
// trigger and a player line MUST normalize identically, and a file is one
// more way for the game and tools/refine_lines.py to drift apart. Words that
// carry topic meaning in this game stay in deliberately — "where", "there",
// "how", "who" all distinguish real topics from each other.
const std::unordered_set<std::string>& stopwords() {
    static const std::unordered_set<std::string> kWords = {
        "a",    "an",  "and", "are",  "as",   "at",   "be",   "been", "but",
        "by",   "can", "could", "did", "do",  "does", "for",  "from", "had",
        "has",  "have", "i",  "if",   "in",   "is",   "it",   "its",  "me",
        "my",   "of",  "on",  "or",   "that", "the",  "this", "to",   "was",
        "were", "what", "will", "with", "would", "you", "your"};
    return kWords;
}

std::vector<std::string> tokens(const std::string& normalized) {
    std::istringstream in(normalized);
    std::vector<std::string> out;
    for (std::string word; in >> word;) out.push_back(word);
    return out;
}

// Character trigrams over the whole normalized string, spaces included — the
// space-bearing grams are what stop a pure word-set match from scoring
// reordered text as identical. Strings shorter than three bytes become one
// gram so "hi" and "got" still compare against something.
std::unordered_set<std::string> trigrams(const std::string& s) {
    std::unordered_set<std::string> out;
    if (s.size() < 3) {
        if (!s.empty()) out.insert(s);
        return out;
    }
    for (std::size_t i = 0; i + 3 <= s.size(); ++i) out.insert(s.substr(i, 3));
    return out;
}

template <typename Set>
float jaccard(const Set& a, const Set& b) {
    if (a.empty() || b.empty()) return 0.f;
    std::size_t shared = 0;
    for (const auto& item : a) {
        if (b.count(item)) ++shared;
    }
    const std::size_t total = a.size() + b.size() - shared;
    return total == 0 ? 0.f : static_cast<float>(shared) / static_cast<float>(total);
}

// Float comparisons on blended Jaccard scores land on exact ties often (two
// topics carrying the same trigger both score 1.0), so ties need a real
// epsilon rather than operator==.
constexpr float kEps = 1e-6f;

}  // namespace

std::string normalizeLine(const std::string& raw) {
    // Fold to lowercase ASCII, turn everything that is not alphanumeric into a
    // separator, and pass bytes >= 0x80 through untouched. std::tolower is only
    // ever handed an ASCII byte: feeding it a negative char is undefined, and
    // mangling UTF-8 here would corrupt a non-English line instead of letting
    // it fall through to the live model, which is the correct behaviour.
    std::string folded;
    folded.reserve(raw.size());
    for (const unsigned char c : raw) {
        if (c >= 0x80) {
            folded.push_back(static_cast<char>(c));
        } else if (std::isalnum(c)) {
            folded.push_back(static_cast<char>(std::tolower(c)));
        } else {
            folded.push_back(' ');
        }
    }

    std::string out;
    out.reserve(folded.size());
    for (const std::string& word : tokens(folded)) {
        if (stopwords().count(word)) continue;
        if (!out.empty()) out.push_back(' ');
        out += word;
    }
    return out;
}

float similarity(const std::string& normalizedA, const std::string& normalizedB) {
    if (normalizedA.empty() || normalizedB.empty()) return 0.f;

    const std::vector<std::string> ta = tokens(normalizedA);
    const std::vector<std::string> tb = tokens(normalizedB);
    const std::unordered_set<std::string> sa(ta.begin(), ta.end());
    const std::unordered_set<std::string> sb(tb.begin(), tb.end());

    // Token-set catches reordering and dropped filler; trigrams catch typos
    // and morphology ("sell"/"sells"). Averaging keeps a reordered phrase
    // scoring high without letting it tie a genuine exact match.
    return 0.5f * jaccard(sa, sb) + 0.5f * jaccard(trigrams(normalizedA), trigrams(normalizedB));
}

TopicMatch bestTopic(const std::string& playerLine,
                     const std::vector<TopicTriggers>& topics, float threshold) {
    const std::string line = normalizeLine(playerLine);
    if (line.empty()) return {};

    TopicMatch best;
    bool haveAny = false;
    for (const TopicTriggers& topic : topics) {
        float score = 0.f;
        for (const std::string& trigger : topic.triggers) {
            // economy: triggers are re-normalized per lookup rather than
            // cached. Ten personas of forty topics is a few thousand short
            // strings — far below the 2.7 s round trip this exists to avoid.
            // Cache only if a profile says to.
            score = std::max(score, similarity(line, normalizeLine(trigger)));
        }
        const bool better = score > best.score + kEps;
        const bool tiedButEarlier =
            std::fabs(score - best.score) <= kEps && topic.topicId < best.topicId;
        if (!haveAny || better || tiedButEarlier) {
            best = {topic.topicId, score};
            haveAny = true;
        }
    }

    if (!haveAny || best.score < threshold) return {};
    return best;
}

}  // namespace llm_npc
