#include "MatchClock.hpp"

#include <algorithm>

namespace llm_npc {
namespace {

// A phase shorter than this cannot be observed and makes phaseProgress()
// divide by zero. Rules are clamped rather than rejected: a badly configured
// match should still run.
constexpr float kMinPhaseSeconds = 1.f;

MatchRules clamped(MatchRules rules) {
    rules.dayLimit = std::max(1, rules.dayLimit);
    rules.introSeconds = std::max(kMinPhaseSeconds, rules.introSeconds);
    rules.investigationSeconds = std::max(kMinPhaseSeconds, rules.investigationSeconds);
    rules.voteSeconds = std::max(kMinPhaseSeconds, rules.voteSeconds);
    rules.resolutionSeconds = std::max(kMinPhaseSeconds, rules.resolutionSeconds);
    return rules;
}

}  // namespace

MatchClock::MatchClock(MatchRules rules) : rules_(clamped(rules)) {}

MatchPhase MatchClock::nextPhase(MatchPhase phase) {
    switch (phase) {
        case MatchPhase::Intro:         return MatchPhase::Investigation;
        case MatchPhase::Investigation: return MatchPhase::Vote;
        case MatchPhase::Vote:          return MatchPhase::Resolution;
        case MatchPhase::Resolution:    return MatchPhase::Investigation;
        case MatchPhase::Ended:         return MatchPhase::Ended;
    }
    return MatchPhase::Ended;
}

float MatchClock::phaseDuration(MatchPhase phase) const {
    switch (phase) {
        case MatchPhase::Intro:         return rules_.introSeconds;
        case MatchPhase::Investigation: return rules_.investigationSeconds;
        case MatchPhase::Vote:          return rules_.voteSeconds;
        case MatchPhase::Resolution:    return rules_.resolutionSeconds;
        case MatchPhase::Ended:         return 0.f;
    }
    return 0.f;
}

PhaseTransition MatchClock::advance(float realDtSeconds) {
    PhaseTransition out;
    out.day = day_;
    out.from = phase_;
    out.to = phase_;

    // Ended absorbs everything. Negative dt is ignored rather than rewinding:
    // a clock that can run backwards makes every event ordering ambiguous.
    if (phase_ == MatchPhase::Ended || realDtSeconds <= 0.f) return out;

    elapsedInPhase_ += realDtSeconds;
    const float duration = phaseDuration(phase_);
    if (elapsedInPhase_ < duration) return out;

    // Exactly ONE transition per call. The overshoot carries into the next
    // phase instead of being discarded, so a long frame delays the following
    // boundary rather than losing it — and every phase still gets its event,
    // which is what the vote and the cutscenes hang off.
    elapsedInPhase_ -= duration;

    const MatchPhase from = phase_;
    MatchPhase to = nextPhase(from);
    bool dayAdvanced = false;

    if (from == MatchPhase::Resolution) {
        // The day limit is checked at the rollover, so the final day plays its
        // Resolution in full before the match ends.
        if (day_ >= rules_.dayLimit) {
            to = MatchPhase::Ended;
        } else {
            ++day_;
            dayAdvanced = true;
        }
    }

    phase_ = to;
    if (phase_ == MatchPhase::Ended) elapsedInPhase_ = 0.f;

    out.fired = true;
    out.from = from;
    out.to = to;
    out.day = day_;
    out.dayAdvanced = dayAdvanced;
    out.matchEnded = (to == MatchPhase::Ended);
    return out;
}

float MatchClock::phaseRemainingSeconds() const {
    if (phase_ == MatchPhase::Ended) return 0.f;
    return std::max(0.f, phaseDuration(phase_) - elapsedInPhase_);
}

float MatchClock::phaseProgress() const {
    if (phase_ == MatchPhase::Ended) return 0.f;
    // phaseDuration is clamped to >= 1s at construction, so this cannot
    // divide by zero however the rules were configured.
    return std::min(1.f, elapsedInPhase_ / phaseDuration(phase_));
}

double MatchClock::worldHour() const {
    const double start = static_cast<double>(rules_.dayStartHour);
    const double end = static_cast<double>(rules_.dayEndHour);

    // Intro holds at the START of the day: the match has not begun, so the
    // sky must not have spent its light yet.
    if (phase_ == MatchPhase::Intro) return start;

    // Vote, Resolution and Ended all hold at the end of the day. Letting the
    // hour run during the vote would slide the town toward night and make the
    // plaza gather unreadable; freezing the light is also a signal in itself.
    if (phase_ != MatchPhase::Investigation) return end;

    return start + (end - start) * static_cast<double>(phaseProgress());
}

void MatchClock::endMatch() {
    phase_ = MatchPhase::Ended;
    elapsedInPhase_ = 0.f;
}

const char* phaseName(MatchPhase phase) {
    switch (phase) {
        case MatchPhase::Intro: return "Intro";
        case MatchPhase::Investigation: return "Investigation";
        case MatchPhase::Vote: return "Vote";
        case MatchPhase::Resolution: return "Resolution";
        case MatchPhase::Ended: return "Ended";
    }
    // Unreachable for a valid enumerator; present because a function with a
    // declared return type must return on every path, not as a fallback for a
    // phase somebody forgot to add above. The missing case is a warning.
    return "Ended";
}

}  // namespace llm_npc
