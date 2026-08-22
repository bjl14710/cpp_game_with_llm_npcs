#pragma once

namespace llm_npc {

// The detective match's day counter and phase machine (plan:
// detective-day-phases, issue #154).
//
// WorldState::advanceTime is a pure fmod loop over 24 hours — the game has no
// concept of a day at all, and the wrap is silent. This adds one.
//
// The relationship with the clock is INVERTED from free roam. Instead of the
// world clock ticking at a fixed 60x while everything reacts, the match owns
// pacing and DRIVES the clock: worldHour() sweeps dayStartHour -> dayEndHour
// across the investigation phase, so dusk always lands exactly when the vote
// opens (DayNight's kDuskStart is 18.0, kDuskEnd 21.0). The existing dusk band
// becomes a free, diegetic countdown that needs no UI.
//
// Pure by design, like DayNight.hpp: this computes, the app layer applies. It
// never touches WorldState, so it is testable with no world at all.

enum class MatchPhase {
    // Day one opens here and no later day returns to it. An explicit value
    // rather than reusing Resolution's slot on day zero: a phase that means
    // something different should not borrow another's name, and the vote and
    // cutscene hooks in later issues switch on this enum.
    Intro,          // the opening beat elapses; the clock holds at dawn
    Investigation,  // players roam and interrogate; the clock sweeps
    Vote,           // ballot open; the hour holds
    Resolution,     // outcome applied, cutscenes play; the hour holds
    Ended,          // match over; nothing advances
};

// All durations in REAL seconds; the in-world hour is derived from them.
struct MatchRules {
    // How many days before the killer wins by default. Clamped to >= 1.
    int dayLimit = 3;
    // The opening beat, before the first Investigation. Sized to the cold
    // open's 12.0s budget (cutscenes/body_in_plaza.cutscene).
    //
    // The cutscene does NOT gate this: with no cutscene playing the phase
    // simply elapses. That keeps the phase machine independent of
    // presentation, which is why MatchClock stayed mergeable while the
    // cutscene system was still unbuilt.
    float introSeconds = 12.f;
    // The long phase: roam, interrogate, examine.
    float investigationSeconds = 8.f * 60.f;
    // Ballot open. Short enough to force a decision, long enough to argue.
    float voteSeconds = 60.f;
    // Outcome applied and cutscenes played. This doubles as the cutscene
    // budget — see .claude/plans/cutscene-system.md.
    float resolutionSeconds = 15.f;
    // Investigation begins at dayStartHour and ends at dayEndHour. dayEndHour
    // sits INSIDE DayNight's dusk band on purpose: that coupling is what makes
    // the countdown diegetic, and a test pins it.
    float dayStartHour = 9.f;
    float dayEndHour = 20.f;
};

// Emitted by advance() on the tick a phase boundary is crossed.
struct PhaseTransition {
    // False on the vast majority of frames — check this before reading
    // anything else in the struct.
    bool fired = false;
    MatchPhase from = MatchPhase::Investigation;
    MatchPhase to = MatchPhase::Investigation;
    // The day the match is on AFTER the transition, 1-based.
    int day = 1;
    // True only on Resolution -> Investigation, i.e. a new day began.
    bool dayAdvanced = false;
    // True on the transition into Ended, whether by day limit or endMatch().
    bool matchEnded = false;
};

class MatchClock {
   public:
    // Rules are CLAMPED on construction, never rejected: dayLimit below 1
    // becomes 1, and any phase duration below a one-second floor is raised so
    // phaseProgress() can never divide by zero. A badly configured match
    // should still run.
    explicit MatchClock(MatchRules rules);

    // Advances by real dt and returns the transition that fired, if any.
    //
    // At most ONE transition is reported per call, and leftover time carries
    // into the next phase rather than being dropped. A frame longer than a
    // whole phase (debugger, alt-tab, a stalled asset load) must not silently
    // skip a phase's event, because the vote hangs off exactly those events —
    // a skipped Vote transition is a match that never resolves.
    //
    // Zero and negative dt move nothing and fire nothing.
    PhaseTransition advance(float realDtSeconds);

    MatchPhase phase() const { return phase_; }

    // 1-based. Does not advance past dayLimit.
    int day() const { return day_; }

    // Seconds left in the current phase; 0 once Ended.
    float phaseRemainingSeconds() const;

    // 0..1 through the current phase; 0 once Ended.
    float phaseProgress() const;

    // The in-world hour the caller writes into WorldState. HOLDS at
    // dayStartHour through Intro -- the day has not begun, and opening on a
    // dusk sky would spend the diegetic countdown before the player has moved.
    // Sweeps dayStartHour -> dayEndHour across Investigation; HOLDS at
    // dayEndHour during Vote and Resolution, so a slow vote cannot slide the
    // town into full night and make the plaza gather unreadable.
    double worldHour() const;

    // Early exit — someone won, or the host quit. Idempotent: a second call
    // fires no transition and changes nothing.
    void endMatch();

    // The clamped rules actually in force, which may differ from those passed
    // to the constructor.
    const MatchRules& rules() const { return rules_; }

   private:
    // Duration of `phase` in real seconds; the Ended phase has none.
    float phaseDuration(MatchPhase phase) const;
    // The phase that follows `phase` in the cycle.
    static MatchPhase nextPhase(MatchPhase phase);

    MatchRules rules_;
    MatchPhase phase_ = MatchPhase::Intro;
    int day_ = 1;
    // Real seconds elapsed inside the current phase.
    float elapsedInPhase_ = 0.f;
};

}  // namespace llm_npc
