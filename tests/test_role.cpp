// Storyline roles layered onto a persona (plan: role-layer).
//
// All cases SKIPPED: loadAllRoles returns nothing and renderRoleBlock returns
// an empty string. Un-skip each in the commit that implements what it covers.
//
// A NOTE ON WHAT THESE CASES CANNOT DO. Three of the plan's acceptance criteria
// are about what a live model emits — no confession under pressure, no spurious
// [[ACTION: call_police]], and the killer's mood not separable from the
// secret-keepers'. None of those is unit-testable, and the two that were
// discovered at all were only visible because the model was actually run. They
// belong to tools/role_leak_probe.py, and a green run of THIS file is not
// evidence the role layer works.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Persona.hpp"
#include "Role.hpp"
#include "Trait.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

std::filesystem::path tempDir(const std::string& tag) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("llm_npc_role_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// A complete, valid role file — used where the case is about something else.
const char* const kKillerRole = R"(name = Killer
directive = You did this, and you will not admit it, ever.
directive = Give your account calmly and steer the talk elsewhere.
demeanour = You are exactly as warm as you have always been. You are not nervous, not hostile, and not defensive.
example_user = Where were you that night?
example_npc = Home, same as any night. Why, what's happened?
)";

}  // namespace

// ---- parsing (step 1) -----------------------------------------------------

TEST_CASE("a well-formed role file parses into every field") {
    const auto dir = tempDir("good");
    writeFile(dir / "killer.role", kKillerRole);

    std::vector<std::string> errors;
    const std::vector<RoleDef> roles = loadAllRoles(dir, &errors);

    CHECK(errors.empty());
    REQUIRE(roles.size() == 1);
    CHECK(roles[0].id == "killer");          // from the filename stem
    CHECK(roles[0].name == "Killer");
    REQUIRE(roles[0].directives.size() == 2);
    CHECK(roles[0].directives[0] == "You did this, and you will not admit it, ever.");
    CHECK(roles[0].directives[1].find("steer the talk elsewhere") != std::string::npos);
    CHECK(roles[0].demeanour.find("exactly as warm") != std::string::npos);
    REQUIRE(roles[0].examples.size() == 1);
    CHECK(roles[0].examples[0].userLine == "Where were you that night?");
    CHECK(roles[0].examples[0].npcLine.find("Home, same as any night") != std::string::npos);
}

TEST_CASE("a role file with no demeanour fails to load") {
    // THE RULE THAT IS NOT IN Trait.cpp. Measured: without an explicit
    // instruction to hold baseline warmth the model defaults to hostile and the
    // mood tag identifies the killer.
    const auto dir = tempDir("nodemeanour");
    writeFile(dir / "killer.role",
              "name = Killer\ndirective = Deny everything.\n");

    std::vector<std::string> errors;
    const std::vector<RoleDef> roles = loadAllRoles(dir, &errors);

    CHECK(roles.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("demeanour") != std::string::npos);
}

TEST_CASE("an EMPTY demeanour fails too") {
    // The likeliest way an author satisfies the key without satisfying the
    // requirement — and the version that looks like an answer in a diff.
    const auto dir = tempDir("blankdemeanour");
    writeFile(dir / "killer.role",
              "name = Killer\ndirective = Deny everything.\ndemeanour =\n");

    std::vector<std::string> errors;
    const std::vector<RoleDef> roles = loadAllRoles(dir, &errors);

    CHECK(roles.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("empty 'demeanour'") != std::string::npos);
}

TEST_CASE("an orphan example line fails to load") {
    // Same contract Trait.cpp holds: an orphan is an authoring mistake worth
    // failing loudly over, in both directions.
    const std::string base =
        "name = Killer\ndirective = Deny everything.\ndemeanour = Stay warm.\n";

    const auto dangling = tempDir("orphanuser");
    writeFile(dangling / "k.role", base + "example_user = Where were you?\n");
    std::vector<std::string> e1;
    CHECK(loadAllRoles(dangling, &e1).empty());
    REQUIRE(e1.size() == 1);
    CHECK(e1[0].find("example_user without a following example_npc") !=
          std::string::npos);

    const auto leading = tempDir("orphannpc");
    writeFile(leading / "k.role", base + "example_npc = Home, same as always.\n");
    std::vector<std::string> e2;
    CHECK(loadAllRoles(leading, &e2).empty());
    REQUIRE(e2.size() == 1);
    CHECK(e2[0].find("example_npc without a preceding example_user") !=
          std::string::npos);
}

TEST_CASE("an unknown key is a named error") {
    // A typo'd key must not be silently dropped — `demeanor` instead of
    // `demeanour` would otherwise produce a role with no anti-tell line and no
    // complaint.
    const auto dir = tempDir("unknownkey");
    writeFile(dir / "k.role",
              "name = Killer\ndirective = Deny.\ndemeanour = Stay warm.\n"
              "demeanor = Stay warm.\n");

    std::vector<std::string> errors;
    CHECK(loadAllRoles(dir, &errors).empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("unknown key 'demeanor'") != std::string::npos);
}

TEST_CASE("one malformed role does not take the others with it") {
    const auto dir = tempDir("mixed");
    writeFile(dir / "killer.role", kKillerRole);
    writeFile(dir / "broken.role", "name = Broken\n");  // no directive, no demeanour

    std::vector<std::string> errors;
    const std::vector<RoleDef> roles = loadAllRoles(dir, &errors);

    REQUIRE(roles.size() == 1);
    CHECK(roles[0].id == "killer");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("broken") != std::string::npos);
}

TEST_CASE("a missing roles directory yields nothing rather than throwing") {
    std::vector<std::string> errors;
    CHECK(loadAllRoles("/does/not/exist/roles", &errors).empty());
}

TEST_CASE("roles load in a reproducible order") {
    // directory_iterator order is unspecified; sorting keeps error output from
    // varying by machine.
    const auto dir = tempDir("order");
    const std::string body =
        "directive = Do the thing.\ndemeanour = Stay warm.\n";
    writeFile(dir / "witness.role", "name = Witness\n" + body);
    writeFile(dir / "bystander.role", "name = Bystander\n" + body);
    writeFile(dir / "killer.role", "name = Killer\n" + body);
    writeFile(dir / "notes.txt", "name = Ignored\n" + body);

    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE(roles.size() == 3);
    CHECK(roles[0].id == "bystander");
    CHECK(roles[1].id == "killer");
    CHECK(roles[2].id == "witness");
}

TEST_CASE("renderRoleBlock returns nothing for a null role") {
    // The guard that makes the feature invisible until used.
    CHECK(renderRoleBlock(nullptr, "").empty());
    CHECK(renderRoleBlock(nullptr, "you were at the mill").empty());
}

TEST_CASE("the rendered block carries directives, secret and demeanour") {
    const auto dir = tempDir("render");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE(roles.size() == 1);

    const std::string block =
        renderRoleBlock(&roles[0], "You were at the mill, not at home.");

    CHECK(block.find("You did this") != std::string::npos);
    CHECK(block.find("You were at the mill") != std::string::npos);
    CHECK(block.find("exactly as warm") != std::string::npos);
    CHECK(block.find("Where were you that night?") != std::string::npos);

    // The demeanour lands AFTER the directives and the secret: it is the
    // instruction most at risk of being ignored, and trailing lines carry most
    // weight with small models.
    CHECK(block.find("exactly as warm") > block.find("You did this"));
    CHECK(block.find("exactly as warm") > block.find("You were at the mill"));
}

TEST_CASE("the role id never appears as a bare token in the block") {
    // An acceptance criterion: neither the role id nor the raw secret may show
    // up verbatim in a transcript, and the id is the one a debug string would
    // leak. The NAME ("Killer") is prose the model reads; the ID ("killer") is
    // a key it should never see.
    const auto dir = tempDir("noid");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE(roles.size() == 1);
    REQUIRE(roles[0].id == "killer");

    const std::string block = renderRoleBlock(&roles[0], "");
    CHECK(block.find("killer") == std::string::npos);  // lowercase id
}

TEST_CASE("an empty secret still renders the demeanour") {
    // bystander has no secret. The honest baseline must not end up the one
    // character with no instruction to stay warm.
    const auto dir = tempDir("nosecret");
    writeFile(dir / "bystander.role",
              "name = Bystander\ndirective = Answer plainly.\n"
              "demeanour = You are relaxed and have nothing to hide.\n");
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE(roles.size() == 1);

    const std::string block = renderRoleBlock(&roles[0], "");
    CHECK_FALSE(block.empty());
    CHECK(block.find("nothing to hide") != std::string::npos);
}

TEST_CASE("findRole resolves a known id and returns nullptr for an unknown one") {
    // LIVE — findRole ships implemented with the scaffold. A linear scan over
    // four roles is not a search problem, and having it work from the first
    // commit lets step 4 wire an NPC's assigned role without waiting on the
    // parser.
    //
    // Unknown ids are demoted with a log at spawn, exactly as unknown traitIds
    // already are: never a crash, never a silent substitution for a different
    // role — which for a killer would be a mystery with two of them.
    std::vector<RoleDef> roles;
    RoleDef killer;
    killer.id = "killer";
    killer.name = "Killer";
    killer.demeanour = "Stay exactly as warm as you have always been.";
    roles.push_back(killer);
    RoleDef bystander;
    bystander.id = "bystander";
    bystander.name = "Bystander";
    bystander.demeanour = "Answer plainly; you have nothing to hide.";
    roles.push_back(bystander);

    const RoleDef* found = findRole(roles, "killer");
    REQUIRE(found != nullptr);
    CHECK(found->name == "Killer");
    CHECK(findRole(roles, "bystander")->id == "bystander");

    CHECK(findRole(roles, "accomplice") == nullptr);
    CHECK(findRole(roles, "") == nullptr);
    CHECK(findRole({}, "killer") == nullptr);
}

TEST_CASE("an empty role list resolves nothing rather than crashing") {
    // LIVE. The state every match starts in until roles are handed out, and
    // the state a match stays in if roles/ is missing entirely.
    CHECK(findRole({}, "anything") == nullptr);
}

// ---- rendering and placement (step 2) -------------------------------------

TEST_CASE("a persona with no role renders byte-identically to today" *
          doctest::skip()) {
    // TODO(role step 2): THE REGRESSION GUARD. Every existing persona is
    // role-less, so this is what says the feature is invisible until used.
    // Compare renderSystemPrompt output with and without the role parameter,
    // as exact strings — not a substring check.
}

TEST_CASE("the role block renders after trait reinforcement and before ACTIONS" *
          doctest::skip()) {
    // TODO(role step 2): placement is the whole design and it is MEASURED, not
    // argued — with the block before "Stay in character", the killer emitted
    // [[ACTION: call_police]] on three of five turns.
    //
    // Assert on find() offsets: reinforcement < role block < "ACTIONS: ".
    // Ordering, not presence, is the contract.
}

TEST_CASE("every existing anti-drift assertion still passes with a role" *
          doctest::skip()) {
    // TODO(role step 2): the existing order — identity < trait rules+examples
    // < memory < trait reinforcement < action protocol — is a tested contract
    // in test_traits.cpp. Adding a role EXTENDS it with one more position and
    // relaxes nothing.
}

TEST_CASE("a long memory cannot push the role block off the end" *
          doctest::skip()) {
    // TODO(role step 2): the role sits after memory precisely so a long
    // remembered history cannot dilute it. Render with a very long memory
    // summary and check the ordering above still holds.
}

TEST_CASE("the role id and the raw secret never appear verbatim" *
          doctest::skip()) {
    // TODO(role step 2): an acceptance criterion, and the one that keeps a
    // debug string from becoming a tell. The rendered prompt may describe the
    // secret in prose; it must not contain the role ID as a bare token the
    // model could echo back.
}

TEST_CASE("an empty secret still renders a usable role block" *
          doctest::skip()) {
    // TODO(role step 2): bystander has no secret. The block must still carry
    // the demeanour, or the honest baseline is the one character with no
    // instruction to stay warm.
}
