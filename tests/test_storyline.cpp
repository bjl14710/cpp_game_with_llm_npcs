// Parsing authored mystery templates (issue #186).
//
// Files are written into a temp directory rather than checked in as fixtures,
// so a case reads as "this text produces this result" without a reader having
// to open a second file.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Storyline.hpp"
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
