// Scaffolded stubs for the trait system (plan: npc-traits-and-ratings).
// Un-skip each in the commit that implements its step.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

TEST_CASE("prompt assembly order: rules -> examples -> memory -> reinforcement" *
          doctest::skip()) {
    // Step 2. TODO(traits): render a persona with trait 'grumpy' and a
    // non-empty memory; assert string indices:
    //   backstory < trait rules < examples < memory summary < reinforcement
    // — the post-memory reinforcement is the anti-drift contract (brief
    // item 4) and MUST be pinned by index comparison, not presence.
}

TEST_CASE("unknown trait ids demote at spawn with a logged reason" *
          doctest::skip()) {
    // Step 2/3. TODO(traits): persona `trait = nonexistent` still spawns;
    // remaining valid traits render; reason surfaced (stale-look rule).
    // Also: >3 trait keys = parse error (cap keeps prompts bounded).
}

TEST_CASE("persona trait keys round-trip through renderPersonaText" *
          doctest::skip()) {
    // Step 2. TODO(traits): repeated `trait =` lines parse in order and
    // render back identically (same guarantee as schedule/look lines).
}

TEST_CASE("rating capture appends JSONL and never touches live prompts" *
          doctest::skip()) {
    // Step 4. TODO(traits): good -> saves/ratings/candidates.jsonl row
    // {trait ids, persona id, player line, npc reply, ts}; bad ->
    // rejected.jsonl; the rendered system prompt is byte-identical before
    // and after rating (no auto-promotion, by design).
}
