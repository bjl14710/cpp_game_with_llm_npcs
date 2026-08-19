#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace llm_npc {

// One personality trait as DATA (plan: npc-traits-and-ratings): behavior
// rules plus curated few-shot example exchanges, loaded from
// traits/<id>.trait files (see traits/grumpy.trait for the format). Traits
// are prompt-conditioning only — they never drive movement or schedules.
struct TraitExchange {
    std::string userLine;  // what was said to the NPC
    std::string npcLine;   // the curated in-trait reply
};

struct TraitDef {
    std::string id;    // filename stem, e.g. "grumpy"
    std::string name;  // display name for the creator picker
    std::vector<std::string> behaviorRules;
    std::vector<TraitExchange> examples;
};

// TODO(traits step 1): parse traits/*.trait (key=value like .persona:
// `name =`, repeated `rule =`, repeated `example_user =`/`example_npc =`
// pairs in order). Malformed files are skipped with named errors, same
// contract as loadAllPersonas.
std::vector<TraitDef> loadAllTraits(const std::filesystem::path& dir,
                                    std::vector<std::string>* errors = nullptr);

// TODO(traits step 2): the two prompt blocks. renderTraitBlock = rules +
// examples, inserted AFTER identity/backstory and BEFORE memory;
// renderTraitReinforcement = a 1-2 line re-assertion inserted AFTER the
// memory summary so long-conversation drift doesn't dilute the trait
// (assembly order is a TESTED contract — see tests/test_traits.cpp).
std::string renderTraitBlock(const std::vector<const TraitDef*>& traits);
std::string renderTraitReinforcement(const std::vector<const TraitDef*>& traits);

}  // namespace llm_npc
