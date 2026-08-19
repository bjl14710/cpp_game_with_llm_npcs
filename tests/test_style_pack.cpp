// Scaffolded stubs for the mii-style-visual-overhaul plan
// (.claude/plans/mii-style-visual-overhaul.md). Each stub maps to an
// acceptance criterion and is skipped until its step lands — un-skip in
// the same commit that implements it. Rendering criteria (outlines,
// proportions on screen) are screenshot-verified, not unit-tested.
#include <set>
#include <string>

#include "CharacterParts.hpp"
#include "PersonaLoader.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("every part and palette carries a pack tag (default core)") {
    // Step 1 (pack seam). The seam now carries two packs: the built-in
    // "core" primitives and the "quaternius" mesh family (issue #139) —
    // anything else is a typo'd row.
    for (const PartDef& part : partCatalog()) {
        CHECK_MESSAGE(!part.pack.empty(), part.id);
        CHECK((part.pack == "core" || part.pack == "quaternius"));
    }
    for (const PartPalette& palette : paletteCatalog()) {
        CHECK_MESSAGE(!palette.pack.empty(), palette.id);
        CHECK(palette.pack == "core");
    }
}

TEST_CASE("cartoon proportions: heads read as ~a third of the body+head silhouette") {
    // Step 2. Metric refined from the scaffold stub (logged in
    // OVERNIGHT_REPORT.md): hair height varies per look, so the STABLE
    // measure is head.y / (headSocket.y + head.y) — the body+head
    // silhouette. The window was 0.48-0.60 while a body was one legless
    // cone; the appealing-character pass gave bodies legs, so the same
    // cartoon-large head now measures 0.32-0.39 against a full figure.
    // Retuned deliberately — the head did NOT shrink to satisfy a number.
    // Scoped to CORE parts: the quaternius mesh family (issue #139) is
    // realistically proportioned by design and pins its own window in
    // test_stylized_parts.cpp.
    for (const PartDef* body : partsForCategory(PartCategory::Body, "any")) {
        if (body->pack != "core") continue;
        const auto socket = body->sockets.find("head");
        REQUIRE_MESSAGE(socket != body->sockets.end(), body->id);
        for (const PartDef* head :
             partsForCategory(PartCategory::Head, body->styleTag)) {
            const float fraction =
                head->localSize.y / (socket->second.y + head->localSize.y);
            CAPTURE(body->id);
            CAPTURE(head->id);
            CHECK(fraction >= 0.30f);
            CHECK(fraction <= 0.40f);
        }
    }
}

TEST_CASE("Mouth is a fifth category with sockets on every head") {
    // Step 4. The combo test in test_character_parts.cpp covers the new
    // axis exhaustively; this pins the wiring itself.
    CHECK(kPartCategoryCount == 5);
    CHECK_FALSE(partsForCategory(PartCategory::Mouth, "any").empty());
    for (const PartDef* head : partsForCategory(PartCategory::Head, "any")) {
        CAPTURE(head->id);
        CHECK(head->sockets.count("mouth") == 1);
    }
    bool mouthRow = false;
    for (const CategorySpec& spec : categorySpecs()) {
        if (spec.category != PartCategory::Mouth) continue;
        mouthRow = true;
        CHECK(spec.parent == PartCategory::Head);
        CHECK(std::string(spec.socket) == "mouth");
    }
    CHECK(mouthRow);
}

TEST_CASE("a five-item look line still parses, mouth defaults to the smile") {
    // Step 4 back-compat: pre-Mouth persona files keep their authored
    // identity — the mouth slot fills with the canonical smile (a fixed,
    // documented default; simpler than a per-name hash for one slot, and
    // Tomodachi mouths are mostly smiles anyway — decision logged in
    // OVERNIGHT_REPORT.md).
    const auto parsed = llm_npc::parsePersonaText(
        "name = Piper\n"
        "look = body_round, head_round, eyes_wide, hair_tuft, warm\n",
        "piper");
    REQUIRE_MESSAGE(parsed.ok, parsed.error);
    REQUIRE(parsed.value.hasLook);
    CHECK(parsed.value.look.part(PartCategory::Mouth) == "mouth_smile");
    CHECK(parsed.value.look.paletteId == "warm");
    CHECK(lookIsValid(parsed.value.look));
    // Written back, the line is always the six-item current format.
    const std::string rendered = llm_npc::renderPersonaText(parsed.value);
    CHECK(rendered.find("hair_tuft, mouth_smile, warm") != std::string::npos);
}

TEST_CASE("round skulls wear the mouth ON the face, not out in front of it") {
    // Regression for the muzzle. A mouth socket's z is only correct for the
    // mouth LINE it was authored against: #104 placed these against a line of
    // 0.30 (round) / 0.34 (oval), the appealing-character pass lowered the
    // line to 0.27 / 0.31, and z stayed put — so the mark kept the depth of a
    // point higher up a skull that is narrower down there, and stood 0.07
    // proud of a head only 0.49 deep, hiding the nose bead behind it.
    //
    // The invariant that catches it: the front face of the drawn mark sits
    // level with the skull surface at the mouth's own height. Only the boxy
    // heads may float a mark off a flat wall and still read as painted on;
    // on a sphere that is a snout, so this is scoped to the round family.
    for (const PartDef* head : partsForCategory(PartCategory::Head, "round")) {
        if (head->pack != "core") continue;
        const auto mouth = head->sockets.find("mouth");
        REQUIRE_MESSAGE(mouth != head->sockets.end(), head->id);
        const float surface = skullSurfaceZ(*head, mouth->second.y);
        for (const PartDef* part : partsForCategory(PartCategory::Mouth, "round")) {
            // Every mouth recipe lands its front at dim.z*0.5 past the
            // socket — the cubes by filling their box, the flattened spheres
            // by being nudged forward to match. That shared reach is what
            // lets one authored z serve every mouth style on a skull; when
            // mouth_open and mouth_o reached only 0.033 and 0.020 of their
            // own accord, this same z left the small "o" inside the head.
            const float front =
                mouth->second.z + part->localSize.z * (kFeatureZPush + 0.5f);
            CAPTURE(head->id);
            CAPTURE(part->id);
            // Bounded by the standoff the derivation authors in, with one
            // standoff of slack: anything further out is the muzzle again.
            CHECK(front > surface);
            CHECK(front < surface + kMarkStandoff * 2.f);
        }
    }
}

TEST_CASE("catalog growth floors: hair>=18 eyes>=10 bodies>=6 mouths>=4 palettes>=12") {
    // Step 5. The roster test keeps the ten shipped looks valid and
    // pairwise-distinct; this pins the pool floors so a future cleanup
    // can't quietly shrink the variety the creator and NPCs share.
    CHECK(partsForCategory(PartCategory::Hair, "any").size() >= 18);
    CHECK(partsForCategory(PartCategory::Eyes, "any").size() >= 10);
    CHECK(partsForCategory(PartCategory::Body, "any").size() >= 6);
    CHECK(partsForCategory(PartCategory::Mouth, "any").size() >= 4);
    CHECK(paletteCatalog().size() >= 12);
}
