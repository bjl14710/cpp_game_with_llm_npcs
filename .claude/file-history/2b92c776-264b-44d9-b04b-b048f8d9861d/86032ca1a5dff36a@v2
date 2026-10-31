// Tests for PersonaFactory: deterministic, varied procedural pedestrians.
#include <set>
#include <string>

#include "PersonaFactory.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("makePedestrianPersona is deterministic for a given seed") {
    const Persona a = makePedestrianPersona(12345u, 0.0);
    const Persona b = makePedestrianPersona(12345u, 0.0);
    CHECK(a.name == b.name);
    CHECK(a.role == b.role);
    CHECK(a.traits == b.traits);
}

TEST_CASE("makePedestrianPersona fills in the required fields") {
    for (std::uint32_t s = 1; s <= 50; ++s) {
        const Persona p = makePedestrianPersona(s * 2654435761u, 0.2);
        CHECK_FALSE(p.name.empty());
        CHECK_FALSE(p.role.empty());
        CHECK_FALSE(p.traits.empty());
        CHECK_FALSE(p.speakingStyle.empty());
        // The system prompt always renders (and includes the action protocol).
        CHECK_FALSE(p.renderSystemPrompt().empty());
    }
}

TEST_CASE("policeChance bounds whether a pedestrian is police") {
    // With zero chance, nobody is police.
    for (std::uint32_t s = 1; s <= 100; ++s) {
        CHECK_FALSE(makePedestrianPersona(s * 2654435761u, 0.0).police);
    }
    // With certainty, everybody is, and they get a role.
    for (std::uint32_t s = 1; s <= 20; ++s) {
        const Persona p = makePedestrianPersona(s * 2654435761u, 1.0);
        CHECK(p.police);
        CHECK_FALSE(p.role.empty());
    }
}

TEST_CASE("the generated crowd is varied, not all identical") {
    std::set<std::string> names;
    for (std::uint32_t s = 1; s <= 40; ++s) {
        names.insert(makePedestrianPersona(s * 2654435761u, 0.15).name);
    }
    CHECK(names.size() > 10);  // far more than one distinct person across 40 seeds
}
