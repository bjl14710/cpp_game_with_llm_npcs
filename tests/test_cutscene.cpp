// Cutscene parsing and playback (issues #227, #228).
//
// The determinism cases are the load-bearing ones. Cutscenes get iterated on
// visually and visual-qa has to be able to say "beat 3 regressed", which only
// works if a beat renders identically every run. A cutscene driven by real
// frame time produces a different image on every machine and the captures are
// then worthless as a regression signal.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Cutscene.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// A temp directory unique to one case, so cases cannot see each other's files.
std::filesystem::path tempDir(const std::string& name) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("llm_npc_cut_" + name);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

const char* const kGoodCutscene = R"(# a fixture
name = Opening Discovery
skippable = after_first
letterbox = 90

beat = establish
  camera = -8.0, 2.2, 14.0
  yaw = 200
  pitch = -6
  hold = 2.5
  ease = smooth
  caption = Tuesday morning. Table #3, still set for two.
  fade_in = 1.0

beat = out
  hold = 1.0
  fade_out = 1.0
  fade_colour = grey
)";

CutsceneDef parsed(const std::string& text, const std::string& stem,
                   std::vector<std::string>* errors = nullptr) {
    const std::filesystem::path dir = tempDir(stem);
    const std::filesystem::path file = dir / (stem + ".cutscene");
    writeFile(file, text);
    std::optional<CutsceneDef> scene = parseCutsceneFile(file, errors);
    REQUIRE(scene.has_value());
    return *scene;
}

// A two-beat scene built in code, for playback cases that do not care about
// parsing.
CutsceneDef twoBeats(Ease ease = Ease::Linear) {
    CutsceneDef scene;
    scene.id = "two_beats";
    scene.skippable = Skippable::Always;
    CutsceneBeat a;
    a.id = "a";
    a.pose.position = Vec3{10.f, 0.f, 0.f};
    a.hasPosition = true;
    a.hold = 1.f;
    a.ease = ease;
    CutsceneBeat b;
    b.id = "b";
    b.pose.position = Vec3{20.f, 0.f, 0.f};
    b.hasPosition = true;
    b.hold = 1.f;
    b.ease = ease;
    scene.beats = {a, b};
    return scene;
}

}  // namespace

// ---- parsing ---------------------------------------------------------------

TEST_CASE("a well-formed cutscene parses into beats") {
    std::vector<std::string> errors;
    const CutsceneDef scene = parsed(kGoodCutscene, "good", &errors);

    CHECK(errors.empty());
    CHECK(scene.id == "good");  // filename stem, not a key
    CHECK(scene.name == "Opening Discovery");
    CHECK(scene.skippable == Skippable::AfterFirst);
    CHECK(scene.letterboxPx == 90);

    REQUIRE(scene.beats.size() == 2);
    CHECK(scene.beats[0].id == "establish");
    CHECK(scene.beats[0].pose.position.x == doctest::Approx(-8.0f));
    CHECK(scene.beats[0].pose.position.y == doctest::Approx(2.2f));
    CHECK(scene.beats[0].pose.position.z == doctest::Approx(14.0f));
    CHECK(scene.beats[0].pose.yawDeg == doctest::Approx(200.f));
    CHECK(scene.beats[0].pose.pitchDeg == doctest::Approx(-6.f));
    CHECK(scene.beats[0].hold == doctest::Approx(2.5f));
    CHECK(scene.beats[0].ease == Ease::Smooth);
    CHECK(scene.beats[0].fadeIn == doctest::Approx(1.0f));
    CHECK(scene.beats[1].fadeColour == "grey");
    CHECK(scene.duration() == doctest::Approx(3.5f));
}

TEST_CASE("a caption keeps a mid-line hash") {
    // '#' opens a comment only as the first non-space character. Captions are
    // prose and "Table #3" has to survive — the same rule LineBank and
    // Storyline follow, and the reason all three depart from Config's readKv.
    const CutsceneDef scene = parsed(kGoodCutscene, "hash");
    CHECK(scene.beats[0].caption == "Tuesday morning. Table #3, still set for two.");
}

TEST_CASE("a beat with no camera fields inherits, and the first inherits runtime") {
    // How "hold here and fade out" is written without repeating coordinates.
    const CutsceneDef scene = parsed(kGoodCutscene, "inherit");
    CHECK_FALSE(scene.beats[1].hasPosition);
    CHECK_FALSE(scene.beats[1].hasYaw);

    CameraPose from;
    from.position = Vec3{1.f, 2.f, 3.f};
    from.yawDeg = 45.f;

    CutscenePlayer player;
    player.play(scene, from);
    // Beat 0 sets everything, so it interpolates away from the runtime pose.
    CHECK(player.pose().position.x == doctest::Approx(1.f));
    CHECK(player.pose().yawDeg == doctest::Approx(45.f));

    // Beat 1 sets nothing, so it holds beat 0's resolved pose exactly.
    player.advance(2.5f);
    REQUIRE(player.beatIndex() == 1);
    CHECK(player.pose().position.x == doctest::Approx(-8.f));
    CHECK(player.pose().yawDeg == doctest::Approx(200.f));
}

TEST_CASE("a file with no beats is skipped with a named error") {
    const std::filesystem::path dir = tempDir("nobeats");
    const std::filesystem::path file = dir / "nobeats.cutscene";
    writeFile(file, "name = Nothing Happens\n");

    std::vector<std::string> errors;
    CHECK_FALSE(parseCutsceneFile(file, &errors).has_value());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("no beats") != std::string::npos);
    CHECK(errors[0].find("nobeats.cutscene") != std::string::npos);
}

TEST_CASE("unknown keys are reported but the cutscene still loads") {
    // Recoverable: an author fixing a typo should not lose the whole scene.
    std::vector<std::string> errors;
    const CutsceneDef scene = parsed(
        "name = Typo\nwobble = 3\n\nbeat = a\n  hold = 1\n  esae = smooth\n",
        "typo", &errors);

    REQUIRE(errors.size() == 2);
    CHECK(errors[0].find("unknown file key `wobble`") != std::string::npos);
    CHECK(errors[1].find("unknown key `esae`") != std::string::npos);
    CHECK(scene.beats.size() == 1);
}

TEST_CASE("a malformed camera line is reported, not silently placed at origin") {
    std::vector<std::string> errors;
    const CutsceneDef scene =
        parsed("beat = a\n  camera = 1.0, 2.0\n  hold = 1\n", "badcam", &errors);

    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("camera needs `x, y, z`") != std::string::npos);
    CHECK_FALSE(scene.beats[0].hasPosition);
}

TEST_CASE("a non-ASCII caption is reported") {
    // The built-in bitmap font has glyphs for ASCII 32-126 and nothing else, so
    // an em-dash reaches the screen as a literal "?". Found by looking at a
    // capture; no test would have caught it, which is why this one exists now.
    std::vector<std::string> errors;
    const CutsceneDef scene = parsed(
        "beat = a\n  hold = 1\n  caption = Tuesday morning \xE2\x80\x94 and cold.\n",
        "nonascii", &errors);

    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("non-ASCII") != std::string::npos);
    // Reported, not rewritten: silently mangling an author's text is worse
    // than telling them.
    CHECK(scene.beats[0].caption.find("\xE2\x80\x94") != std::string::npos);
}

TEST_CASE("hold is clamped up to one timestep") {
    // A beat that renders on no frame at all is never what an author meant.
    const CutsceneDef scene =
        parsed("beat = blink\n  hold = 0\n\nbeat = back\n  hold = -4\n", "clamp");
    CHECK(scene.beats[0].hold == doctest::Approx(CutscenePlayer::kFixedTimestep));
    CHECK(scene.beats[1].hold == doctest::Approx(CutscenePlayer::kFixedTimestep));
}

TEST_CASE("an id key that disagrees with the filename is reported") {
    std::vector<std::string> errors;
    const CutsceneDef scene =
        parsed("id = something_else\n\nbeat = a\n  hold = 1\n", "realname", &errors);
    CHECK(scene.id == "realname");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("filename stem `realname` wins") != std::string::npos);
}

TEST_CASE("loading a missing directory degrades to inert") {
    // Absorb the failure, say so once, never throw into the game loop — the
    // contract ConversationStore, RatingLog, LineBank and Storyline all hold.
    std::vector<std::string> errors;
    const auto all = loadCutscenes("/does/not/exist/cutscenes", &errors);
    CHECK(all.empty());
}

TEST_CASE("cutscenes load sorted by filename") {
    // directory_iterator order is unspecified; load order must be reproducible.
    const std::filesystem::path dir = tempDir("sorted");
    for (const char* stem : {"charlie", "alpha", "bravo"}) {
        writeFile(dir / (std::string(stem) + ".cutscene"), "beat = a\n  hold = 1\n");
    }
    writeFile(dir / "ignored.txt", "not a cutscene");

    const auto all = loadCutscenes(dir);
    REQUIRE(all.size() == 3);
    CHECK(all[0].id == "alpha");
    CHECK(all[1].id == "bravo");
    CHECK(all[2].id == "charlie");
    CHECK(findCutscene(all, "bravo") != nullptr);
    CHECK(findCutscene(all, "delta") == nullptr);
}

// ---- playback timing -------------------------------------------------------

TEST_CASE("a cutscene of known duration ends after exactly that long") {
    CutscenePlayer player;
    player.play(twoBeats(), CameraPose{});
    REQUIRE(player.active());

    // 2.0s total, in 0.5s steps: still running at 1.5s, done at 2.0s.
    CHECK(player.advance(0.5f));
    CHECK(player.advance(0.5f));
    CHECK(player.advance(0.5f));
    CHECK_FALSE(player.advance(0.5f));
    CHECK_FALSE(player.active());
}

TEST_CASE("beatIndex follows authored order") {
    CutscenePlayer player;
    player.play(twoBeats(), CameraPose{});
    CHECK(player.beatIndex() == 0);
    player.advance(1.0f);
    CHECK(player.beatIndex() == 1);
}

TEST_CASE("one long step crosses several short beats") {
    // Dropping the beats a long frame skipped would make playback frame-rate
    // dependent, which is what fixed-step mode exists to prevent.
    CutsceneDef scene;
    scene.id = "many";
    for (int i = 0; i < 5; ++i) {
        CutsceneBeat b;
        b.id = "b" + std::to_string(i);
        b.hold = 0.1f;
        scene.beats.push_back(b);
    }
    CutscenePlayer player;
    player.play(scene, CameraPose{});
    CHECK_FALSE(player.advance(1.0f));  // 1.0s over a 0.5s scene
    CHECK_FALSE(player.active());
}

TEST_CASE("fixed-step playback is bit-identical across runs") {
    // THE determinism case. Two independent players stepped the same number of
    // times must agree at EVERY step, not just at the end — a scene that
    // converges late would still produce different screenshots along the way.
    const CutsceneDef scene = twoBeats(Ease::Smooth);

    CutscenePlayer a;
    CutscenePlayer b;
    a.setFixedStep(true);
    b.setFixedStep(true);
    a.play(scene, CameraPose{});
    b.play(scene, CameraPose{});

    for (int step = 0; step < 200; ++step) {
        // Wildly different wall-clock deltas; fixed step must ignore both.
        a.advance(0.001f);
        b.advance(0.933f);
        const CameraPose pa = a.pose();
        const CameraPose pb = b.pose();
        REQUIRE(pa.position.x == doctest::Approx(pb.position.x));
        REQUIRE(pa.position.y == doctest::Approx(pb.position.y));
        REQUIRE(pa.position.z == doctest::Approx(pb.position.z));
        REQUIRE(pa.yawDeg == doctest::Approx(pb.yawDeg));
        REQUIRE(a.beatIndex() == b.beatIndex());
        REQUIRE(a.active() == b.active());
    }
}

TEST_CASE("ease hold snaps and smooth eases") {
    CutscenePlayer snap;
    snap.play(twoBeats(Ease::Hold), CameraPose{});
    snap.advance(0.25f);
    // Hold is a cut: the target pose from the first instant.
    CHECK(snap.pose().position.x == doctest::Approx(10.f));

    CutscenePlayer eased;
    eased.play(twoBeats(Ease::Smooth), CameraPose{});
    eased.advance(0.25f);
    // Smoothstep at t=0.25 is 0.15625, so it lags a linear quarter of the way.
    CHECK(eased.pose().position.x == doctest::Approx(1.5625f));

    CutscenePlayer linear;
    linear.play(twoBeats(Ease::Linear), CameraPose{});
    linear.advance(0.25f);
    CHECK(linear.pose().position.x == doctest::Approx(2.5f));
}

TEST_CASE("yaw interpolates the short way round") {
    // 350 -> 10 must cross zero, not spin 340 degrees backwards.
    CutsceneDef scene;
    scene.id = "wrap";
    CutsceneBeat a;
    a.id = "a";
    a.pose.yawDeg = 10.f;
    a.hasYaw = true;
    a.hold = 1.f;
    a.ease = Ease::Linear;
    scene.beats = {a};

    CameraPose from;
    from.yawDeg = 350.f;

    CutscenePlayer player;
    player.play(scene, from);
    player.advance(0.5f);
    // Halfway along a +20 degree arc from 350 is 360, not 180.
    CHECK(player.pose().yawDeg == doctest::Approx(360.f));
}

// ---- fades, skipping and overlap -------------------------------------------

TEST_CASE("fade in runs opaque to clear, fade out clear to opaque") {
    CutsceneDef scene;
    scene.id = "fades";
    CutsceneBeat b;
    b.id = "b";
    b.hold = 2.f;
    b.fadeIn = 1.f;
    b.fadeOut = 1.f;
    scene.beats = {b};

    CutscenePlayer player;
    player.play(scene, CameraPose{});
    CHECK(player.fadeAlpha() == doctest::Approx(1.f));  // opaque at the start
    player.advance(0.5f);
    CHECK(player.fadeAlpha() == doctest::Approx(0.5f));
    player.advance(0.5f);
    CHECK(player.fadeAlpha() == doctest::Approx(0.f));  // clear in the middle
    player.advance(0.75f);
    CHECK(player.fadeAlpha() == doctest::Approx(0.75f));  // fading back out
}

TEST_CASE("skipping during a fade leaves a clean state") {
    // A half-dimmed screen left behind by a skip is indistinguishable from a bug.
    CutsceneDef scene;
    scene.id = "skipfade";
    scene.skippable = Skippable::Always;
    CutsceneBeat b;
    b.id = "b";
    b.hold = 2.f;
    b.fadeIn = 2.f;
    scene.beats = {b};

    CutscenePlayer player;
    player.play(scene, CameraPose{});
    player.advance(0.5f);
    REQUIRE(player.fadeAlpha() > 0.f);

    player.skip();
    CHECK_FALSE(player.active());
    CHECK(player.fadeAlpha() == doctest::Approx(0.f));
}

TEST_CASE("after_first is mandatory once and skippable afterwards") {
    CutsceneDef scene = twoBeats();
    scene.skippable = Skippable::AfterFirst;

    CutscenePlayer player;
    player.play(scene, CameraPose{});
    CHECK(player.timesSeen(scene.id) == 1);
    CHECK_FALSE(player.canSkip());
    player.skip();
    CHECK(player.active());  // refused

    player.advance(5.f);
    REQUIRE_FALSE(player.active());

    player.play(scene, CameraPose{});
    CHECK(player.timesSeen(scene.id) == 2);
    CHECK(player.canSkip());
    player.skip();
    CHECK_FALSE(player.active());
}

TEST_CASE("skippable never refuses even on the tenth viewing") {
    CutsceneDef scene = twoBeats();
    scene.skippable = Skippable::Never;

    CutscenePlayer player;
    for (int i = 0; i < 10; ++i) {
        player.play(scene, CameraPose{});
        CHECK_FALSE(player.canSkip());
        player.skip();
        CHECK(player.active());
        player.advance(5.f);
    }
}

TEST_CASE("a cutscene requested while one plays is ignored") {
    // Overlapping camera authority is worse than a dropped beat.
    CutsceneDef first = twoBeats();
    first.id = "first";
    CutsceneDef second = twoBeats();
    second.id = "second";

    CutscenePlayer player;
    player.play(first, CameraPose{});
    player.play(second, CameraPose{});
    CHECK(player.timesSeen("second") == 0);
    CHECK(player.beatIndex() == 0);
}

TEST_CASE("a cutscene with no beats is refused rather than played empty") {
    CutsceneDef empty;
    empty.id = "empty";

    CutscenePlayer player;
    player.play(empty, CameraPose{});
    CHECK_FALSE(player.active());
}

// ---- budget ----------------------------------------------------------------

TEST_CASE("a cutscene over its phase budget is truncated at the boundary") {
    // The match clock never pauses, so the phase length is a hard budget and
    // overrunning would run a cutscene into the next phase.
    CutsceneDef scene = twoBeats();  // 2.0s
    truncateToBudget(scene, 1.5f);

    CHECK(scene.duration() == doctest::Approx(1.5f));
    REQUIRE(scene.beats.size() == 2);
    CHECK(scene.beats[0].hold == doctest::Approx(1.0f));
    CHECK(scene.beats[1].hold == doctest::Approx(0.5f));
}

TEST_CASE("truncation drops beats that no longer fit") {
    CutsceneDef scene = twoBeats();  // two 1.0s beats
    truncateToBudget(scene, 1.0f);

    CHECK(scene.beats.size() == 1);
    CHECK(scene.duration() == doctest::Approx(1.0f));
}

TEST_CASE("a cutscene inside its budget is untouched") {
    CutsceneDef scene = twoBeats();
    truncateToBudget(scene, 10.f);
    CHECK(scene.beats.size() == 2);
    CHECK(scene.duration() == doctest::Approx(2.0f));

    // A zero or negative budget means "no budget", not "drop everything".
    truncateToBudget(scene, 0.f);
    CHECK(scene.beats.size() == 2);
}

// ---- the generated opening (issue #230) ------------------------------------

#include "Mystery.hpp"
#include "Zones.hpp"

namespace {

CutsceneDef openingTemplate() {
    CutsceneDef scene;
    scene.id = "opening";
    scene.skippable = Skippable::AfterFirst;
    CutsceneBeat wide;
    wide.id = "establish";
    wide.pose.position = Vec3{-14.f, 9.f, 14.f};
    wide.hasPosition = true;
    wide.pose.yawDeg = 135.f;
    wide.hasYaw = true;
    wide.hold = 2.5f;
    wide.caption = "{zone}. Tuesday morning.";
    CutsceneBeat close;
    close.id = "the_body";
    close.pose.position = Vec3{-2.f, 1.5f, 2.f};
    close.hasPosition = true;
    close.hold = 2.f;
    close.caption = "Sometime around {hour} last night, someone died here.";
    CutsceneBeat out;
    out.id = "out";  // inherits its pose deliberately
    out.hold = 1.f;
    out.caption = "{zone} again, and {zone} still.";
    scene.beats = {wide, close, out};
    return scene;
}

std::vector<Persona> openingRoster() {
    std::vector<Persona> out;
    for (const char* name : {"Marge Holloway", "Ray Okafor", "Yuki Tanaka",
                             "Officer Dana Brooks", "Theo Vance", "Gus Pike"}) {
        Persona p;
        p.name = name;
        p.role = "resident";
        out.push_back(p);
    }
    return out;
}

}  // namespace

TEST_CASE("the opening translates authored offsets onto the body") {
    const Vec3 body{40.f, 0.f, -12.f};
    const CutsceneDef built =
        buildOpeningCutscene(openingTemplate(), "plaza_block", 21.5, body);

    REQUIRE(built.beats.size() == 3);
    CHECK(built.beats[0].pose.position.x == doctest::Approx(-14.f + 40.f));
    CHECK(built.beats[0].pose.position.z == doctest::Approx(14.f - 12.f));
    CHECK(built.beats[1].pose.position.x == doctest::Approx(-2.f + 40.f));

    // Angles are absolute: the body sits at a known offset from every authored
    // position, so an authored angle frames it identically in any zone.
    CHECK(built.beats[0].pose.yawDeg == doctest::Approx(135.f));
}

TEST_CASE("a beat that inherits its pose is not translated into one") {
    // Translating an unset (0,0,0) would silently turn "hold here" into "cut
    // to the body" — a shot the author never wrote.
    const CutsceneDef built = buildOpeningCutscene(openingTemplate(), "plaza_block",
                                                   21.5, Vec3{40.f, 0.f, -12.f});
    CHECK_FALSE(built.beats[2].hasPosition);
    CHECK(built.beats[2].pose.position.x == doctest::Approx(0.f));
}

TEST_CASE("captions carry the zone name and the hour") {
    const CutsceneDef built = buildOpeningCutscene(openingTemplate(), "plaza_block",
                                                   21.5, Vec3{});
    CHECK(built.beats[0].caption == zoneName("plaza_block") + ". Tuesday morning.");
    CHECK(built.beats[1].caption ==
          "Sometime around 21:30 last night, someone died here.");
    // Every occurrence, not just the first: a caption may name the place twice.
    const std::string& place = zoneName("plaza_block");
    CHECK(built.beats[2].caption == place + " again, and " + place + " still.");
}

TEST_CASE("two different scene zones produce two different shots") {
    // The whole reason the template authors offsets rather than positions.
    const CutsceneDef a = buildOpeningCutscene(openingTemplate(), "plaza_block",
                                               21.5, Vec3{40.f, 0.f, -12.f});
    const CutsceneDef b = buildOpeningCutscene(openingTemplate(), "bakery_block",
                                               21.5, Vec3{-64.f, 0.f, -64.f});
    CHECK(a.beats[0].pose.position.x != doctest::Approx(b.beats[0].pose.position.x));
    CHECK(a.beats[0].caption != b.beats[0].caption);
}

TEST_CASE("LEAK: nothing generated derives from the killer, across many seeds") {
    // The signature already makes this true by construction — buildOpeningCutscene
    // cannot read a field it was never handed. Asserted anyway, because the
    // thing being defended is that the signature STAYS that way: if someone
    // later passes the whole MysterySetup "for convenience", this goes red.
    const std::vector<Persona> roster = openingRoster();
    for (unsigned seed = 1; seed <= 60; ++seed) {
        CAPTURE(seed);
        const MysterySetup setup = generateMystery(roster, seed);
        const CutsceneDef built =
            buildOpeningCutscene(openingTemplate(), setup.sceneZoneId,
                                 setup.murderHour, setup.bodyPosition);

        for (const CutsceneBeat& beat : built.beats) {
            CAPTURE(beat.caption);
            CHECK(beat.caption.find(setup.killer) == std::string::npos);
            // The victim is safe to name and the template does not, but a
            // future edit that named them would be fine; the killer never is.
            for (const char* tell : {"killer", "murderer", "did it"}) {
                CHECK(beat.caption.find(tell) == std::string::npos);
            }
        }
    }
}

TEST_CASE("the opening is a pure function of the three fields it takes") {
    // Same scene, same hour, same body, different killer -> identical cutscene.
    // This is the property a reviewer actually cares about, and it survives
    // even if the function's internals change.
    const std::vector<Persona> roster = openingRoster();
    MysterySetup a = generateMystery(roster, 3u);
    MysterySetup b = a;
    b.killer = (a.killer == "Gus Pike") ? "Theo Vance" : "Gus Pike";
    REQUIRE(a.killer != b.killer);

    const CutsceneDef ca = buildOpeningCutscene(openingTemplate(), a.sceneZoneId,
                                                a.murderHour, a.bodyPosition);
    const CutsceneDef cb = buildOpeningCutscene(openingTemplate(), b.sceneZoneId,
                                                b.murderHour, b.bodyPosition);

    REQUIRE(ca.beats.size() == cb.beats.size());
    for (std::size_t i = 0; i < ca.beats.size(); ++i) {
        CHECK(ca.beats[i].caption == cb.beats[i].caption);
        CHECK(ca.beats[i].pose.position.x == doctest::Approx(cb.beats[i].pose.position.x));
        CHECK(ca.beats[i].hold == doctest::Approx(cb.beats[i].hold));
    }
}

TEST_CASE("the shipped opening.cutscene parses and fits the Intro budget") {
    namespace fs = std::filesystem;
    fs::path dir = "cutscenes";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    std::vector<std::string> errors;
    const std::vector<CutsceneDef> shipped = loadCutscenes(dir, &errors);
    CHECK(errors.empty());

    const CutsceneDef* opening = findCutscene(shipped, "opening");
    REQUIRE(opening != nullptr);
    CHECK(opening->skippable == Skippable::AfterFirst);
    CHECK(opening->duration() <= 10.0f);  // an opening nobody wants to skip

    // Every caption is ASCII: the built-in font renders nothing else.
    for (const CutsceneBeat& beat : opening->beats) {
        CAPTURE(beat.caption);
        for (const char c : beat.caption) {
            CHECK(static_cast<unsigned char>(c) <= 126u);
        }
    }
}
