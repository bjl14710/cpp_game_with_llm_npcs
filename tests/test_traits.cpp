// Scaffolded stubs for the trait system (plan: npc-traits-and-ratings).
// Un-skip each in the commit that implements its step.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Persona.hpp"
#include "PersonaLoader.hpp"
#include "Trait.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("trait files parse and malformed ones are skipped with errors") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "llm_npc_traits_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    const auto write = [&](const char* stem, const char* text) {
        std::ofstream out(dir / (std::string(stem) + ".trait"));
        out << text;
    };
    write("grumpy",
          "name = Grumpy\n"
          "rule = Complain briefly before helping.\n"
          "example_user = Good morning!\n"
          "example_npc = It's a day. What do you want?\n");
    write("orphan",  // example_npc without a preceding example_user
          "name = Broken\n"
          "rule = A rule.\n"
          "example_npc = Who am I answering?\n");
    write("ruleless", "name = Empty\n");  // needs at least one rule

    std::vector<std::string> errors;
    const auto traits = loadAllTraits(dir, &errors);
    REQUIRE(traits.size() == 1);
    CHECK(traits[0].id == "grumpy");
    CHECK(traits[0].name == "Grumpy");
    REQUIRE(traits[0].behaviorRules.size() == 1);
    REQUIRE(traits[0].examples.size() == 1);
    CHECK(traits[0].examples[0].userLine == "Good morning!");
    CHECK(traits[0].examples[0].npcLine == "It's a day. What do you want?");
    REQUIRE(errors.size() == 2);
    CHECK(errors[0].find("example_npc without") != std::string::npos);
    CHECK(errors[1].find("at least one rule") != std::string::npos);
    fs::remove_all(dir);

    // The SHIPPED library loads cleanly and is non-trivial.
    fs::path shipped = "traits";
    for (int i = 0; i < 4 && !fs::exists(shipped); ++i) shipped = ".." / shipped;
    REQUIRE(fs::exists(shipped));
    std::vector<std::string> shippedErrors;
    const auto library = loadAllTraits(shipped, &shippedErrors);
    CHECK(shippedErrors.empty());
    CHECK(library.size() >= 6);
    for (const TraitDef& trait : library) {
        CAPTURE(trait.id);
        CHECK_FALSE(trait.behaviorRules.empty());
    }
}

TEST_CASE("prompt assembly order: rules -> examples -> memory -> reinforcement") {
    // The anti-drift contract (brief item 4), pinned by INDEX comparison:
    // identity < trait rules < examples < memory < reinforcement < ACTIONS.
    TraitDef grumpy;
    grumpy.id = "grumpy";
    grumpy.name = "Grumpy";
    grumpy.behaviorRules = {"Complain briefly before helping."};
    grumpy.examples = {{"Good morning!", "It's a day. What do you want?"}};

    Persona p;
    p.name = "Marge";
    p.role = "baker";
    p.traitIds = {"grumpy"};
    const std::string prompt = p.renderSystemPrompt(
        "The player bought rye last week.", "", {&grumpy});

    const auto identity = prompt.find("You are Marge");
    const auto rules = prompt.find("Complain briefly before helping.");
    const auto examples = prompt.find("It's a day. What do you want?");
    const auto memory = prompt.find("The player bought rye last week.");
    const auto reinforcement = prompt.find("No matter what the conversation");
    const auto actions = prompt.find("ACTIONS: ");
    REQUIRE(identity != std::string::npos);
    REQUIRE(rules != std::string::npos);
    REQUIRE(examples != std::string::npos);
    REQUIRE(memory != std::string::npos);
    REQUIRE(reinforcement != std::string::npos);
    REQUIRE(actions != std::string::npos);
    CHECK(identity < rules);
    CHECK(rules < examples);
    CHECK(examples < memory);
    CHECK(memory < reinforcement);
    CHECK(reinforcement < actions);

    // Trait-less rendering is byte-identical to the pre-trait behavior.
    Persona bare = p;
    bare.traitIds.clear();
    CHECK(bare.renderSystemPrompt("m", "g") == bare.renderSystemPrompt("m", "g", {}));
}

TEST_CASE("unknown trait ids demote and the cap is a parse error") {
    // Stale ids demote (render composes only what resolves — the spawn
    // site logs the reason); more than 3 trait lines is an author error.
    const auto parsed = llm_npc::parsePersonaText(
        "name = Piper\ntrait = grumpy\ntrait = nonexistent\n", "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    REQUIRE(parsed.value.persona.traitIds.size() == 2);

    const auto capped = llm_npc::parsePersonaText(
        "name = Piper\ntrait = a\ntrait = b\ntrait = c\ntrait = d\n", "piper");
    CHECK_FALSE(capped.ok);
    CHECK(capped.error.find("more than 3") != std::string::npos);
}

TEST_CASE("persona trait keys round-trip through renderPersonaText") {
    const auto parsed = llm_npc::parsePersonaText(
        "name = Piper\nposition = 1, 2\ntrait = grumpy\ntrait = poetic\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    const auto again =
        llm_npc::parsePersonaText(llm_npc::renderPersonaText(parsed.value), "piper");
    REQUIRE_MESSAGE(again.ok, again.error);
    REQUIRE(again.value.persona.traitIds.size() == 2);
    CHECK(again.value.persona.traitIds[0] == "grumpy");
    CHECK(again.value.persona.traitIds[1] == "poetic");
}

TEST_CASE("rating capture appends JSONL and never touches live prompts" *
          doctest::skip()) {
    // Step 4. TODO(traits): good -> saves/ratings/candidates.jsonl row
    // {trait ids, persona id, player line, npc reply, ts}; bad ->
    // rejected.jsonl; the rendered system prompt is byte-identical before
    // and after rating (no auto-promotion, by design).
}
