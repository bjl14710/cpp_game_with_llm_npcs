// Self-contained roster test. At commit 6 copy this into the repo as
// tests/test_persona_roster.cpp (no append/edit of test_persona.cpp needed).
#include <filesystem>
#include <string>
#include <vector>

#include "PersonaLoader.hpp"
#include "doctest.h"

TEST_CASE("the shipped personas directory yields the full ten-citizen roster") {
    namespace fs = std::filesystem;
    // The test binary runs from tests/ (make) or build/ (ctest); walk up to
    // wherever personas/ lives.
    fs::path dir = "personas";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    std::vector<std::string> errors;
    const auto roster = llm_npc::loadAllPersonas(dir, &errors);
    CHECK(errors.empty());
    REQUIRE(roster.size() == 10);

    // Sorted by filename; every persona is named, placed, and distinct.
    CHECK(roster.front().id == "baker");
    for (const auto& loaded : roster) {
        CHECK_FALSE(loaded.persona.name.empty());
        CHECK_FALSE(loaded.spotId.empty());
        for (const auto& other : roster) {
            if (&other == &loaded) continue;
            const bool samePlace = other.position.x == loaded.position.x &&
                                   other.position.z == loaded.position.z;
            CHECK_FALSE(samePlace);
        }
    }
}

TEST_CASE("schedule header lines parse into entries and round-trip") {
    const auto parsed = llm_npc::parsePersonaText(
        "name = Piper\n"
        "position = 1, 2\n"
        "schedule = 5-12, -70, -36, baking bread\n"
        "schedule = 22-6, 4.5, -8, night watch, extra shift\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    REQUIRE(parsed.value.schedule.size() == 2);
    CHECK(parsed.value.schedule[0].startHour == doctest::Approx(5.f));
    CHECK(parsed.value.schedule[0].endHour == doctest::Approx(12.f));
    CHECK(parsed.value.schedule[0].position.x == doctest::Approx(-70.f));
    CHECK(parsed.value.schedule[0].position.z == doctest::Approx(-36.f));
    CHECK(parsed.value.schedule[0].activity == "baking bread");
    // The activity keeps its own commas (everything after the third).
    CHECK(parsed.value.schedule[1].activity == "night watch, extra shift");

    // renderPersonaText writes the lines back out; reparsing matches.
    const auto again =
        llm_npc::parsePersonaText(llm_npc::renderPersonaText(parsed.value), "piper");
    REQUIRE_MESSAGE(again.ok, again.error);
    REQUIRE(again.value.schedule.size() == 2);
    CHECK(again.value.schedule[1].startHour == doctest::Approx(22.f));
    CHECK(again.value.schedule[1].activity == "night watch, extra shift");
}

TEST_CASE("malformed schedule lines are named errors") {
    for (const char* bad :
         {"schedule = 5, -70, -36, baking",        // no hour range
          "schedule = 5-25, -70, -36, baking",     // hour out of range
          "schedule = 5-12, -70, baking",          // missing z
          "schedule = 5-12, -70, -36,"}) {         // empty activity
        CAPTURE(bad);
        const auto parsed = llm_npc::parsePersonaText(
            std::string("name = X\n") + bad + "\n", "x");
        CHECK_FALSE(parsed.ok);
        CHECK(parsed.error.find("schedule") != std::string::npos);
    }
}
