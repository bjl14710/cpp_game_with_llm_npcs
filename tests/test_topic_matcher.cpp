// Topic matcher (issue #148). The threshold used throughout is 0.6, the
// config default; cases are written so the verdict is not marginal at it.
#include <string>
#include <vector>

#include "TopicMatcher.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Two topics whose trigger sets overlap on "what" but not much else — the
// interesting shape, because a matcher that only counts shared words picks
// the wrong one.
std::vector<TopicTriggers> sampleTopics() {
    return {
        {"shop_inventory",
         {"what do you sell", "got anything to eat", "whats on the menu",
          "what have you got"}},
        {"directions_bakery",
         {"where is the bakery", "how do i get to the bakery",
          "which way to the bakery"}},
        {"greeting", {"hello", "hi there", "good morning", "hey"}},
    };
}

}  // namespace

TEST_CASE("normalizeLine lowercases, strips punctuation and drops stopwords") {
    // Case, trailing punctuation and filler words must all wash out, so that
    // "What do you SELL?!" and "what do you sell" score identically.
    CHECK(normalizeLine("What do you SELL?!") == normalizeLine("what do you sell"));
    CHECK(normalizeLine("  Hello,   there!  ") == normalizeLine("hello there"));
    CHECK(normalizeLine("").empty());
    CHECK(normalizeLine("   ").empty());
}

TEST_CASE("normalizeLine passes non-ASCII through instead of corrupting it") {
    // A non-English line should simply fail to match later and fall through
    // to the live model. Mangling the bytes here would be a silent data bug.
    const std::string french = normalizeLine("Bonjour, ça va?");
    CHECK(french.find("\xc3\xa7") != std::string::npos);  // 'ç' survived
}

TEST_CASE("similarity scores an exact normalized match at 1") {
    const std::string a = normalizeLine("what do you sell");
    CHECK(similarity(a, a) == doctest::Approx(1.f));
}

TEST_CASE("similarity is insensitive to word order but not blind to it") {
    // Token-set Jaccard makes reordering score high; the trigram half keeps
    // it from scoring a perfect 1, which is what stops "bakery the where is"
    // outranking a real exact match.
    const float reordered =
        similarity(normalizeLine("hello there"), normalizeLine("there hello"));
    CHECK(reordered > 0.5f);
    CHECK(reordered < 1.f);
}

TEST_CASE("bestTopic routes a paraphrase to the topic that authored it") {
    // The whole design bet: paraphrase coverage comes from the authored
    // trigger list, not from a smarter scorer.
    const auto match = bestTopic("got anything to eat?", sampleTopics(), 0.6f);
    CHECK(match.topicId == "shop_inventory");
}

TEST_CASE("bestTopic returns nothing when the best score is below threshold") {
    // A miss must fall through to the live model, byte-identical to today.
    const auto match =
        bestTopic("what did you think of the election", sampleTopics(), 0.6f);
    CHECK(match.topicId.empty());
}

TEST_CASE("bestTopic returns nothing for empty or whitespace input") {
    CHECK(bestTopic("", sampleTopics(), 0.6f).topicId.empty());
    CHECK(bestTopic("   ", sampleTopics(), 0.6f).topicId.empty());
}

TEST_CASE("bestTopic breaks exact ties deterministically on topicId") {
    // Two topics carrying the identical trigger. Whichever wins, it must be
    // the SAME one every run — a cache that picks differently across two
    // identical runs cannot be tested or reviewed.
    const std::vector<TopicTriggers> tied = {
        {"zebra_topic", {"hello"}},
        {"alpha_topic", {"hello"}},
    };
    const auto first = bestTopic("hello", tied, 0.6f);
    const auto second = bestTopic("hello", tied, 0.6f);
    CHECK(first.topicId == second.topicId);
    CHECK(first.topicId == "alpha_topic");  // ascending topicId wins
}

TEST_CASE("bestTopic on an empty topic list misses rather than crashing") {
    CHECK(bestTopic("hello", {}, 0.6f).topicId.empty());
}
