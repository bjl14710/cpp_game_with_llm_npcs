// Tests for Persona prompt rendering and the .persona file parser.
#include <string>

#include "PersonaLoader.hpp"
#include "doctest.h"

using llm_npc::parsePersonaText;
using llm_npc::Persona;
using llm_npc::PersonaParseResult;

TEST_CASE("renderSystemPrompt includes every populated field") {
    Persona p;
    p.name = "Marge Holloway";
    p.role = "bakery owner";
    p.traits = {"warm", "gossipy"};
    p.speakingStyle = "short sentences";
    p.knowledgeBoundary = "Knows bread.";
    p.extraDirectives = "Mention sourdough when greeting.";

    const std::string prompt = p.renderSystemPrompt();
    CHECK(prompt.find("You are Marge Holloway, a bakery owner.") != std::string::npos);
    CHECK(prompt.find("warm, gossipy") != std::string::npos);
    CHECK(prompt.find("short sentences") != std::string::npos);
    CHECK(prompt.find("Knows bread.") != std::string::npos);
    CHECK(prompt.find("Mention sourdough") != std::string::npos);
    CHECK(prompt.find("Stay in character") != std::string::npos);
}

TEST_CASE("parsePersonaText reads a full definition") {
    const std::string text =
        "name = Marge Holloway\n"
        "role = bakery owner\n"
        "traits = warm, gossipy, fiercely proud of her sourdough\n"
        "style = 1-3 short sentences, motherly\n"
        "knowledge = Knows the neighborhood and bread.\n"
        "spot = bakery\n"
        "position = -40, 28\n"
        "facing = 180\n"
        "---\n"
        "Greet regulars by guessing their usual order.\n"
        "Complain gently about the morning rush.\n";

    PersonaParseResult r = parsePersonaText(text, "baker");
    REQUIRE(r.ok);
    CHECK(r.value.id == "baker");
    CHECK(r.value.persona.name == "Marge Holloway");
    CHECK(r.value.persona.role == "bakery owner");
    REQUIRE(r.value.persona.traits.size() == 3);
    CHECK(r.value.persona.traits[2] == "fiercely proud of her sourdough");
    CHECK(r.value.persona.speakingStyle == "1-3 short sentences, motherly");
    CHECK(r.value.spotId == "bakery");
    CHECK(r.value.position.x == doctest::Approx(-40.f));
    CHECK(r.value.position.z == doctest::Approx(28.f));
    CHECK(r.value.position.y == doctest::Approx(0.f));
    CHECK(r.value.facingDeg == doctest::Approx(180.f));
    CHECK(r.value.persona.extraDirectives.find("Greet regulars") == 0);
    CHECK(r.value.persona.extraDirectives.find("morning rush") != std::string::npos);
}

TEST_CASE("action protocol gates arrest behind the police flag") {
    Persona civilian;
    civilian.name = "Marge";
    const std::string cp = civilian.renderSystemPrompt();
    CHECK(cp.find("[[ACTION: call_police]]") != std::string::npos);
    CHECK(cp.find("[[ACTION: arrest]]") == std::string::npos);

    Persona cop;
    cop.name = "Dana";
    cop.police = true;
    const std::string pp = cop.renderSystemPrompt();
    CHECK(pp.find("[[ACTION: arrest]]") != std::string::npos);
    CHECK(pp.find("[[ACTION: call_police]]") == std::string::npos);

    // Both get the mandatory mood contract and the asterisk-emote rule.
    for (const std::string* prompt : {&cp, &pp}) {
        CHECK(prompt->find("[[MOOD: embarrassed]]") != std::string::npos);
        CHECK(prompt->find("asterisks") != std::string::npos);
    }
}

TEST_CASE("parsePersonaText reads the police flag") {
    PersonaParseResult on = parsePersonaText("name = Dana\npolice = true\n", "cop");
    REQUIRE(on.ok);
    CHECK(on.value.persona.police);

    PersonaParseResult off = parsePersonaText("name = Marge\n", "baker");
    REQUIRE(off.ok);
    CHECK_FALSE(off.value.persona.police);

    PersonaParseResult odd = parsePersonaText("name = X\npolice = maybe\n", "x");
    REQUIRE(odd.ok);
    CHECK_FALSE(odd.value.persona.police);
}

TEST_CASE("parsePersonaText works without the extra-directives section") {
    PersonaParseResult r = parsePersonaText("name = Bob\nposition = 1, 2\n", "bob");
    REQUIRE(r.ok);
    CHECK(r.value.persona.extraDirectives.empty());
}

TEST_CASE("parsePersonaText rejects a missing name") {
    PersonaParseResult r = parsePersonaText("role = cop\n", "cop");
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("missing required 'name'") != std::string::npos);
}

TEST_CASE("parsePersonaText rejects malformed header lines") {
    PersonaParseResult r = parsePersonaText("name = A\nthis line has no equals\n", "x");
    CHECK_FALSE(r.ok);
    CHECK(r.error.find("without '='") != std::string::npos);
}

TEST_CASE("parsePersonaText rejects bad positions and unknown keys") {
    PersonaParseResult bad = parsePersonaText("name = A\nposition = 1\n", "x");
    CHECK_FALSE(bad.ok);
    CHECK(bad.error.find("bad position") != std::string::npos);

    PersonaParseResult unknown = parsePersonaText("name = A\ncolour = red\n", "x");
    CHECK_FALSE(unknown.ok);
    CHECK(unknown.error.find("unknown header key") != std::string::npos);
}
