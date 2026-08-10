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

namespace {

// A persona with a trait, so the assembly order has every position filled.
Persona rolePersona(TraitDef& grumpy) {
    grumpy.id = "grumpy";
    grumpy.name = "Grumpy";
    grumpy.behaviorRules = {"Complain briefly before helping."};
    grumpy.examples = {{"Good morning!", "It's a day. What do you want?"}};

    Persona p;
    p.name = "Marge Holloway";
    p.role = "bakery owner";
    p.traitIds = {"grumpy"};
    return p;
}

}  // namespace

TEST_CASE("a persona with no role renders byte-identically to today") {
    // THE REGRESSION GUARD. Every existing persona is role-less, so this is
    // what says the feature is invisible until it is used. Exact strings, not
    // a substring check.
    TraitDef grumpy;
    const Persona p = rolePersona(grumpy);
    const std::vector<const TraitDef*> traits{&grumpy};

    CHECK(p.renderSystemPrompt("mem", "gos", traits) ==
          p.renderSystemPrompt("mem", "gos", traits, ""));
    CHECK(p.renderSystemPrompt("", "", traits) ==
          p.renderSystemPrompt("", "", traits, ""));
    // And with no traits either — the wholly bare path.
    CHECK(p.renderSystemPrompt("", "", {}) ==
          p.renderSystemPrompt("", "", {}, ""));
}

TEST_CASE("the role block renders after trait reinforcement and before ACTIONS") {
    // Placement is the whole design and it is MEASURED: with the block placed
    // earlier, a killer emitted [[ACTION: call_police]] on three of five turns.
    // Ordering, not presence, is the contract.
    TraitDef grumpy;
    const Persona p = rolePersona(grumpy);
    const std::vector<const TraitDef*> traits{&grumpy};

    const auto dir = tempDir("placement");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE(roles.size() == 1);
    const std::string block = renderRoleBlock(&roles[0], "You were at the mill.");

    const std::string prompt = p.renderSystemPrompt("mem", "gos", traits, block);

    const auto rules = prompt.find("Complain briefly before helping.");
    const auto memory = prompt.find("What you remember from earlier meetings");
    const auto reinforcement = prompt.find("No matter what the conversation");
    const auto role = prompt.find("In this story you are Killer");
    const auto actions = prompt.find("ACTIONS: ");

    REQUIRE(rules != std::string::npos);
    REQUIRE(memory != std::string::npos);
    REQUIRE(reinforcement != std::string::npos);
    REQUIRE(role != std::string::npos);
    REQUIRE(actions != std::string::npos);

    CHECK(rules < memory);
    CHECK(memory < reinforcement);
    CHECK(reinforcement < role);   // the new position
    CHECK(role < actions);         // and it never displaces the protocol
}

TEST_CASE("every existing anti-drift assertion still passes with a role") {
    // test_traits.cpp pins identity < rules < examples < memory <
    // reinforcement < ACTIONS. Adding a role EXTENDS that with one more
    // position and relaxes nothing — this re-asserts the whole original chain
    // with a role block present.
    TraitDef grumpy;
    const Persona p = rolePersona(grumpy);
    const std::vector<const TraitDef*> traits{&grumpy};

    const auto dir = tempDir("antidrift");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    const std::string prompt =
        p.renderSystemPrompt("mem", "gos", traits, renderRoleBlock(&roles[0], "s"));

    const auto identity = prompt.find("You are Marge Holloway");
    const auto rules = prompt.find("Complain briefly before helping.");
    const auto examples = prompt.find("It's a day. What do you want?");
    const auto memory = prompt.find("What you remember from earlier meetings");
    const auto reinforcement = prompt.find("No matter what the conversation");
    const auto actions = prompt.find("ACTIONS: ");

    CHECK(identity < rules);
    CHECK(rules < examples);
    CHECK(examples < memory);
    CHECK(memory < reinforcement);
    CHECK(reinforcement < actions);
}

TEST_CASE("a long memory cannot push the role block off the end") {
    // The role sits after memory precisely so a long remembered history cannot
    // dilute it. This is the same failure mode trait reinforcement was placed
    // to avoid.
    TraitDef grumpy;
    const Persona p = rolePersona(grumpy);
    const std::vector<const TraitDef*> traits{&grumpy};

    const auto dir = tempDir("longmem");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    const std::string block = renderRoleBlock(&roles[0], "You were at the mill.");

    const std::string huge(20000, 'x');
    const std::string prompt = p.renderSystemPrompt(huge, "gos", traits, block);

    const auto memory = prompt.find(huge);
    const auto role = prompt.find("In this story you are Killer");
    const auto actions = prompt.find("ACTIONS: ");
    REQUIRE(role != std::string::npos);
    CHECK(memory < role);
    CHECK(role < actions);
}

TEST_CASE("the role id never reaches the rendered prompt") {
    // An acceptance criterion, and the one that keeps a debug string from
    // becoming a tell. The prompt carries the secret as prose the NPC believes;
    // it must not carry "killer" as a bare key the model could echo back.
    TraitDef grumpy;
    const Persona p = rolePersona(grumpy);
    const std::vector<const TraitDef*> traits{&grumpy};

    const auto dir = tempDir("noidprompt");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    const std::string prompt = p.renderSystemPrompt(
        "mem", "gos", traits, renderRoleBlock(&roles[0], "You were at the mill."));

    CHECK(prompt.find("killer") == std::string::npos);
}

TEST_CASE("a role with no traits still lands before ACTIONS") {
    // A cast NPC with no structured traits is a real case — most personas
    // carry free-text traits only. The insertion point must not depend on
    // renderTraitBlock having produced anything.
    Persona p;
    p.name = "Gus Pike";
    p.role = "hot-dog vendor";

    const auto dir = tempDir("notraits");
    writeFile(dir / "killer.role", kKillerRole);
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    const std::string prompt =
        p.renderSystemPrompt("", "", {}, renderRoleBlock(&roles[0], ""));

    const auto role = prompt.find("In this story you are Killer");
    const auto actions = prompt.find("ACTIONS: ");
    REQUIRE(role != std::string::npos);
    CHECK(role < actions);
}

// ---- the shipped role files (issue #198) ---------------------------------

TEST_CASE("the shipped roles directory loads with no errors") {
    namespace fs = std::filesystem;
    fs::path dir = "roles";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    std::vector<std::string> errors;
    const std::vector<RoleDef> roles = loadAllRoles(dir, &errors);

    for (const std::string& error : errors) CAPTURE(error);
    CHECK(errors.empty());
    REQUIRE(roles.size() == 4);

    // The four the plan names, in sorted order.
    CHECK(roles[0].id == "bystander");
    CHECK(roles[1].id == "killer");
    CHECK(roles[2].id == "secret_keeper");
    CHECK(roles[3].id == "witness");
}

TEST_CASE("every shipped role carries a real demeanour and examples") {
    namespace fs = std::filesystem;
    fs::path dir = "roles";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    REQUIRE_FALSE(roles.empty());

    for (const RoleDef& role : roles) {
        CAPTURE(role.id);
        // The loader already rejects a missing or empty demeanour. This asserts
        // it is a real instruction rather than a token satisfying the parser —
        // a one-word demeanour would load and would not hold the model.
        CHECK(role.demeanour.size() > 40);
        CHECK_FALSE(role.directives.empty());
        CHECK_FALSE(role.examples.empty());
    }
}

TEST_CASE("bystander has a demeanour too") {
    // The honest baseline is the easiest one to leave without an anti-tell
    // line, and doing so would make it the ONLY role whose warmth is not
    // pinned — turning the innocent default into the odd one out.
    namespace fs = std::filesystem;
    fs::path dir = "roles";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    // The vector must be NAMED. findRole returns a pointer INTO it, so
    // findRole(loadAllRoles(dir), ...) dangles the moment the statement ends —
    // which is exactly how this case failed the first time it ran.
    const std::vector<RoleDef> roles = loadAllRoles(dir);
    const RoleDef* bystander = findRole(roles, "bystander");
    REQUIRE(bystander != nullptr);
    CHECK(bystander->demeanour.find("exactly as warm") != std::string::npos);
}

TEST_CASE("every role's demeanour holds baseline warmth explicitly") {
    // Measurement 3 is that the model defaults to hostile under a role. Each
    // demeanour must say so in as many words; a role whose demeanour describes
    // a mood WITHOUT pinning warmth is the failure mode this whole field
    // exists to prevent.
    namespace fs = std::filesystem;
    fs::path dir = "roles";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;

    for (const RoleDef& role : loadAllRoles(dir)) {
        CAPTURE(role.id);
        CHECK(role.demeanour.find("exactly as warm as you have always been") !=
              std::string::npos);
    }
}

// ---- carrying a role on an NPC (issue #199) --------------------------------

#include "FakeOllama.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"

using llm_npc_test::FakeOllama;

namespace {

// The shipped roles, held in a NAMED vector. findRole returns a pointer into
// it, so `findRole(loadAllRoles(dir), ...)` dangles the moment the statement
// ends — which is how a test in this file failed the first time it ran.
std::vector<RoleDef> shippedRoles() {
    namespace fs = std::filesystem;
    fs::path dir = "roles";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));
    return loadAllRoles(dir, nullptr);
}

Persona rolePersona() {
    Persona p;
    p.name = "Marge Holloway";
    p.role = "baker";
    p.knowledgeBoundary = "Knows the bakery and its regulars.";
    return p;
}

}  // namespace

TEST_CASE("an NPC with no role renders exactly the prompt it did before") {
    // The regression guard for every NPC in the game that is not in a mystery.
    const Persona persona = rolePersona();
    const std::string before =
        persona.renderSystemPrompt("", "", std::vector<const TraitDef*>{});
    const std::string after = persona.renderSystemPrompt(
        "", "", std::vector<const TraitDef*>{}, renderRoleBlock(nullptr, ""));
    CHECK(before == after);
}

TEST_CASE("an assigned role reaches the prompt, after traits and before ACTIONS") {
    const std::vector<RoleDef> roles = shippedRoles();
    const RoleDef* killer = findRole(roles, "killer");
    REQUIRE(killer != nullptr);

    const Persona persona = rolePersona();
    const std::string prompt = persona.renderSystemPrompt(
        "", "", std::vector<const TraitDef*>{},
        renderRoleBlock(killer, "You were at the bakery, not the coffee house."));

    REQUIRE_FALSE(killer->directives.empty());
    const auto rolePos = prompt.find(killer->directives.front());
    const auto actionsPos = prompt.find("ACTIONS: ");
    REQUIRE(rolePos != std::string::npos);
    REQUIRE(actionsPos != std::string::npos);
    CHECK(rolePos < actionsPos);  // the ACTIONS contract is not relaxed
    CHECK(prompt.find("You were at the bakery, not the coffee house.") !=
          std::string::npos);
}

TEST_CASE("an unknown role id is demoted, not substituted") {
    // Substituting for an unknown killer role would produce a match with two
    // killers or none, and nothing downstream would notice.
    const std::vector<RoleDef> roles = shippedRoles();
    CHECK(findRole(roles, "arsonist") == nullptr);

    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(rolePersona(), client);
    npc.setRoleRegistry(&roles);
    npc.setRole("arsonist", "I did not do it.");

    CHECK(npc.resolvedRole() == nullptr);
    CHECK(npc.secret() == "I did not do it.");  // carried, not lost

    // And the rendered block is EMPTY — the secret does not reach the prompt
    // on its own. That is the safe demotion, not an oversight: the directives
    // are what tell the model to protect a secret, so a secret without them is
    // one it will happily volunteer the first time anybody asks. Losing the
    // whole block costs a role; keeping just the secret would hand the answer
    // out.
    const std::string block = renderRoleBlock(npc.resolvedRole(), npc.secret());
    CHECK(block.empty());
}

TEST_CASE("an NPC with no registry resolves to no role rather than crashing") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(rolePersona(), client);
    npc.setRole("killer", "a secret");  // registry never installed
    CHECK(npc.resolvedRole() == nullptr);
}

TEST_CASE("a role assigned to an NPC resolves through the registry") {
    const std::vector<RoleDef> roles = shippedRoles();
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(rolePersona(), client);
    npc.setRoleRegistry(&roles);
    npc.setRole("witness", "I saw someone by the alley door.");

    const RoleDef* resolved = npc.resolvedRole();
    REQUIRE(resolved != nullptr);
    CHECK(resolved->id == "witness");
    CHECK(npc.roleId() == "witness");
}

TEST_CASE("the secret is never in the NPC's visible state") {
    // Roles are never shown to a player. The secret lives in the prompt and
    // in nothing a UI reads — history, gossip and memory must all stay clean.
    const std::vector<RoleDef> roles = shippedRoles();
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(rolePersona(), client);
    npc.setRoleRegistry(&roles);
    npc.setRole("killer", "You were at the bakery, not the coffee house.");

    CHECK(npc.history().empty());
    CHECK(npc.gossip().empty());
    CHECK(npc.memory().empty());
    // The persona a UI would render carries nothing about it either.
    CHECK(npc.persona().knowledgeBoundary.find("bakery, not the coffee") ==
          std::string::npos);
}
