#include "TopicMatcher.hpp"

namespace llm_npc {

// Scaffolded stubs (plan: npc-line-bank, step 1). Every one returns the
// "no match" answer, so a LineBank built on this misses on every lookup and
// the game behaves exactly as it does today. Fill these in and un-skip the
// cases in tests/test_topic_matcher.cpp in the same commit.

std::string normalizeLine(const std::string&) {
    return {};  // TODO(linebank step 1)
}

float similarity(const std::string&, const std::string&) {
    return 0.f;  // TODO(linebank step 1)
}

TopicMatch bestTopic(const std::string&, const std::vector<TopicTriggers>&, float) {
    return {};  // TODO(linebank step 1)
}

}  // namespace llm_npc
