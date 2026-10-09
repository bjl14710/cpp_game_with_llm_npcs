// The arrest cutscene (plan: .claude/plans/cutscene-authored-scenes.md):
// the officer's mood-selected line, and the authored scene file itself.
//
// SCAFFOLD -- every case is skipped until ArrestLine.hpp is implemented.
// These stubs ARE the contract; each maps to an acceptance criterion in the
// plan. Remove the doctest::skip() decorator as each is filled in, and do not
// weaken a CHECK to make one pass.
//
// THE SCENE FILE IS PARKED. cutscenes/arrest.cutscene.pending is authored and
// complete but not yet named *.cutscene, because its slug/speaker/journal keys
// arrive with PR #283 and dev's parser would report them as unknown -- which
// turns two existing directory-level tests red. The parse cases below name
// "arrest.cutscene", so they start passing exactly when the file is renamed.
//
// DELIBERATELY LIMITED TO TODAY'S API. The scene file carries
// journal_subject, journal_line, slug and speaker keys, but CutsceneDef and
// CutsceneBeat do not have those fields on dev yet -- they arrive with PR
// #283. doctest::skip() skips at RUNTIME, so a test body referencing
// scene.journalSubject would fail to COMPILE and take the whole suite down.
// The assertions waiting on #283 are listed at the bottom of this file rather
// than written against an API that does not exist.
#include <filesystem>
#include <string>
#include <vector>

#include "ArrestLine.hpp"
#include "Cutscene.hpp"
#include "NpcAction.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// The test binary runs from tests/ (make) or build/ (ctest); walk up to
// wherever cutscenes/ lives. Same approach as test_persona_roster.cpp.
std::filesystem::path cutscenesDir() {
    namespace fs = std::filesystem;
    fs::path dir = "cutscenes";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    return dir;
}

bool isRenderableAscii(const std::string& text) {
    for (const unsigned char c : text) {
        if (c < 32 || c > 126) return false;
    }
    return true;
}

}  // namespace

TEST_CASE("every mood yields a line, so a caller never handles an empty one"
          * doctest::skip()) {
    for (const NpcMood mood : {NpcMood::Neutral, NpcMood::Happy, NpcMood::Angry,
                               NpcMood::Sad, NpcMood::Embarrassed,
                               NpcMood::Surprised}) {
        CHECK_FALSE(arrestLineFor(mood).empty());
    }
}

TEST_CASE("an angry officer says something different from a neutral one"
          * doctest::skip()) {
    // The whole point of the variant: being arrested by someone you just
    // annoyed should not read identically to a routine pickup.
    CHECK(arrestLineFor(NpcMood::Angry) != arrestLineFor(NpcMood::Neutral));
    CHECK(arrestLineFor(NpcMood::Happy) != arrestLineFor(NpcMood::Neutral));
    CHECK(arrestLineFor(NpcMood::Happy) != arrestLineFor(NpcMood::Angry));
}

TEST_CASE("the four unhandled moods share the neutral line, on purpose"
          * doctest::skip()) {
    // This is a DECISION, not a gap. The design asked for three variants
    // against a six-value enum; Sad, Embarrassed and Surprised are not moods
    // an officer performs at an arrest. Pinned so nobody "fixes" it later by
    // inventing three more lines.
    const std::string neutral = arrestLineFor(NpcMood::Neutral);
    CHECK(arrestLineFor(NpcMood::Sad) == neutral);
    CHECK(arrestLineFor(NpcMood::Embarrassed) == neutral);
    CHECK(arrestLineFor(NpcMood::Surprised) == neutral);
}

TEST_CASE("exactly three distinct lines exist" * doctest::skip()) {
    std::vector<std::string> seen;
    for (const NpcMood mood : {NpcMood::Neutral, NpcMood::Happy, NpcMood::Angry,
                               NpcMood::Sad, NpcMood::Embarrassed,
                               NpcMood::Surprised}) {
        const std::string line = arrestLineFor(mood);
        bool known = false;
        for (const std::string& s : seen) known = known || (s == line);
        if (!known) seen.push_back(line);
    }
    CHECK(seen.size() == 3);
}

TEST_CASE("every line is renderable ASCII" * doctest::skip()) {
    // The built-in bitmap font covers 32-126 and substitutes '?' for anything
    // else -- a curly apostrophe in "we'll" would reach the screen as "we?ll".
    // src/core/AsciiText.hpp documents the exact raylib mechanism.
    for (const NpcMood mood : {NpcMood::Neutral, NpcMood::Happy, NpcMood::Angry,
                               NpcMood::Sad, NpcMood::Embarrassed,
                               NpcMood::Surprised}) {
        CHECK(isRenderableAscii(arrestLineFor(mood)));
    }
}

TEST_CASE("the shipped arrest scene parses and runs its authored six seconds"
          * doctest::skip()) {
    std::vector<std::string> errors;
    const auto scene = parseCutsceneFile(cutscenesDir() / "arrest.cutscene", &errors);
    REQUIRE(scene.has_value());
    CHECK(errors.empty());

    CHECK(scene->id == "arrest");
    CHECK(scene->name == "Taken In");
    // Escape must always work: the scene locks input for six seconds while
    // the world keeps simulating, and in a networked match a player may need
    // those seconds back.
    CHECK(scene->skippable == Skippable::Always);
    CHECK(scene->letterboxPx == 96);
    // The design's budget for this beat, to the frame.
    CHECK(scene->duration() == doctest::Approx(6.0f));
    REQUIRE(scene->beats.size() == 3);
    CHECK(scene->beats[0].id == "approach");
    CHECK(scene->beats[1].id == "line");
    CHECK(scene->beats[2].id == "out");
}

TEST_CASE("the arrest camera frames the police station, not the origin"
          * doctest::skip()) {
    // The station AABB is (-20,-80)..(20,-40) (City.cpp:28) and the arrest
    // teleport lands the player at ((minX+maxX)/2, 0, maxZ+2) = (0, 0, -38).
    // Unlike opening.cutscene these are ABSOLUTE coordinates, because an
    // arrest always ends in the same place -- so a regression here puts the
    // camera in the middle of the plaza looking at nothing.
    std::vector<std::string> errors;
    const auto scene = parseCutsceneFile(cutscenesDir() / "arrest.cutscene", &errors);
    REQUIRE(scene.has_value());
    REQUIRE(scene->beats.size() >= 2);

    for (int i = 0; i < 2; ++i) {
        // Both moving beats author a position and a yaw; the third inherits.
        REQUIRE(scene->beats[i].hasPosition);
        REQUIRE(scene->beats[i].hasYaw);
        // Inside the station block's x span and just north of its front face.
        CHECK(scene->beats[i].pose.position.z < -30.f);
        CHECK(scene->beats[i].pose.position.z > -40.f);
        CHECK(scene->beats[i].pose.position.x > -20.f);
        CHECK(scene->beats[i].pose.position.x < 20.f);
        // yaw 0 faces +Z, so looking back at the station is 180.
        CHECK(scene->beats[i].pose.yawDeg == doctest::Approx(180.f));
    }
    // It PUSHES: the second beat is closer to the station than the first.
    CHECK(scene->beats[1].pose.position.z < scene->beats[0].pose.position.z);
    // The out beat inherits its pose rather than authoring one -- that is what
    // "hold here and fade" means in this format.
    CHECK_FALSE(scene->beats[2].hasPosition);
}

// ---------------------------------------------------------------------------
// WAITING ON PR #283 -- add these once CutsceneDef/CutsceneBeat carry the
// fields. They cannot be written today: doctest::skip() is a runtime skip, so
// referencing a field that does not exist breaks the build for the whole
// suite, not just this file.
//
//   scene->journalSubject == "arrested_by_brooks"
//   scene->journalLine contains "{clock}"
//   beats[0].slug       == "POLICE STATION STEPS - {clock}"
//   beats[1].speakerLine == "{officer}: {line}"
//   every slug/speaker/journal string is renderable ASCII
//   the three substitution tokens {officer} {line} {clock} each appear at
//     least once across the scene, so a rename in main.cpp cannot silently
//     leave a literal "{officer}" on screen
// ---------------------------------------------------------------------------
