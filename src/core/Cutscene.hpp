#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// Scripted, non-interactive camera sequences: the shared machinery the opening
// murder, wrong-accusation and win cutscenes all need (issues #227-#229, plan
// .claude/plans/cutscene-system.md).
//
// THE RENDERER IS UNCHANGED AND MUST STAY THAT WAY. RaylibRenderer::beginFrame
// already takes a CameraPose and rebuilds its Camera3D from it every frame, so
// a cutscene is nothing more than driving that pose from a keyframe track
// instead of from player input. If a renderer change ever looks necessary here,
// the design has gone wrong — stop and reconsider.
//
// Content lives in cutscenes/*.cutscene in the same `key = value` shape as
// traits/*.trait and storylines/*.storyline, for the reason the line bank
// gives: authored content has to review as a normal text diff in a PR.

// How a beat's pose is approached from the pose before it.
enum class Ease {
    Linear,  // constant rate
    Smooth,  // smoothstep; the default, because constant-rate camera moves read
             // as mechanical
    Hold,    // snap to the target immediately and stay there — a cut, not a move
};

// Whether a player may skip. Three-valued rather than a bool because a cutscene
// that fires every wrong day is a toll on repeat viewings, and a repeat-viewing
// toll is the fastest way to make players hate a scene they liked once.
enum class Skippable { Never, Always, AfterFirst };

// One shot: where the camera is, how long it stays, and what is written over it.
struct CutsceneBeat {
    std::string id;

    // Authored camera values, in EYE space: `camera = x, y, z` places the lens
    // at literally that height. CameraPose elsewhere is feet-space and
    // beginFrame adds kEyeHeight, so the app subtracts it when handing a
    // cutscene pose over — the same thing the sandbox editor's vantage does.
    // Authors should not have to know the player is 1.7m tall to frame a shot.
    //
    // The three `has` flags exist because a beat may
    // set any subset — "hold here and fade out" is written by setting neither
    // position nor angles, and a slow pan by setting yaw alone. Unset fields
    // inherit from the beat before, and the FIRST beat inherits from whatever
    // pose playback started at, which is why inheritance is resolved in play()
    // rather than at parse time.
    CameraPose pose{};
    bool hasPosition = false;
    bool hasYaw = false;
    bool hasPitch = false;

    // Seconds this beat lasts. Clamped up to one timestep at parse: a beat that
    // renders on no frame at all is never what an author meant.
    float hold = 1.f;
    Ease ease = Ease::Smooth;
    std::string caption;

    // Seconds of fade at the start and end of this beat. A fade is a
    // full-screen rectangle with alpha — there is no post-processing chain, so
    // dissolves and colour grading are not available and are not planned.
    float fadeIn = 0.f;
    float fadeOut = 0.f;
    std::string fadeColour;  // empty means black; the app maps the name
};

// One authored cutscene.
struct CutsceneDef {
    std::string id;    // filename stem; how --cutscene and triggers name it
    std::string name;  // player-facing
    Skippable skippable = Skippable::AfterFirst;
    int letterboxPx = 90;
    std::vector<CutsceneBeat> beats;

    // Total playback time. Used to check a scene against its phase budget.
    float duration() const;
};

// Parses one .cutscene file. Returns nullopt and appends a named message to
// `errors` when the file cannot yield a usable cutscene; recoverable problems
// append a message and still return the cutscene.
std::optional<CutsceneDef> parseCutsceneFile(const std::filesystem::path& path,
                                             std::vector<std::string>* errors);

// Every .cutscene in `dir`, sorted by filename so load order is reproducible
// across machines (directory_iterator order is unspecified).
//
// DEGRADES TO INERT. An unreadable directory yields an empty vector and one
// line on stderr — the same contract ConversationStore, RatingLog, LineBank and
// Storyline hold: absorb the failure, say so once, never throw into the game
// loop. A missing cutscenes/ directory must leave the game playable, and a
// cutscene that fails to load must never block a phase transition.
std::vector<CutsceneDef> loadCutscenes(const std::filesystem::path& dir,
                                       std::vector<std::string>* errors = nullptr);

// The cutscene with `id`, or nullptr.
//
// LIFETIME: the pointer is into `all` and dies with it. Never write
//   const CutsceneDef* c = findCutscene(loadCutscenes(dir), "opening");
// which leaves a dangling pointer into a temporary. Hold the vector.
const CutsceneDef* findCutscene(const std::vector<CutsceneDef>& all,
                                const std::string& id);

// Trims `scene` so it cannot outlast `budgetSeconds`, warning once on stderr.
//
// Cutscenes play inside the Resolution phase that already exists, which means
// the match clock never pauses: no server sync, no timeout for a client that
// never reports finished, no way for one stuck client to stall a match. The
// phase length is therefore a hard budget, and overrunning it would run a
// cutscene into the next phase. Truncating is the lesser harm, and the warning
// is how an author finds out.
void truncateToBudget(CutsceneDef& scene, float budgetSeconds);

// Drives a cutscene's camera, fade and captions over time.
//
// PURE: owns no raylib types and draws nothing. The app layer reads pose() and
// hands it to beginFrame, then draws the bars, the fade rect and the caption as
// 2D. Every timing rule here is unit-testable with no window open.
class CutscenePlayer {
public:
    // The step used when fixed-step playback is on. 60 Hz to match the target
    // frame rate, so a smoke run's frame N lands at a predictable time.
    static constexpr float kFixedTimestep = 1.f / 60.f;

    // Starts `scene`, interpolating the first beat from `from`.
    //
    // Takes a COPY rather than a reference. Generated cutscenes (the opening is
    // built per-match from MysterySetup) are temporaries at the call site, and
    // holding a pointer into one is the dangling-pointer bug this codebase has
    // already hit once in findRole.
    //
    // Ignored, with a logged line, if a cutscene is already playing. Two owners
    // of the camera is worse than a dropped beat.
    void play(const CutsceneDef& scene, const CameraPose& from);

    // Ends playback immediately, leaving no residual fade. Does nothing when
    // the current scene is not skippable yet.
    void skip();

    // In fixed-step mode every advance() moves exactly kFixedTimestep no matter
    // what is passed, so frame N always lands at the same moment in the scene.
    // That is what makes a screenshot a regression signal: a cutscene driven by
    // real frame time produces a different image on every machine, and the
    // captures are then worthless for saying "beat 3 regressed".
    void setFixedStep(bool on) { fixedStep_ = on; }
    bool fixedStep() const { return fixedStep_; }

    // Advances playback. Returns whether a cutscene is still running after the
    // step, so the caller can switch modes on a false without a second query.
    bool advance(float dtSeconds);

    bool active() const { return active_; }

    // May the player skip right now? Never, always, or every viewing after the
    // first — counted per session, per cutscene id.
    bool canSkip() const;

    // Interpolated camera for this instant. Meaningless when inactive; the
    // caller should not be asking.
    CameraPose pose() const;

    // 0 = fully visible, 1 = fully covered by the fade colour.
    float fadeAlpha() const;
    const std::string& fadeColour() const;

    int letterboxPx() const;
    const std::string& caption() const;

    // Which beat is on screen. Exposed for screenshot naming and for tests
    // asserting that authored order is what plays.
    int beatIndex() const { return beatIndex_; }

    // How many times each cutscene id has been played this session. Drives
    // AfterFirst; exposed so a test can reach the second viewing without
    // sleeping through the first.
    int timesSeen(const std::string& id) const;

private:
    CutsceneDef scene_;
    // Poses with inheritance already applied, one per beat, plus the starting
    // pose at index 0 — so beat i interpolates from resolved_[i] to
    // resolved_[i + 1] and no branch is needed for the first beat.
    std::vector<CameraPose> resolved_;
    std::map<std::string, int> seen_;

    bool active_ = false;
    bool fixedStep_ = false;
    int beatIndex_ = 0;
    float beatElapsed_ = 0.f;

    const CutsceneBeat* beat() const;
};

}  // namespace llm_npc
