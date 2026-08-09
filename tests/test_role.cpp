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

// These three are used only by cases that are still skipped. The attribute
// goes away with the last skip; the alternative — calling them from a live
// case — would mean asserting against a stub and passing for the wrong reason.
[[maybe_unused]] std::filesystem::path tempDir(const std::string& tag) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("llm_npc_role_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);
    return dir;
}

[[maybe_unused]] void writeFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path);
    out << text;
}

// A complete, valid role file — used where the case is about something else.
[[maybe_unused]] const char* const kKillerRole = R"(name = Killer
directive = You did this, and you will not admit it, ever.
directive = Give your account calmly and steer the talk elsewhere.
demeanour = You are exactly as warm as you have always been. You are not nervous, not hostile, and not defensive.
example_user = Where were you that night?
example_npc = Home, same as any night. Why, what's happened?
)";

}  // namespace

// ---- parsing (step 1) -----------------------------------------------------

TEST_CASE("a well-formed role file parses into every field" * doctest::skip()) {
    // TODO(role step 1): id from the filename stem, name, both directives in
    // file order, the demeanour, and one example pair.
}

TEST_CASE("a role file with no demeanour fails to load" * doctest::skip()) {
    // TODO(role step 1): THE ONE RULE THAT IS NOT IN Trait.cpp, and the reason
    // it exists is measured: without an explicit instruction to hold baseline
    // warmth the model defaults to hostile and the mood tag identifies the
    // killer.
    //
    // Must be a named error, and an EMPTY demeanour must fail too — a blank
    // value is the likeliest way an author satisfies the key without
    // satisfying the requirement.
}

TEST_CASE("an orphan example line fails to load" * doctest::skip()) {
    // TODO(role step 1): example_user with no following example_npc, and
    // example_npc with no preceding example_user. Same contract Trait.cpp
    // holds — an orphan is an authoring mistake worth failing loudly over.
}

TEST_CASE("an unknown key is a named error" * doctest::skip()) {
    // TODO(role step 1): mirrors parseTraitText. A typo'd key must not be
    // silently dropped.
}

TEST_CASE("one malformed role does not take the others with it" *
          doctest::skip()) {
    // TODO(role step 1): the loadAllTraits/loadAllPersonas contract. A broken
    // file is skipped with a named error and the rest still load.
}

TEST_CASE("roles load in a reproducible order" * doctest::skip()) {
    // TODO(role step 1): directory_iterator order is unspecified; sort by
    // filename so error output does not vary by machine.
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
