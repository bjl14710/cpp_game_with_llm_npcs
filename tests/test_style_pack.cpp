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
    // Step 1 (pack seam). Live already — the defaulted field ships with
    // the scaffold, so this one is NOT skipped: it pins the seam down.
    for (const PartDef& part : partCatalog()) {
        CHECK_MESSAGE(!part.pack.empty(), part.id);
        CHECK(part.pack == "core");  // only the built-in pack exists tonight
    }
    for (const PartPalette& palette : paletteCatalog()) {
        CHECK_MESSAGE(!palette.pack.empty(), palette.id);
        CHECK(palette.pack == "core");
    }
}

TEST_CASE("Mii proportions: heads read as ~half the body+head silhouette") {
    // Step 2. Metric refined from the scaffold stub (logged in
    // OVERNIGHT_REPORT.md): hair height varies per look, so the STABLE
    // measure is head.y / (headSocket.y + head.y) — the body+head
    // silhouette. 0.48-0.60 lands the plan's "head reads ~40-45% of
    // standing height" once a typical hair crown joins the assembly.
    for (const PartDef* body : partsForCategory(PartCategory::Body, "any")) {
        const auto socket = body->sockets.find("head");
        REQUIRE_MESSAGE(socket != body->sockets.end(), body->id);
        for (const PartDef* head :
             partsForCategory(PartCategory::Head, body->styleTag)) {
            const float fraction =
                head->localSize.y / (socket->second.y + head->localSize.y);
            CAPTURE(body->id);
            CAPTURE(head->id);
            CHECK(fraction >= 0.48f);
            CHECK(fraction <= 0.60f);
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

TEST_CASE("player avatar look round-trips through CharacterStore" *
          doctest::skip()) {
    // Step 6. TODO(mii-style): saveLook/loadAll under the reserved
    // "player_avatar" id — persists across a store reopen, validates on
    // load, stale look demotes to the fixed-seed fallback. Likely lives in
    // test_character_store.cpp with its temp-dir fixtures — keep or move.
}
