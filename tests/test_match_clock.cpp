// Match day counter and phase machine (issue #154).
#include <vector>

#include "DayNight.hpp"
#include "MatchClock.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Short phases so a whole match is a handful of advance() calls:
// 10 + 4 + 2 = 16s per day, 48s for a three-day match.
MatchRules fastRules() {
    MatchRules rules;
    rules.dayLimit = 3;
    rules.investigationSeconds = 10.f;
    rules.voteSeconds = 4.f;
    rules.resolutionSeconds = 2.f;
    return rules;
}

std::vector<PhaseTransition> run(MatchClock& clock, float dt, int steps) {
    std::vector<PhaseTransition> fired;
    for (int i = 0; i < steps; ++i) {
        const PhaseTransition t = clock.advance(dt);
        if (t.fired) fired.push_back(t);
    }
    return fired;
}

}  // namespace

TEST_CASE("the vote opens inside the dusk band, so the light is the countdown") {
    // Pins the coupling that makes the countdown diegetic. dayEndHour must sit
    // strictly inside DayNight's dusk blend — if a later retune moves either
    // value, sunset stops coinciding with the vote and the feature quietly
    // loses its only piece of UI-free signalling.
    const MatchRules rules;
    CHECK(rules.dayEndHour > daynight::kDuskStart);
    CHECK(rules.dayEndHour < daynight::kDuskEnd);

    const float atVote = lightLevelAt(rules.dayEndHour);
    CHECK(atVote < 1.f);    // no longer full daylight
    CHECK(atVote > 0.35f);  // not yet full night
}

TEST_CASE("a full match runs its day limit and then ends") {
    MatchClock clock(fastRules());
    const auto fired = run(clock, 1.f, 100);

    REQUIRE(!fired.empty());
    CHECK(fired.back().matchEnded);
    CHECK(clock.phase() == MatchPhase::Ended);
    CHECK(clock.day() == 3);
    // Three days x (Investigation -> Vote -> Resolution) = 9 transitions.
    CHECK(fired.size() == 9);
}

TEST_CASE("phases cycle in order and the day counter advances") {
    MatchClock clock(fastRules());
    CHECK(clock.phase() == MatchPhase::Investigation);
    CHECK(clock.day() == 1);

    const auto fired = run(clock, 1.f, 17);  // through day 1 into day 2
    REQUIRE(fired.size() >= 3);
    CHECK(fired[0].to == MatchPhase::Vote);
    CHECK(fired[1].to == MatchPhase::Resolution);
    CHECK(fired[2].to == MatchPhase::Investigation);
    CHECK(fired[2].dayAdvanced);
    CHECK(fired[2].day == 2);
    CHECK_FALSE(fired[0].dayAdvanced);
}

TEST_CASE("worldHour sweeps from dayStartHour to dayEndHour") {
    MatchClock clock(fastRules());
    CHECK(clock.worldHour() == doctest::Approx(9.0));

    clock.advance(5.f);  // halfway through a 10s investigation
    CHECK(clock.phaseProgress() == doctest::Approx(0.5f));
    CHECK(clock.worldHour() == doctest::Approx(14.5));  // 9 + 11 * 0.5
}

TEST_CASE("worldHour holds at dayEndHour during Vote and Resolution") {
    // A slow vote must not slide the town into full night — the plaza gather
    // has to stay readable, and freezing the light is a signal in itself.
    MatchClock clock(fastRules());
    clock.advance(10.f);
    REQUIRE(clock.phase() == MatchPhase::Vote);
    CHECK(clock.worldHour() == doctest::Approx(20.0));

    clock.advance(3.f);
    REQUIRE(clock.phase() == MatchPhase::Vote);
    CHECK(clock.worldHour() == doctest::Approx(20.0));

    clock.advance(2.f);
    REQUIRE(clock.phase() == MatchPhase::Resolution);
    CHECK(clock.worldHour() == doctest::Approx(20.0));
}

TEST_CASE("a frame longer than a whole phase reports one transition, not none") {
    // Debugger, alt-tab, a stalled load. The vote hangs off these events, so a
    // phase skipped without an event is a match that never resolves.
    MatchClock clock(fastRules());
    const PhaseTransition t = clock.advance(600.f);
    CHECK(t.fired);
    CHECK(t.from == MatchPhase::Investigation);
    CHECK(t.to == MatchPhase::Vote);
}

TEST_CASE("leftover time carries into the next phase rather than being dropped") {
    MatchClock clock(fastRules());
    clock.advance(12.f);  // 10s investigation + 2s into the 4s vote
    CHECK(clock.phase() == MatchPhase::Vote);
    CHECK(clock.phaseRemainingSeconds() == doctest::Approx(2.f));
}

TEST_CASE("a very long frame still resolves the whole match one step at a time") {
    // Each call reports one transition, so nine calls walk a three-day match
    // to its end no matter how big the deltas are. Nothing is skipped.
    MatchClock clock(fastRules());
    int fired = 0;
    for (int i = 0; i < 9; ++i) {
        if (clock.advance(1000.f).fired) ++fired;
    }
    CHECK(fired == 9);
    CHECK(clock.phase() == MatchPhase::Ended);
}

TEST_CASE("a zero or negative dt moves nothing") {
    MatchClock clock(fastRules());
    CHECK_FALSE(clock.advance(0.f).fired);
    CHECK_FALSE(clock.advance(-5.f).fired);
    CHECK(clock.phase() == MatchPhase::Investigation);
    CHECK(clock.phaseProgress() == doctest::Approx(0.f));
    CHECK(clock.worldHour() == doctest::Approx(9.0));
}

TEST_CASE("a dayLimit below one is clamped rather than producing no match") {
    MatchRules rules = fastRules();
    rules.dayLimit = 0;
    MatchClock clock(rules);
    CHECK(clock.rules().dayLimit == 1);

    const auto fired = run(clock, 1.f, 40);
    CHECK(clock.phase() == MatchPhase::Ended);
    CHECK(fired.back().matchEnded);
}

TEST_CASE("a zero-length phase is clamped so progress never divides by zero") {
    MatchRules rules = fastRules();
    rules.investigationSeconds = 0.f;
    MatchClock clock(rules);
    CHECK(clock.rules().investigationSeconds >= 1.f);
    CHECK(clock.phaseProgress() == doctest::Approx(0.f));  // finite, not NaN
}

TEST_CASE("endMatch is idempotent") {
    MatchClock clock(fastRules());
    clock.endMatch();
    CHECK(clock.phase() == MatchPhase::Ended);
    CHECK_FALSE(clock.advance(1.f).fired);

    clock.endMatch();
    CHECK(clock.phase() == MatchPhase::Ended);
    CHECK(clock.phaseRemainingSeconds() == doctest::Approx(0.f));
    CHECK(clock.phaseProgress() == doctest::Approx(0.f));
}

TEST_CASE("an ended match stops advancing") {
    MatchClock clock(fastRules());
    clock.endMatch();
    const int dayAtEnd = clock.day();
    run(clock, 1.f, 50);
    CHECK(clock.day() == dayAtEnd);
    CHECK(clock.phase() == MatchPhase::Ended);
}

TEST_CASE("a one-day match ends after a single cycle") {
    MatchRules rules = fastRules();
    rules.dayLimit = 1;
    MatchClock clock(rules);

    const auto fired = run(clock, 1.f, 40);
    CHECK(fired.size() == 3);  // Investigation -> Vote -> Resolution -> Ended
    CHECK(fired.back().matchEnded);
    CHECK(clock.day() == 1);
}
