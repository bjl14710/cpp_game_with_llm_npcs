// Parsing authored mystery templates (issue #186).
//
// Files are written into a temp directory rather than checked in as fixtures,
// so a case reads as "this text produces this result" without a reader having
// to open a second file.
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "Storyline.hpp"
#include "Zones.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A directory unique to each case, so cases cannot see each other's files.
std::filesystem::path tempDir(const std::string& tag) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("llm_npc_storyline_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// A minimal well-formed template, used where the case is about something else.
const char* const kGoodStoryline = R"(# a fixture
id = the_late_delivery
title = The Late Delivery
min_residents = 4

role = neighbour
  kind = witness
  note = lives above the bakery

role = rival
  kind = red_herring

clue = 1
  zone = bakery_block
  caption = The bakery's back door was unlocked all evening.
  points_at_killer = true

clue = 2
  zone = coffee_block
  caption = Two cups on the counter, one untouched.
  slot = rival
  points_at_killer = false

witness = neighbour
  zone = bakery_block
  hour = 21.5
  about = rival
  observed = someone leaving by the alley door
)";

}  // namespace

TEST_CASE("a well-formed template parses into every field") {
    const auto dir = tempDir("good");
    writeFile(dir / "the_late_delivery.storyline", kGoodStoryline);

    std::vector<std::string> errors;
    const auto story = parseStorylineFile(dir / "the_late_delivery.storyline", &errors);

    REQUIRE(story.has_value());
    CHECK(errors.empty());
    CHECK(story->id == "the_late_delivery");
    CHECK(story->title == "The Late Delivery");
    CHECK(story->minResidents == 4);

    REQUIRE(story->roles.size() == 2);
    CHECK(story->roles[0].slotId == "neighbour");
    CHECK(story->roles[0].kind == "witness");
    CHECK(story->roles[0].note == "lives above the bakery");
    CHECK(story->roles[1].slotId == "rival");
    CHECK(story->roles[1].note.empty());

    REQUIRE(story->clues.size() == 2);
    CHECK(story->clues[0].order == 1);
    CHECK(story->clues[0].zoneId == "bakery_block");
    CHECK(story->clues[0].pointsAtKiller);
    CHECK(story->clues[0].slotId.empty());
    CHECK(story->clues[1].order == 2);
    CHECK(story->clues[1].slotId == "rival");
    CHECK_FALSE(story->clues[1].pointsAtKiller);

    REQUIRE(story->witnesses.size() == 1);
    CHECK(story->witnesses[0].slotId == "neighbour");
    CHECK(story->witnesses[0].zoneId == "bakery_block");
    CHECK(story->witnesses[0].atHour == doctest::Approx(21.5));
    CHECK(story->witnesses[0].observed == "someone leaving by the alley door");
    CHECK(story->witnesses[0].aboutSlotId == "rival");
}

TEST_CASE("clues keep the order they were authored in, not a sorted one") {
    // win-cutscene.md's requirement: the chain is an argument, so a template
    // that authors 3, 1, 2 gets back 3, 1, 2 and the validator complains —
    // rather than the parser quietly fixing it and hiding the mistake.
    const auto dir = tempDir("order");
    writeFile(dir / "s.storyline", R"(
id = s
clue = 3
  zone = plaza
  caption = third
clue = 1
  zone = plaza
  caption = first
clue = 2
  zone = plaza
  caption = second
)");

    const auto story = parseStorylineFile(dir / "s.storyline", nullptr);
    REQUIRE(story.has_value());
    REQUIRE(story->clues.size() == 3);
    CHECK(story->clues[0].order == 3);
    CHECK(story->clues[1].order == 1);
    CHECK(story->clues[2].order == 2);
    CHECK(story->clues[0].caption == "third");
}

TEST_CASE("a hash inside a caption is text, not a comment") {
    // The one place this parser deliberately differs from Config.cpp's readKv,
    // matching LineBank.cpp. Captions are prose.
    const auto dir = tempDir("hash");
    writeFile(dir / "s.storyline", R"(
id = s
clue = 1
  zone = plaza
  caption = Two cups on the counter, table #3, one untouched.
)");

    const auto story = parseStorylineFile(dir / "s.storyline", nullptr);
    REQUIRE(story.has_value());
    REQUIRE(story->clues.size() == 1);
    CHECK(story->clues[0].caption ==
          "Two cups on the counter, table #3, one untouched.");
}

TEST_CASE("a leading hash is still a comment") {
    const auto dir = tempDir("comment");
    writeFile(dir / "s.storyline", R"(
# id = wrong_id
id = right_id
clue = 1
  zone = plaza
  caption = a clue
   # indented comments count too
)");

    const auto story = parseStorylineFile(dir / "s.storyline", nullptr);
    REQUIRE(story.has_value());
    CHECK(story->id == "right_id");
    CHECK(story->clues.size() == 1);
}

TEST_CASE("a file with no id is rejected with one reason") {
    const auto dir = tempDir("noid");
    writeFile(dir / "s.storyline", R"(
title = Nameless
clue = 1
  zone = plaza
  caption = a clue
)");

    std::vector<std::string> errors;
    CHECK_FALSE(parseStorylineFile(dir / "s.storyline", &errors).has_value());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("no `id` key") != std::string::npos);
}

TEST_CASE("a file with no clues is rejected") {
    const auto dir = tempDir("noclues");
    writeFile(dir / "s.storyline", "id = empty\ntitle = Nothing Happens\n");

    std::vector<std::string> errors;
    CHECK_FALSE(parseStorylineFile(dir / "s.storyline", &errors).has_value());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("no clues") != std::string::npos);
}

TEST_CASE("a missing file reports rather than throwing") {
    std::vector<std::string> errors;
    CHECK_FALSE(parseStorylineFile("/does/not/exist.storyline", &errors).has_value());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("cannot be read") != std::string::npos);
}

TEST_CASE("an unknown key is named and the rest of the file still parses") {
    const auto dir = tempDir("unknown");
    writeFile(dir / "s.storyline", R"(
id = s
clue = 1
  zone = plaza
  caption = a clue
  colour = blue
)");

    std::vector<std::string> errors;
    const auto story = parseStorylineFile(dir / "s.storyline", &errors);

    REQUIRE(story.has_value());
    CHECK(story->clues.size() == 1);
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("unknown clue key `colour`") != std::string::npos);
}

TEST_CASE("a section key before any section is named, not silently dropped") {
    const auto dir = tempDir("orphan");
    writeFile(dir / "s.storyline", R"(
id = s
zone = plaza
clue = 1
  zone = bakery_block
  caption = a clue
)");

    std::vector<std::string> errors;
    const auto story = parseStorylineFile(dir / "s.storyline", &errors);

    REQUIRE(story.has_value());
    CHECK(story->clues[0].zoneId == "bakery_block");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("before any") != std::string::npos);
}

TEST_CASE("a non-numeric order parses as 0 rather than throwing") {
    // std::atoi over std::stoi on purpose: a parser that throws on a typo
    // breaks the degrade-to-inert contract. 0 is out of range, so the
    // validator reports it as a real problem.
    const auto dir = tempDir("badorder");
    writeFile(dir / "s.storyline", R"(
id = s
clue = first
  zone = plaza
  caption = a clue
)");

    const auto story = parseStorylineFile(dir / "s.storyline", nullptr);
    REQUIRE(story.has_value());
    REQUIRE(story->clues.size() == 1);
    CHECK(story->clues[0].order == 0);
}

// ---- directory loading ---------------------------------------------------

TEST_CASE("loadStorylines reads every template in filename order") {
    const auto dir = tempDir("dir");
    writeFile(dir / "b_second.storyline",
              "id = b\nclue = 1\n  zone = plaza\n  caption = b\n");
    writeFile(dir / "a_first.storyline",
              "id = a\nclue = 1\n  zone = plaza\n  caption = a\n");
    writeFile(dir / "notes.txt", "id = ignored\n");

    const std::vector<StorylineDef> stories = loadStorylines(dir);

    REQUIRE(stories.size() == 2);
    // Sorted, because directory_iterator order is unspecified and a
    // machine-dependent load order makes error output irreproducible.
    CHECK(stories[0].id == "a");
    CHECK(stories[1].id == "b");
}

TEST_CASE("one broken template does not take the others with it") {
    const auto dir = tempDir("mixed");
    writeFile(dir / "good.storyline",
              "id = good\nclue = 1\n  zone = plaza\n  caption = c\n");
    writeFile(dir / "broken.storyline", "title = no id here\n");

    std::vector<std::string> errors;
    const std::vector<StorylineDef> stories = loadStorylines(dir, &errors);

    REQUIRE(stories.size() == 1);
    CHECK(stories[0].id == "good");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("broken.storyline") != std::string::npos);
}

TEST_CASE("a missing directory degrades to inert") {
    // The contract that matters: no storylines must leave the game playable,
    // not crashed.
    std::vector<std::string> errors;
    const std::vector<StorylineDef> stories =
        loadStorylines("/does/not/exist/storylines", &errors);
    CHECK(stories.empty());
}

// ---- validation (issue #187) ---------------------------------------------

namespace {

// A template that passes every rule, so each case below can break exactly one
// thing and assert on that one error.
StorylineDef validStoryline() {
    StorylineDef story;
    story.id = "the_late_delivery";
    story.title = "The Late Delivery";
    story.minResidents = 4;
    story.roles = {
        StorylineRole{"culprit", "killer", ""},
        StorylineRole{"neighbour", "witness", ""},
        StorylineRole{"rival", "red_herring", ""},
    };
    story.clues = {
        StorylineClue{1, "bakery_block", "The back door was unlocked.", "", true},
        StorylineClue{2, "coffee_block", "Two cups, one untouched.", "rival", false},
    };
    // Same `about`, different accounts — the subject collision Journal.hpp
    // flags. Not "same zone and hour": that was the pre-#214 rule and it stood
    // in for a collision that could not happen.
    story.witnesses = {
        StorylineWitness{"neighbour", "bakery_block", "someone left by the alley",
                         21.5, "culprit"},
        StorylineWitness{"rival", "bakery_block", "nobody came or went", 21.6,
                         "culprit"},
    };
    return story;
}

// The reasons for one `where`, so a case can assert on text without caring
// about the order errors came out in.
bool hasError(const std::vector<StorylineError>& errors, const std::string& where,
              const std::string& reasonFragment) {
    for (const StorylineError& error : errors) {
        if (error.where != where) continue;
        if (error.reason.find(reasonFragment) != std::string::npos) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("a valid template produces no errors") {
    CHECK(validateStoryline(validStoryline(), 21).empty());
}

TEST_CASE("a gap in the chain is reported") {
    // The chain is an argument; a gap means a step of the reasoning was cut.
    StorylineDef story = validStoryline();
    story.clues[1].order = 5;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[1]", "beyond the 2 clues authored"));
}

TEST_CASE("two clues claiming the same position are reported") {
    StorylineDef story = validStoryline();
    story.clues[1].order = 1;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[1]", "duplicate order 1"));
}

TEST_CASE("a non-numeric order that parsed as 0 is reported") {
    StorylineDef story = validStoryline();
    story.clues[0].order = 0;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[0]", "order must be 1 or greater"));
}

TEST_CASE("an unknown zone is reported for clues and witnesses") {
    StorylineDef story = validStoryline();
    story.clues[0].zoneId = "bakery";  // the id is bakery_block
    story.witnesses[0].zoneId = "";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[0]", "unknown zone `bakery`"));
    CHECK(hasError(errors, "witnesses[0]", "unknown zone"));
}

TEST_CASE("the streets are a valid zone") {
    // kStreetsZoneId is a real zone, deliberately not in zonesForDowntown()
    // because it is the complement of the nine blocks. A clue found in the
    // street is legitimate.
    StorylineDef story = validStoryline();
    story.clues[0].zoneId = kStreetsZoneId;

    CHECK(validateStoryline(story, 21).empty());
}

TEST_CASE("a caption over the KnownFact limit is reported") {
    StorylineDef story = validStoryline();
    story.clues[0].caption = std::string(141, 'x');

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[0]", "over the 140-char KnownFact limit"));
}

TEST_CASE("witnesses that all agree are reported") {
    // Without a contradiction there is nothing for the journal to flag and
    // nothing to investigate.
    StorylineDef story = validStoryline();
    story.witnesses[1].observed = story.witnesses[0].observed;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "witnesses", "no two witnesses contradict"));
}

TEST_CASE("witnesses talking about different people are not a contradiction") {
    // Two people describing two different people is ordinary, not a
    // disagreement — and it produces two subjects, which the journal never
    // compares. The rule is a shared `about`.
    StorylineDef story = validStoryline();
    story.witnesses[1].aboutSlotId = "neighbour";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "witnesses", "no two witnesses contradict"));
}

TEST_CASE("witnesses in different places CAN contradict") {
    // The pre-#214 rule rejected this, and it was the rule that was wrong.
    // Disagreeing about WHERE someone was is the most common shape a real
    // alibi conflict takes, so two different zones must stay valid.
    StorylineDef story = validStoryline();
    story.witnesses[1].zoneId = "coffee_block";

    CHECK(validateStoryline(story, 21).empty());
}

TEST_CASE("a witness whose `about` names no declared slot is reported") {
    // Silently downgrading to speaker keying would make the author's intended
    // contradiction quietly stop existing, which is exactly the failure #214
    // was.
    StorylineDef story = validStoryline();
    story.witnesses[0].aboutSlotId = "nobody_by_that_name";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "witnesses[0]", "`about` cites undeclared slot"));
}

TEST_CASE("`about = victim` resolves without being a declared slot") {
    // The victim is picked by generateMystery and never cast, but "where was
    // the dead man that evening" is the most obvious question in a mystery.
    StorylineDef story = validStoryline();
    story.witnesses[0].aboutSlotId = "victim";
    story.witnesses[1].aboutSlotId = "victim";

    CHECK(validateStoryline(story, 21).empty());
}

TEST_CASE("a chain of pure red herrings is reported") {
    StorylineDef story = validStoryline();
    story.clues[0].pointsAtKiller = false;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues", "no clue has points_at_killer = true"));
}

TEST_CASE("a template with no killer role is reported") {
    StorylineDef story = validStoryline();
    story.roles[0].kind = "bystander";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "roles", "no role declares kind = killer"));
}

TEST_CASE("an unknown role kind is reported") {
    StorylineDef story = validStoryline();
    story.roles[1].kind = "accomplice";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "roles[1]", "unknown kind `accomplice`"));
}

TEST_CASE("a duplicate slot id is reported") {
    StorylineDef story = validStoryline();
    story.roles[2].slotId = "neighbour";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "roles[2]", "duplicate slot id `neighbour`"));
}

TEST_CASE("a clue citing an undeclared slot is reported") {
    StorylineDef story = validStoryline();
    story.clues[1].slotId = "the_butler";

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "clues[1]", "cites undeclared slot `the_butler`"));
}

TEST_CASE("a template needing more residents than the roster is reported") {
    StorylineDef story = validStoryline();
    story.minResidents = 30;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "min_residents", "needs 30 residents but the roster has 21"));
}

TEST_CASE("an hour outside the day is reported") {
    StorylineDef story = validStoryline();
    story.witnesses[0].atHour = 24.0;

    const auto errors = validateStoryline(story, 21);
    CHECK(hasError(errors, "witnesses[0]", "hour must be in [0, 24)"));
}

TEST_CASE("validation reports every problem, not just the first") {
    // The behaviour validateMap has, for the reason it has it: an author
    // fixing one error at a time through six round trips gives up.
    StorylineDef story = validStoryline();
    story.clues[0].zoneId = "nowhere";
    story.clues[1].order = 9;
    story.roles[1].kind = "accomplice";
    story.witnesses[0].atHour = 99.0;

    const auto errors = validateStoryline(story, 21);
    CHECK(errors.size() >= 4);
    CHECK(hasError(errors, "clues[0]", "unknown zone"));
    CHECK(hasError(errors, "clues[1]", "beyond the"));
    CHECK(hasError(errors, "roles[1]", "unknown kind"));
    CHECK(hasError(errors, "witnesses[0]", "hour must be"));
}

TEST_CASE("a roster size of zero skips the roster-dependent rules") {
    // Callers validating a template with no roster in hand — an authoring
    // tool, a unit test — should not be told the roster is too small.
    StorylineDef story = validStoryline();
    story.minResidents = 30;

    const auto errors = validateStoryline(story, 0);
    CHECK_FALSE(hasError(errors, "min_residents", "but the roster has"));
}

// ---- casting onto a roster (issue #188) ----------------------------------

namespace {

std::vector<Persona> castRoster() {
    std::vector<Persona> roster;
    for (const char* name : {"Marge Holloway", "Ray Okafor", "Yuki Tanaka",
                             "Officer Dana Brooks", "Theo Vance", "Gus Pike"}) {
        Persona p;
        p.name = name;
        p.role = "resident";
        roster.push_back(p);
    }
    return roster;
}

// A setup with the victim and killer already decided, as generateMystery
// leaves it.
MysterySetup decidedSetup() {
    MysterySetup setup;
    setup.victim = "Theo Vance";
    setup.killer = "Marge Holloway";
    setup.sceneZoneId = "bakery_block";
    setup.murderHour = 21.5;
    return setup;
}

}  // namespace

TEST_CASE("casting fills the evidence and witnesses generateMystery left empty") {
    MysterySetup setup = decidedSetup();
    REQUIRE(setup.evidence.empty());
    REQUIRE(setup.witnesses.empty());

    const StorylineCast cast =
        castStoryline(validStoryline(), castRoster(), setup, 42u);

    CHECK(setup.evidence.size() == 2);
    CHECK(setup.witnesses.size() == 2);
    CHECK(cast.assignments.size() == 3);
}

TEST_CASE("the same template, roster and seed cast identically") {
    MysterySetup a = decidedSetup();
    MysterySetup b = decidedSetup();

    const StorylineCast castA = castStoryline(validStoryline(), castRoster(), a, 7u);
    const StorylineCast castB = castStoryline(validStoryline(), castRoster(), b, 7u);

    REQUIRE(castA.assignments.size() == castB.assignments.size());
    for (std::size_t i = 0; i < castA.assignments.size(); ++i) {
        CHECK(castA.assignments[i].first == castB.assignments[i].first);
        CHECK(castA.assignments[i].second == castB.assignments[i].second);
    }
}

TEST_CASE("a different seed casts different people") {
    // Determinism without variation is a constant, which would satisfy the
    // case above and be useless.
    std::set<std::string> neighbours;
    for (unsigned seed = 0; seed < 40; ++seed) {
        MysterySetup setup = decidedSetup();
        const StorylineCast cast =
            castStoryline(validStoryline(), castRoster(), setup, seed);
        neighbours.insert(cast.residentFor("neighbour"));
    }
    CHECK(neighbours.size() > 1);
}

TEST_CASE("the killer slot binds the killer generateMystery already chose") {
    // role-layer.md scopes choosing the killer to #178. Reassigning it here
    // would silently break voteIsCorrect.
    for (unsigned seed = 0; seed < 40; ++seed) {
        MysterySetup setup = decidedSetup();
        const StorylineCast cast =
            castStoryline(validStoryline(), castRoster(), setup, seed);

        CHECK(cast.residentFor("culprit") == "Marge Holloway");
        CHECK(setup.killer == "Marge Holloway");
        CHECK(voteIsCorrect(setup, "Marge Holloway"));
    }
}

TEST_CASE("the victim is never cast in a living role") {
    // The dead cannot testify, and a clue citing them as a living witness
    // would never resolve.
    for (unsigned seed = 0; seed < 40; ++seed) {
        MysterySetup setup = decidedSetup();
        const StorylineCast cast =
            castStoryline(validStoryline(), castRoster(), setup, seed);

        for (const auto& [slot, resident] : cast.assignments) {
            CHECK(resident != "Theo Vance");
        }
    }
}

TEST_CASE("no resident holds two slots") {
    for (unsigned seed = 0; seed < 40; ++seed) {
        MysterySetup setup = decidedSetup();
        const StorylineCast cast =
            castStoryline(validStoryline(), castRoster(), setup, seed);

        std::set<std::string> seen;
        for (const auto& [slot, resident] : cast.assignments) {
            CHECK(seen.insert(resident).second);
        }
    }
}

TEST_CASE("an undersized roster casts nothing rather than partially") {
    // A partial cast would leave clues citing residents who do not exist.
    StorylineDef story = validStoryline();
    story.minResidents = 10;

    MysterySetup setup = decidedSetup();
    const StorylineCast cast = castStoryline(story, castRoster(), setup, 3u);

    CHECK(cast.assignments.empty());
    CHECK(setup.evidence.empty());
    CHECK(setup.witnesses.empty());
}

TEST_CASE("evidence keeps the authored chain order") {
    // The parser never re-sorts and casting walks the vector as parsed, so
    // authored order survives all the way into MysterySetup.
    StorylineDef story = validStoryline();
    story.clues[0].caption = "first beat";
    story.clues[1].caption = "second beat";

    MysterySetup setup = decidedSetup();
    castStoryline(story, castRoster(), setup, 11u);

    REQUIRE(setup.evidence.size() == 2);
    CHECK(setup.evidence[0].description == "first beat");
    CHECK(setup.evidence[1].description == "second beat");
    CHECK(setup.evidence[0].pointsAtKiller);
    CHECK_FALSE(setup.evidence[1].pointsAtKiller);
}

TEST_CASE("witness observations are attributed to the cast resident") {
    MysterySetup setup = decidedSetup();
    const StorylineCast cast =
        castStoryline(validStoryline(), castRoster(), setup, 19u);

    REQUIRE(setup.witnesses.size() == 2);
    CHECK(setup.witnesses[0].agent == cast.residentFor("neighbour"));
    CHECK(setup.witnesses[1].agent == cast.residentFor("rival"));
    CHECK(setup.witnesses[0].sawZoneId == "bakery_block");
    CHECK(setup.witnesses[0].atHour == doctest::Approx(21.5));
}

TEST_CASE("a cast storyline seeds facts that still hide the killer") {
    // The end-to-end guard: a template's witnesses become real facts, and the
    // leak rule survives content it did not author.
    MysterySetup setup = decidedSetup();
    const std::vector<Persona> roster = castRoster();
    castStoryline(validStoryline(), roster, setup, 55u);

    WorldState state;
    seedMysteryFacts(state, setup, roster);

    REQUIRE(state.facts().size() >= 3);  // one death + two testimonies
    for (const KnownFact& fact : state.facts()) {
        const bool namesKiller =
            fact.content.find(setup.killer) != std::string::npos;
        const bool aboutTheDeath =
            fact.content.find("dead") != std::string::npos ||
            fact.content.find("killed") != std::string::npos ||
            fact.content.find("murder") != std::string::npos;
        // Assigned first: doctest cannot decompose `&&` inside an assertion.
        const bool leaks = namesKiller && aboutTheDeath;
        CHECK_FALSE(leaks);
    }
}

TEST_CASE("a template with no roles casts nothing") {
    StorylineDef story = validStoryline();
    story.roles.clear();

    MysterySetup setup = decidedSetup();
    const StorylineCast cast = castStoryline(story, castRoster(), setup, 2u);

    CHECK(cast.assignments.empty());
    CHECK(setup.evidence.empty());
}

TEST_CASE("residentFor returns empty for a slot that was never declared") {
    MysterySetup setup = decidedSetup();
    const StorylineCast cast =
        castStoryline(validStoryline(), castRoster(), setup, 4u);
    CHECK(cast.residentFor("the_butler").empty());
}
