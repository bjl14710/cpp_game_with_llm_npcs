// Persona `look` header key + the lookForPersona authored-or-fallback rule
// (plan: shared-character-library, issue #95). The look line is parsed
// STRUCTURALLY by PersonaLoader; catalog validation happens in
// lookForPersona so a stale part id demotes to the deterministic fallback
// instead of killing the persona file.
#include <string>

#include "CharacterParts.hpp"
#include "PersonaLoader.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("a look header line parses into the five look fields") {
    const auto parsed = parsePersonaText(
        "name = Piper\n"
        "position = 1, 2\n"
        "look = body_round, head_round, eyes_happy, hair_bowl, warm\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    REQUIRE(parsed.value.hasLook);
    CHECK(parsed.value.look.part(PartCategory::Body) == "body_round");
    CHECK(parsed.value.look.part(PartCategory::Head) == "head_round");
    CHECK(parsed.value.look.part(PartCategory::Eyes) == "eyes_happy");
    CHECK(parsed.value.look.part(PartCategory::Hair) == "hair_bowl");
    CHECK(parsed.value.look.paletteId == "warm");
}

TEST_CASE("a persona without a look line simply has none") {
    const auto parsed = parsePersonaText("name = Piper\nposition = 1, 2\n", "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    CHECK_FALSE(parsed.value.hasLook);
}

TEST_CASE("a malformed look line is a parse error, like a bad position") {
    // Wrong item count is an author typo, not a stale catalog — fail loudly.
    const auto parsed = parsePersonaText(
        "name = Piper\n"
        "look = body_round, head_round, eyes_happy\n",
        "piper");
    CHECK_FALSE(parsed.ok);
    CHECK(parsed.error.find("look") != std::string::npos);
}

TEST_CASE("renderPersonaText round-trips the look line") {
    auto parsed = parsePersonaText(
        "name = Piper\n"
        "position = 1, 2\n"
        "look = body_block, head_block, eyes_visor, hair_spikes, cool\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    const auto again = parsePersonaText(renderPersonaText(parsed.value), "piper");
    REQUIRE_MESSAGE(again.ok, again.error);
    REQUIRE(again.value.hasLook);
    for (int c = 0; c < kPartCategoryCount; ++c) {
        CHECK(again.value.look.partIds[c] == parsed.value.look.partIds[c]);
    }
    CHECK(again.value.look.paletteId == "cool");
}

TEST_CASE("renderPersonaText omits the look line when there is none") {
    LoadedPersona loaded;
    loaded.persona.name = "Piper";
    CHECK(renderPersonaText(loaded).find("look") == std::string::npos);
}

TEST_CASE("lookForPersona prefers a valid authored look") {
    CharacterLook authored;
    authored.part(PartCategory::Body) = "body_round";
    authored.part(PartCategory::Head) = "head_round";
    authored.part(PartCategory::Eyes) = "eyes_wide";
    authored.part(PartCategory::Hair) = "hair_tuft";
    authored.paletteId = "warm";
    REQUIRE(lookIsValid(authored));

    std::string why;
    const CharacterLook chosen = lookForPersona("Piper", &authored, &why);
    CHECK(why.empty());
    for (int c = 0; c < kPartCategoryCount; ++c) {
        CHECK(chosen.partIds[c] == authored.partIds[c]);
    }
    CHECK(chosen.paletteId == "warm");
}

TEST_CASE("an invalid authored look falls back deterministically, with a reason") {
    CharacterLook stale;
    stale.part(PartCategory::Body) = "body_round";
    stale.part(PartCategory::Head) = "head_round";
    stale.part(PartCategory::Eyes) = "eyes_wide";
    stale.part(PartCategory::Hair) = "hair_dreadlocks";  // not in the catalog
    stale.paletteId = "warm";

    std::string why;
    const CharacterLook chosen = lookForPersona("Piper", &stale, &why);
    CHECK_FALSE(why.empty());                      // reason surfaced for the log
    CHECK(lookIsValid(chosen));                    // the NPC still spawns valid
    CHECK(chosen.part(PartCategory::Hair) != "hair_dreadlocks");
    // Fallback equals the no-authored-look result: ONE deterministic rule.
    const CharacterLook bare = lookForPersona("Piper");
    for (int c = 0; c < kPartCategoryCount; ++c) {
        CHECK(chosen.partIds[c] == bare.partIds[c]);
    }
}

TEST_CASE("the deterministic fallback is stable and valid per name") {
    const CharacterLook a1 = lookForPersona("Marge Holloway");
    const CharacterLook a2 = lookForPersona("Marge Holloway");
    CHECK(lookIsValid(a1));
    for (int c = 0; c < kPartCategoryCount; ++c) {
        CHECK(a1.partIds[c] == a2.partIds[c]);     // same name → same look, always
    }
    CHECK(a1.paletteId == a2.paletteId);

    // Different names hash apart (fixed inputs, so this is deterministic —
    // it guards the hash actually feeding the seed, not luck).
    const CharacterLook b = lookForPersona("Officer Dana Brooks");
    CHECK(lookIsValid(b));
    bool anyDifference = a1.paletteId != b.paletteId;
    for (int c = 0; c < kPartCategoryCount; ++c) {
        anyDifference = anyDifference || a1.partIds[c] != b.partIds[c];
    }
    CHECK(anyDifference);
}
