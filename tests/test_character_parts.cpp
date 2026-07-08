// Tests for the character-creator part system: socket assembly, style
// compatibility, randomize determinism, and look serialization.
#include <set>
#include <string>

#include "CharacterParts.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A known-good round-family look used as the baseline in several cases.
CharacterLook roundLook() {
    CharacterLook look;
    look.part(PartCategory::Body) = "body_round";
    look.part(PartCategory::Head) = "head_round";
    look.part(PartCategory::Eyes) = "eyes_wide";
    look.part(PartCategory::Hair) = "hair_tuft";
    look.paletteId = "warm";
    return look;
}

}  // namespace

TEST_CASE("catalog covers every category and category specs are orderable") {
    for (int c = 0; c < kPartCategoryCount; ++c) {
        const auto options =
            partsForCategory(static_cast<PartCategory>(c), "any");
        CAPTURE(c);
        CHECK(options.size() >= 2);  // a picker with one option isn't a picker
    }
    // Specs list parents before children (assembly relies on it).
    std::set<int> seen;
    for (const CategorySpec& spec : categorySpecs()) {
        if (spec.socket[0] != '\0') {
            CHECK(seen.count(static_cast<int>(spec.parent)) == 1);
        }
        seen.insert(static_cast<int>(spec.category));
    }
    CHECK(seen.size() == kPartCategoryCount);
}

TEST_CASE("every style-consistent combination assembles completely") {
    // Exhaustive: bodies x heads x eyes x hair, filtered by pairwise
    // compatibility — each valid combo must place all categories with
    // positive height. This is the "any combination renders correctly"
    // acceptance criterion at the data level.
    int validCombos = 0;
    for (const PartDef* body : partsForCategory(PartCategory::Body, "any")) {
        for (const PartDef* head : partsForCategory(PartCategory::Head, "any")) {
            for (const PartDef* eyes : partsForCategory(PartCategory::Eyes, "any")) {
                for (const PartDef* hair : partsForCategory(PartCategory::Hair, "any")) {
                    CharacterLook look;
                    look.part(PartCategory::Body) = body->id;
                    look.part(PartCategory::Head) = head->id;
                    look.part(PartCategory::Eyes) = eyes->id;
                    look.part(PartCategory::Hair) = hair->id;
                    look.paletteId = "warm";
                    const bool compatible =
                        styleCompatible(*body, *head) &&
                        styleCompatible(*body, *eyes) &&
                        styleCompatible(*body, *hair) &&
                        styleCompatible(*head, *eyes) &&
                        styleCompatible(*head, *hair) &&
                        styleCompatible(*eyes, *hair);
                    const AssembledLook assembled = assembleLook(look);
                    CAPTURE(body->id + "+" + head->id + "+" + eyes->id +
                            "+" + hair->id);
                    CHECK(assembled.ok == compatible);
                    if (!compatible) continue;
                    ++validCombos;
                    CHECK(assembled.parts.size() == kPartCategoryCount);
                    CHECK(assembled.height > 0.f);
                    // The head must sit above the body's midpoint — a cheap
                    // sanity check that sockets actually stacked parts.
                    CHECK(assembled.parts[1].position.y >
                          assembled.parts[0].part->localSize.y * 0.5f);
                }
            }
        }
    }
    CHECK(validCombos > 0);
}

TEST_CASE("style gate rejects cross-family looks with a reason") {
    CharacterLook look = roundLook();
    look.part(PartCategory::Eyes) = "eyes_visor";  // blocky into round
    std::string why;
    CHECK_FALSE(lookIsValid(look, &why));
    CHECK(why.find("eyes_visor") != std::string::npos);
    CHECK_FALSE(assembleLook(look).ok);
}

TEST_CASE("unknown part and unknown palette are rejected") {
    CharacterLook look = roundLook();
    look.part(PartCategory::Hair) = "hair_mohawk";  // not in the catalog
    CHECK_FALSE(lookIsValid(look));
    CHECK(findPart("hair_mohawk") == nullptr);

    look = roundLook();
    look.paletteId = "neon";
    CHECK_FALSE(lookIsValid(look));
}

TEST_CASE("randomizeLook is deterministic and always valid") {
    for (unsigned seed = 0; seed < 50; ++seed) {
        const CharacterLook a = randomizeLook(seed);
        const CharacterLook b = randomizeLook(seed);
        CAPTURE(seed);
        for (int c = 0; c < kPartCategoryCount; ++c) {
            CHECK(a.partIds[c] == b.partIds[c]);
        }
        CHECK(a.paletteId == b.paletteId);
        std::string why;
        CHECK_MESSAGE(lookIsValid(a, &why), why);
        CHECK(assembleLook(a).ok);
    }
}

TEST_CASE("look serialization round-trips and rejects garbage") {
    const CharacterLook look = roundLook();
    CharacterLook back;
    REQUIRE(CharacterLook::fromJson(look.toJson(), back));
    for (int c = 0; c < kPartCategoryCount; ++c) {
        CHECK(back.partIds[c] == look.partIds[c]);
    }
    CHECK(back.paletteId == look.paletteId);

    CharacterLook junk;
    CHECK_FALSE(CharacterLook::fromJson("not json", junk));
    CHECK_FALSE(CharacterLook::fromJson("{\"body\": 3}", junk));
    CHECK_FALSE(CharacterLook::fromJson("{}", junk));
}

TEST_CASE("bald option exists and assembles flush with the head") {
    CharacterLook look = roundLook();
    look.part(PartCategory::Hair) = "hair_none";
    const AssembledLook assembled = assembleLook(look);
    REQUIRE(assembled.ok);
    // Zero-size hair must not add height beyond the head.
    const PlacedPart& head = assembled.parts[1];
    CHECK(assembled.height ==
          doctest::Approx(head.position.y + head.part->localSize.y));
}
