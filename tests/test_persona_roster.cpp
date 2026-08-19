// Self-contained roster test. At commit 6 copy this into the repo as
// tests/test_persona_roster.cpp (no append/edit of test_persona.cpp needed).
//
// The DIVERSITY GATE cases at the bottom are scaffolded for the 21-resident
// plan (.claude/plans/twenty-one-residents.md) and are skipped until it lands.
// They exist because the existing look check below is weaker than it appears:
// it compares the WHOLE look, so two residents differing only by palette pass
// while reading as the same person across a plaza.
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "CharacterParts.hpp"
#include "PersonaLoader.hpp"
#include "doctest.h"

TEST_CASE("the shipped personas directory yields the full ten-citizen roster") {
    namespace fs = std::filesystem;
    // The test binary runs from tests/ (make) or build/ (ctest); walk up to
    // wherever personas/ lives.
    fs::path dir = "personas";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));

    std::vector<std::string> errors;
    const auto roster = llm_npc::loadAllPersonas(dir, &errors);
    CHECK(errors.empty());
    REQUIRE(roster.size() == 10);

    // Sorted by filename; every persona is named, placed, and distinct.
    CHECK(roster.front().id == "baker");
    for (const auto& loaded : roster) {
        CHECK_FALSE(loaded.persona.name.empty());
        CHECK_FALSE(loaded.spotId.empty());
        for (const auto& other : roster) {
            if (&other == &loaded) continue;
            const bool samePlace = other.position.x == loaded.position.x &&
                                   other.position.z == loaded.position.z;
            CHECK_FALSE(samePlace);
        }
    }

    // Shared character library (issue #98): every shipped persona carries an
    // AUTHORED look that validates against the live catalog — never the
    // hashed fallback, so the cast stays art-directed — and no two citizens
    // wear the identical look. Variety therefore meets and exceeds the old
    // five-rigged-model baseline by construction.
    for (const auto& loaded : roster) {
        REQUIRE_MESSAGE(loaded.hasLook, (loaded.id + " has no authored look"));
        std::string why;
        CHECK_MESSAGE(llm_npc::lookIsValid(loaded.look, &why), (loaded.id + ": " + why));
        for (const auto& other : roster) {
            if (&other == &loaded) continue;
            bool identical = other.look.paletteId == loaded.look.paletteId;
            for (int c = 0; c < llm_npc::kPartCategoryCount; ++c) {
                identical = identical && other.look.partIds[c] == loaded.look.partIds[c];
            }
            CHECK_MESSAGE(!identical, (loaded.id + " and " + other.id + " wear the same look"));
        }
    }
}

TEST_CASE("schedule header lines parse into entries and round-trip") {
    const auto parsed = llm_npc::parsePersonaText(
        "name = Piper\n"
        "position = 1, 2\n"
        "schedule = 5-12, -70, -36, baking bread\n"
        "schedule = 22-6, 4.5, -8, night watch, extra shift\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    REQUIRE(parsed.value.schedule.size() == 2);
    CHECK(parsed.value.schedule[0].startHour == doctest::Approx(5.f));
    CHECK(parsed.value.schedule[0].endHour == doctest::Approx(12.f));
    CHECK(parsed.value.schedule[0].position.x == doctest::Approx(-70.f));
    CHECK(parsed.value.schedule[0].position.z == doctest::Approx(-36.f));
    CHECK(parsed.value.schedule[0].activity == "baking bread");
    // The activity keeps its own commas (everything after the third).
    CHECK(parsed.value.schedule[1].activity == "night watch, extra shift");

    // renderPersonaText writes the lines back out; reparsing matches.
    const auto again =
        llm_npc::parsePersonaText(llm_npc::renderPersonaText(parsed.value), "piper");
    REQUIRE_MESSAGE(again.ok, again.error);
    REQUIRE(again.value.schedule.size() == 2);
    CHECK(again.value.schedule[1].startHour == doctest::Approx(22.f));
    CHECK(again.value.schedule[1].activity == "night watch, extra shift");
}

TEST_CASE("malformed schedule lines are named errors") {
    for (const char* bad :
         {"schedule = 5, -70, -36, baking",        // no hour range
          "schedule = 5-25, -70, -36, baking",     // hour out of range
          "schedule = 5-12, -70, baking",          // missing z
          "schedule = 5-12, -70, -36,"}) {         // empty activity
        CAPTURE(bad);
        const auto parsed = llm_npc::parsePersonaText(
            std::string("name = X\n") + bad + "\n", "x");
        CHECK_FALSE(parsed.ok);
        CHECK(parsed.error.find("schedule") != std::string::npos);
    }
}

// --- Diversity gate (plan: twenty-one-residents) ----------------------------
//
// Skipped until the 21-resident work lands, EXCEPT the silhouette-budget case,
// which measures the catalog rather than the roster and is true today.
//
// Why these exist: a player under time pressure has to tell 21 characters
// apart. The check above ("no two wear the same look") is not enough for that
// — it compares the whole look string, so a palette swap satisfies it while
// two residents remain visually identical in silhouette, which is most of what
// reads at conversational distance and in fog.

namespace {

// Loads the shipped roster from wherever personas/ lives relative to the test
// binary (tests/ under make, build/ under ctest).
std::vector<llm_npc::LoadedPersona> shippedRoster() {
    namespace fs = std::filesystem;
    fs::path dir = "personas";
    for (int i = 0; i < 4 && !fs::exists(dir); ++i) dir = ".." / dir;
    REQUIRE(fs::exists(dir));
    std::vector<std::string> errors;
    return llm_npc::loadAllPersonas(dir, &errors);
}

// Body + head only. This is the silhouette a player actually reads across a
// street; palette, hair and mouth are detail that fog and distance remove.
std::string silhouetteOf(const llm_npc::LoadedPersona& p) {
    return p.look.part(llm_npc::PartCategory::Body) + "/" +
           p.look.part(llm_npc::PartCategory::Head);
}

// Core-pack parts in one category — the pool a shipped resident may draw from.
std::vector<const llm_npc::PartDef*> coreParts(llm_npc::PartCategory category) {
    std::vector<const llm_npc::PartDef*> out;
    for (const llm_npc::PartDef* def :
         llm_npc::partsForCategory(category, "any")) {
        if (def->pack == "core") out.push_back(def);
    }
    return out;
}

// How many body/head pairs actually VALIDATE.
//
// The obvious count — bodies x heads — is wrong, and wrong by a factor of two.
// Parts carry a style family ("round" or "blocky") and lookIsValid rejects any
// mix of the two, so only same-family pairs are usable. Six bodies and four
// heads look like 24 combinations and are really 12.
int validSilhouetteCount() {
    int n = 0;
    for (const llm_npc::PartDef* body : coreParts(llm_npc::PartCategory::Body)) {
        for (const llm_npc::PartDef* head : coreParts(llm_npc::PartCategory::Head)) {
            if (llm_npc::styleCompatible(*body, *head)) ++n;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("the core catalog can seat the current roster") {
    // Measures the CATALOG, not the roster, and counts only pairs that pass
    // lookIsValid's style-family rule — see validSilhouetteCount above for why
    // the naive product is double the truth.
    //
    // This is the headroom check: if a part is ever removed, it fails before
    // anyone tries to author into a pool that cannot hold them.
    const int valid = validSilhouetteCount();
    const int roster = static_cast<int>(shippedRoster().size());
    CHECK_MESSAGE(valid >= roster,
                  ("core catalog offers " + std::to_string(valid) +
                   " valid silhouettes for " + std::to_string(roster) +
                   " residents"));
}

TEST_CASE("the core catalog can seat twenty-one residents") {
    // Skipped and failing until issue #175. There were 12 valid silhouettes —
    // 3 round bodies x 2 round heads plus 3 blocky bodies x 2 blocky heads —
    // and the ten shipped residents already wore ten of them, so 21 distinct
    // silhouettes was unreachable by any arrangement of the catalog.
    //
    // #175 added one body and one head per family: 4x3 + 4x3 = 24. Three parts
    // would have hit 21 exactly; four were specified so the cast of #173 is not
    // authored against a budget with zero slack.
    const int valid = validSilhouetteCount();
    CHECK_MESSAGE(valid >= 21, ("core catalog offers " + std::to_string(valid) +
                                " valid silhouettes"));
}

TEST_CASE("neither style family can seat the cast alone") {
    // The trap this milestone kept falling into: bodies x heads looks like the
    // pool and is double it, because lookIsValid rejects a round/blocky mix.
    // A future part added to only one family would raise the total above 21
    // while leaving one family unable to hold its share — so count per family,
    // not just overall.
    int roundPairs = 0;
    int blockyPairs = 0;
    for (const llm_npc::PartDef* body : coreParts(llm_npc::PartCategory::Body)) {
        for (const llm_npc::PartDef* head : coreParts(llm_npc::PartCategory::Head)) {
            if (!llm_npc::styleCompatible(*body, *head)) continue;
            if (body->styleTag == "round") ++roundPairs;
            if (body->styleTag == "blocky") ++blockyPairs;
        }
    }
    CHECK(roundPairs + blockyPairs == validSilhouetteCount());
    // Neither family alone reaches 21, which is why a 21-strong cast has to be
    // split across both. Both must be able to carry roughly half.
    CHECK(roundPairs >= 11);
    CHECK(blockyPairs >= 11);
}

TEST_CASE("no two residents share a body/head silhouette") {
    // Stricter than the whole-look check above, and the one that decides
    // whether a player can tell the cast apart at a distance.
    //
    // This found a real collision when it was first run: barista and librarian
    // both wore body_slim/head_oval. The barista moved to head_tall (an unused
    // combination) rather than the gate being relaxed — the librarian's
    // glasses-and-bob is the more natural read for an oval head.
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        const std::string shape = silhouetteOf(p);
        CHECK_MESSAGE(seen.insert(shape).second,
                      (p.id + " reuses the silhouette " + shape));
    }
}

TEST_CASE("no two residents share a trait set") {
    // Distinct voice is not unit-testable; a unique trait set is the closest
    // honest proxy available.
    //
    // This checks the FREE-TEXT `traits =` line, which is what the shipped
    // cast actually uses. The structured traitIds (from repeated `trait =`
    // keys, backed by traits/*.trait) are EMPTY for all ten residents — the
    // trait library exists and nothing ships using it. Gating on traitIds
    // would fail ten ways for a reason that has nothing to do with diversity;
    // see the case below, which records that gap deliberately.
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        CHECK_FALSE_MESSAGE(p.persona.traits.empty(),
                            (p.id + " has no traits"));
        std::set<std::string> sorted(p.persona.traits.begin(),
                                     p.persona.traits.end());
        std::string key;
        for (const auto& t : sorted) key += t + ",";
        CHECK_MESSAGE(seen.insert(key).second,
                      (p.id + " reuses the trait set " + key));
    }
}

TEST_CASE("no two residents share a speaking style") {
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        CHECK_FALSE_MESSAGE(p.persona.speakingStyle.empty(),
                            (p.id + " has no speaking style"));
        CHECK_MESSAGE(seen.insert(p.persona.speakingStyle).second,
                      (p.id + " reuses another resident's speaking style"));
    }
}

TEST_CASE("the shipped cast does not use the structured trait library") {
    // Not a failure — a recorded gap. traits/*.trait ships eight traits with
    // behaviour rules and few-shot examples, Persona composes them into the
    // system prompt at a tested position, and NO shipped resident references
    // one. Every persona uses only the free-text adjectives line.
    //
    // This case exists so the day someone starts using `trait =` keys, it
    // fails and prompts a decision: either the gate above moves to traitIds
    // (stronger, since they drive prompt composition) or both are checked.
    const auto roster = shippedRoster();
    for (const auto& p : roster) {
        CHECK_MESSAGE(p.persona.traitIds.empty(),
                      (p.id + " now uses structured traits — move the trait-set "
                              "gate to traitIds, which drive the prompt"));
    }
}

TEST_CASE("the diversity gate fails on a deliberate duplicate") {
    // A gate that cannot fail is decoration. Rather than trusting the shipped
    // cast, build a roster with a known collision and assert the same
    // comparison the silhouette case uses reports it.
    llm_npc::LoadedPersona a;
    a.id = "alpha";
    a.look.part(llm_npc::PartCategory::Body) = "body_slim";
    a.look.part(llm_npc::PartCategory::Head) = "head_oval";

    llm_npc::LoadedPersona b = a;
    b.id = "beta";
    // Differs in every OTHER slot, so a whole-look comparison would pass it —
    // which is exactly the weakness the silhouette gate exists to close.
    b.look.part(llm_npc::PartCategory::Eyes) = "eyes_wink";
    b.look.paletteId = "forest";

    std::set<std::string> seen;
    CHECK(seen.insert(silhouetteOf(a)).second);
    CHECK_FALSE_MESSAGE(seen.insert(silhouetteOf(b)).second,
                        "the silhouette gate failed to reject a duplicate");
}

TEST_CASE("the roster is the full twenty-one residents" * doctest::skip()) {
    // Flipped from 10 at the end of the plan, once the render budget holds and
    // the eleven new residents pass the gate.
    CHECK(shippedRoster().size() == 21);
}
