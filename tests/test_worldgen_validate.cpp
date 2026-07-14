// LLM world-generation validator (plan: llm-world-generation). The
// VALIDATOR is the deliverable — all tests offline, no model calls.
// Map-side validation is shared with the sandbox (validateMap, tested in
// test_sandbox_map.cpp); these cover the cast side and the envelope.
#include <string>
#include <vector>

#include "WorldGen.hpp"
#include "WorldGenValidate.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

std::vector<TraitDef> library() {
    TraitDef grumpy;
    grumpy.id = "grumpy";
    grumpy.name = "Grumpy";
    grumpy.behaviorRules = {"Complain briefly."};
    TraitDef poetic;
    poetic.id = "poetic";
    poetic.name = "Poetic";
    poetic.behaviorRules = {"One image per reply."};
    return {grumpy, poetic};
}

const char* kValidPersona =
    "name = Old Salt Harbard\n"
    "role = retired sailor\n"
    "position = 4, 8\n"
    "trait = grumpy\n"
    "look = body_bulk, head_block, eyes_sleepy, hair_buzz, mouth_neutral, slate\n";

}  // namespace

TEST_CASE("validateCast: every failure class yields a specific error") {
    std::vector<GeneratedCharacter> cast;
    cast.push_back({kValidPersona});                                   // fine
    cast.push_back({"role = nameless\n"});                             // no name
    cast.push_back({std::string(kValidPersona) + "trait = brooding\n"});  // dup name TOO
    cast.push_back({"name = Kid One\n"
                    "look = body_slim, head_round, eyes_wide, hair_dreads, "
                    "mouth_o, sky\n"});                                // bad part
    cast.push_back({"name = Kid Two\n"});                              // no look

    const auto errors = validateCast(cast, library());
    auto has = [&](const std::string& whereBit, const std::string& reasonBit) {
        for (const CastError& e : errors) {
            if (e.where.find(whereBit) != std::string::npos &&
                e.reason.find(reasonBit) != std::string::npos) {
                return true;
            }
        }
        return false;
    };
    CHECK(has("characters[1]", "missing required 'name'"));
    CHECK(has("characters[2]", "duplicate character name"));
    CHECK(has("characters[2]", "unknown trait id 'brooding'"));  // lists valid ids
    CHECK(has("characters[2]", "valid: grumpy, poetic"));
    CHECK(has("characters[3]", "unknown part: hair_dreads"));
    CHECK(has("characters[3]", "nearest valid id: hair_braids"));
    CHECK(has("characters[4]", "missing 'look"));
    // The valid entry contributed no errors.
    for (const CastError& e : errors) CHECK(e.where != "characters[0]");
}

TEST_CASE("generated characters load through the EXISTING loaders") {
    // One entrance: a cast that validates is, by construction, loadable by
    // parsePersonaText + lookIsValid — the same calls validateCast made.
    const std::vector<GeneratedCharacter> cast{{kValidPersona}};
    CHECK(validateCast(cast, library()).empty());
    const PersonaParseResult parsed = parsePersonaText(kValidPersona, "gen");
    REQUIRE(parsed.ok);
    CHECK(parsed.value.persona.name == "Old Salt Harbard");
    CHECK(lookIsValid(parsed.value.look));
    CHECK(parsed.value.persona.traitIds == std::vector<std::string>{"grumpy"});
}

TEST_CASE("extractJsonObject tolerates fences and prose") {
    std::string out;
    // Fences + prose.
    REQUIRE(extractJsonObject(
        "Sure! Here's your map:\n```json\n{\"version\": 1, \"name\": \"x\"}\n``` hope it helps",
        out));
    CHECK(out == "{\"version\": 1, \"name\": \"x\"}");
    // Nested braces inside strings (and escaped quotes).
    REQUIRE(extractJsonObject(
        R"(noise {"a": "curly } inside", "b": "esc \" quote", "c": {"d": 1}} tail)", out));
    CHECK(out ==
          R"({"a": "curly } inside", "b": "esc \" quote", "c": {"d": 1}})");
    // Arrays count as the envelope too.
    REQUIRE(extractJsonObject("prefix [1, 2, {\"x\": 3}] suffix", out));
    CHECK(out == "[1, 2, {\"x\": 3}]");
    // No JSON at all — false, never partial.
    CHECK_FALSE(extractJsonObject("there is no json here", out));
    CHECK_FALSE(extractJsonObject("unbalanced {\"a\": 1", out));
}

TEST_CASE("retry feedback renders every error verbatim") {
    std::vector<MapError> mapErrors{{"pieces[3]", "unknown piece id 'shp_bakery'"}};
    std::vector<CastError> castErrors{{"characters[0]", "duplicate character name 'Kid'"}};
    const std::string feedback = renderRetryFeedback(mapErrors, castErrors);
    CHECK(feedback.find("INVALID") != std::string::npos);
    CHECK(feedback.find("pieces[3]: unknown piece id 'shp_bakery'") != std::string::npos);
    CHECK(feedback.find("characters[0]: duplicate character name 'Kid'") !=
          std::string::npos);
    CHECK(feedback.find("ONLY the corrected JSON") != std::string::npos);
}

TEST_CASE("generation parsers: cast, village, ids, links (offline)") {
    // Cast contract.
    std::vector<GeneratedCharacter> cast;
    REQUIRE(parseGeneratedCast(
        R"({"characters": ["name = A\nlook = x\n", "name = B\n"]})", cast));
    REQUIRE(cast.size() == 2);
    CHECK(cast[0].personaText.find("name = A") != std::string::npos);
    CHECK_FALSE(parseGeneratedCast(R"({"characters": []})", cast));   // empty
    CHECK_FALSE(parseGeneratedCast(R"({"people": ["x"]})", cast));    // wrong key
    CHECK_FALSE(parseGeneratedCast(R"({"characters": [7]})", cast));  // non-string

    // Stable generated ids.
    CHECK(generatedCharacterId("Old Salt Harbard") == "gen_old_salt_harbard");
    CHECK(generatedCharacterId("Mo'ana O-Kai") == "gen_mo_ana_o_kai");

    // Village contract + link cross-check.
    SandboxMap map;
    std::vector<GeneratedCharacter> villagers;
    const char* village = R"({
      "map": {"version": 1, "name": "cove", "tile": 8,
              "pieces": [{"piece": "bench", "x": 0, "z": 0}],
              "npcs": [{"source": "character:gen_old_salt_harbard",
                        "x": 12.0, "z": 4.0, "facing": 180.0},
                       {"source": "character:gen_nobody",
                        "x": 14.0, "z": 4.0, "facing": 0.0}]},
      "characters": ["name = Old Salt Harbard\n"]})";
    REQUIRE(parseGeneratedVillage(village, map, villagers));
    CHECK(map.name == "cove");
    REQUIRE(villagers.size() == 1);
    const auto links = validateVillageLinks(map, villagers);
    REQUIRE(links.size() == 1);  // gen_nobody flagged, harbard linked
    CHECK(links[0].where == "npcs[1]");
    CHECK(links[0].reason.find("gen_nobody") != std::string::npos);
    CHECK(links[0].reason.find("valid: gen_old_salt_harbard") != std::string::npos);
}
