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

// Counts core-pack parts in one category — the pool a shipped resident may
// draw from.
int coreCount(llm_npc::PartCategory category) {
    int n = 0;
    for (const llm_npc::PartDef* def :
         llm_npc::partsForCategory(category, "any")) {
        if (def->pack == "core") ++n;
    }
    return n;
}

}  // namespace

TEST_CASE("the core catalog has enough silhouettes for the planned cast") {
    // NOT skipped: this measures the CATALOG, not the roster, so it is
    // answerable today and it is the constraint that is invisible until
    // counted. Six core bodies x four core heads = 24 combinations for 21
    // residents — three spare. If a part is ever removed, this fails before
    // anyone tries to author into a pool that cannot hold them.
    constexpr int kPlannedCast = 21;
    const int bodies = coreCount(llm_npc::PartCategory::Body);
    const int heads = coreCount(llm_npc::PartCategory::Head);
    CHECK_MESSAGE(bodies * heads >= kPlannedCast,
                  ("core catalog offers only " + std::to_string(bodies * heads) +
                   " body/head silhouettes for " + std::to_string(kPlannedCast) +
                   " residents"));
}

TEST_CASE("no two residents share a body/head silhouette" * doctest::skip()) {
    // Stricter than the whole-look check above, and the one that decides
    // whether a player can tell the cast apart at a distance.
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        const std::string shape = silhouetteOf(p);
        CHECK_MESSAGE(seen.insert(shape).second,
                      (p.id + " reuses the silhouette " + shape));
    }
}

TEST_CASE("no two residents share a trait set" * doctest::skip()) {
    // Distinct voice is not unit-testable; a unique trait set is the closest
    // honest proxy, and the trait library is large enough to allow it
    // (8 traits taken one or two at a time is 36 combinations).
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        std::set<std::string> ids(p.persona.traitIds.begin(),
                                  p.persona.traitIds.end());
        std::string key;
        for (const auto& id : ids) key += id + ",";
        CHECK_MESSAGE(seen.insert(key).second,
                      (p.id + " reuses the trait set " + key));
    }
}

TEST_CASE("no two residents share a speaking style" * doctest::skip()) {
    const auto roster = shippedRoster();
    std::set<std::string> seen;
    for (const auto& p : roster) {
        CHECK_FALSE_MESSAGE(p.persona.speakingStyle.empty(),
                            (p.id + " has no speaking style"));
        CHECK_MESSAGE(seen.insert(p.persona.speakingStyle).second,
                      (p.id + " reuses another resident's speaking style"));
    }
}

TEST_CASE("the diversity gate fails on a deliberate duplicate" * doctest::skip()) {
    // A gate that cannot fail is decoration. This proves the silhouette check
    // above actually rejects something, by building a roster with a known
    // collision rather than trusting the shipped cast to be correct.
    //
    // TODO(residents): build two LoadedPersona values sharing a body/head pair
    // and assert the same comparison the case above uses reports a collision.
}

TEST_CASE("the roster is the full twenty-one residents" * doctest::skip()) {
    // Flipped from 10 at the end of the plan, once the render budget holds and
    // the eleven new residents pass the gate.
    CHECK(shippedRoster().size() == 21);
}
