// Tests for CharacterStore: independent persona/look records, round-trips,
// and corrupt-row degradation. Uses a temp database per case, mirroring
// test_conversation_store.cpp.
#include <cstdio>
#include <filesystem>
#include <string>

#include "CharacterParts.hpp"
#include "CharacterStore.hpp"
#include "PersonaLoader.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// Unique temp path per test run; removed by the fixture destructor.
struct TempDb {
    std::filesystem::path path;
    TempDb(const char* name) {
        path = std::filesystem::temp_directory_path() /
               (std::string("llm_npc_chartest_") + name + ".sqlite3");
        std::filesystem::remove(path);
    }
    ~TempDb() { std::filesystem::remove(path); }
};

CharacterLook sampleLook() {
    CharacterLook look;
    look.part(PartCategory::Body) = "body_block";
    look.part(PartCategory::Head) = "head_block";
    look.part(PartCategory::Eyes) = "eyes_dot";
    look.part(PartCategory::Hair) = "hair_spikes";
    look.paletteId = "cool";
    return look;
}

}  // namespace

TEST_CASE("persona and look save/load independently by character id") {
    TempDb db("roundtrip");
    CharacterStore store(db.path);
    REQUIRE(store.ok());

    // Look saved WITHOUT a persona — the records must not require each
    // other (independence is the ticket's core constraint).
    CHECK(store.saveLook("c1", sampleLook()));
    CharacterLook look;
    CHECK(store.loadLook("c1", look));
    CHECK(look.part(PartCategory::Body) == "body_block");
    std::string personaText;
    CHECK_FALSE(store.loadPersona("c1", personaText));

    CHECK(store.savePersona("c1", "name = Test Person\nrole = tester\n"));
    CHECK(store.loadPersona("c1", personaText));
    CHECK(personaText.find("Test Person") != std::string::npos);

    // Upsert: saving again replaces, not duplicates.
    CHECK(store.savePersona("c1", "name = Renamed\n"));
    CHECK(store.loadPersona("c1", personaText));
    CHECK(personaText.find("Renamed") != std::string::npos);
}

TEST_CASE("loadAll returns only characters with both records intact") {
    TempDb db("loadall");
    CharacterStore store(db.path);
    REQUIRE(store.ok());

    // c_full has both records; c_lookless has a persona only and must be
    // skipped (the look JSON parse gate is covered in
    // test_character_parts; missing-row degradation is covered here).
    CHECK(store.savePersona("c_full", "name = Whole\n"));
    CHECK(store.saveLook("c_full", sampleLook()));
    CHECK(store.savePersona("c_lookless", "name = NoBody\n"));
    const auto all = store.loadAll();
    REQUIRE(all.size() == 1);
    CHECK(all[0].characterId == "c_full");
    CHECK(all[0].personaText.find("Whole") != std::string::npos);
    CHECK(all[0].look.part(PartCategory::Hair) == "hair_spikes");

    // The stored persona text parses through the ONE persona parser.
    const PersonaParseResult parsed =
        parsePersonaText(all[0].personaText, all[0].characterId);
    CHECK(parsed.ok);
    CHECK(parsed.value.persona.name == "Whole");
}

TEST_CASE("renderPersonaText round-trips through parsePersonaText") {
    LoadedPersona loaded;
    loaded.id = "custom_1";
    loaded.persona.name = "Piper Vale";
    loaded.persona.role = "street artist";
    loaded.persona.traits = {"bold", "wry"};
    loaded.persona.speakingStyle = "clipped, confident";
    loaded.persona.knowledgeBoundary = "Knows the plaza and its regulars.";
    loaded.persona.extraDirectives = "Never reveal where the murals come from.";
    loaded.position = {4.f, 0.f, -9.f};
    loaded.facingDeg = 90.f;
    loaded.spotId = "plaza";

    const std::string text = renderPersonaText(loaded);
    const PersonaParseResult parsed = parsePersonaText(text, "custom_1");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    CHECK(parsed.value.persona.name == "Piper Vale");
    CHECK(parsed.value.persona.role == "street artist");
    REQUIRE(parsed.value.persona.traits.size() == 2);
    CHECK(parsed.value.persona.traits[1] == "wry");
    CHECK(parsed.value.persona.speakingStyle == "clipped, confident");
    CHECK(parsed.value.persona.extraDirectives ==
          "Never reveal where the murals come from.");
    CHECK(parsed.value.position.x == doctest::Approx(4.f));
    CHECK(parsed.value.position.z == doctest::Approx(-9.f));
    CHECK(parsed.value.facingDeg == doctest::Approx(90.f));
    CHECK(parsed.value.spotId == "plaza");
    CHECK_FALSE(parsed.value.persona.police);
}

TEST_CASE("unopenable database degrades to ok() == false, not a crash") {
    // A directory path can't be opened as a database file.
    const auto dir = std::filesystem::temp_directory_path() / "llm_npc_chartest_dir";
    std::filesystem::create_directories(dir);
    CharacterStore store(dir);
    CHECK_FALSE(store.ok());
    CHECK_FALSE(store.savePersona("x", "name = X\n"));
    CHECK_FALSE(store.saveLook("x", sampleLook()));
    std::string text;
    CHECK_FALSE(store.loadPersona("x", text));
    CHECK(store.loadAll().empty());
    std::filesystem::remove_all(dir);
}
