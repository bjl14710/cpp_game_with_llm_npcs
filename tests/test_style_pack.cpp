// Scaffolded stubs for the mii-style-visual-overhaul plan
// (.claude/plans/mii-style-visual-overhaul.md). Each stub maps to an
// acceptance criterion and is skipped until its step lands — un-skip in
// the same commit that implements it. Rendering criteria (outlines,
// proportions on screen) are screenshot-verified, not unit-tested.
#include <set>
#include <string>

#include "CharacterParts.hpp"
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

TEST_CASE("Mouth is a fifth category with sockets on every head" *
          doctest::skip()) {
    // Step 4. TODO(mii-style): kPartCategoryCount == 5; every Head part
    // declares a "mouth" socket; partsForCategory(Mouth, "any") is
    // non-empty; the existing exhaustive combo test in
    // test_character_parts.cpp absorbs the new axis automatically.
}

TEST_CASE("a five-item look line still parses, mouth defaults deterministically" *
          doctest::skip()) {
    // Step 4 back-compat. TODO(mii-style): parsePersonaText accepts the
    // old five-item `look =` (mouth filled via lookForPersona's
    // deterministic rule) AND the new six-item form; renderPersonaText
    // emits six items; round-trip holds. Likely lives beside the other
    // look tests in test_persona_look.cpp — keep or move this stub there.
}

TEST_CASE("catalog growth targets: hair>=18 eyes>=10 bodies>=6 mouths>=4 palettes>=12" *
          doctest::skip()) {
    // Step 5. TODO(mii-style): count partsForCategory(<cat>, "any") per
    // category and paletteCatalog().size() against the plan's floors, and
    // CHECK all ten roster looks stay valid + pairwise-distinct (the
    // roster test already enforces the latter — this pins the floors).
}

TEST_CASE("player avatar look round-trips through CharacterStore" *
          doctest::skip()) {
    // Step 6. TODO(mii-style): saveLook/loadAll under the reserved
    // "player_avatar" id — persists across a store reopen, validates on
    // load, stale look demotes to the fixed-seed fallback. Likely lives in
    // test_character_store.cpp with its temp-dir fixtures — keep or move.
}
