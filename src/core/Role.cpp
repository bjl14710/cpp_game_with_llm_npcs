#include "Role.hpp"

namespace llm_npc {

// Scaffolded stubs (plan: role-layer, steps 1-2). Loading returns nothing and
// rendering returns an empty string, so a persona with no role — which is every
// persona today — renders exactly as it does now and nothing calls this into a
// half-built state. Fill these in and un-skip tests/test_role.cpp in the same
// commit.

std::vector<RoleDef> loadAllRoles(const std::filesystem::path& dir,
                                  std::vector<std::string>* errors) {
    (void)dir;
    (void)errors;
    // TODO(role step 1): mirror Trait.cpp almost exactly. Same key=value
    // parse, same example_user/example_npc PAIR rule with an orphan on either
    // side failing loudly, same "unknown key" error, same sorted directory
    // walk so load order is reproducible.
    //
    // Keys: `name`, repeated `directive`, `demeanour`, paired
    // `example_user`/`example_npc`.
    //
    // THE ONE RULE THAT IS NOT IN Trait.cpp: a file with no `demeanour`, or an
    // empty one, must fail with a named error. It is the anti-tell line, the
    // only thing standing between this feature and the measured mood leak, and
    // an author who omits it gets a role that works in every unit test and
    // breaks the mystery in play.
    return {};
}

std::string renderRoleBlock(const RoleDef* role, const std::string& secret) {
    (void)role;
    (void)secret;
    // TODO(role step 1): assemble directives, then the demeanour line, then
    // the secret, then the examples. Shape it on renderTraitBlock —
    // "You are <name>." followed by the directives, examples rendered as
    // labelled exchanges.
    //
    // Return "" for a null role. A persona with no role must render
    // BYTE-IDENTICALLY to today, and there is a test that says so.
    //
    // Do NOT emit the role id or the raw secret as a quoted string the model
    // might echo — an acceptance criterion says neither may appear verbatim in
    // a transcript.
    return "";
}

const RoleDef* findRole(const std::vector<RoleDef>& roles, const std::string& id) {
    // Real, not a stub: a linear scan over four roles is not a search problem,
    // and having it work from the first commit means step 4 can wire an NPC's
    // assigned role without waiting on the parser.
    for (const RoleDef& role : roles) {
        if (role.id == id) return &role;
    }
    return nullptr;
}

}  // namespace llm_npc
