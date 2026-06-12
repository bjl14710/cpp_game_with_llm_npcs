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
