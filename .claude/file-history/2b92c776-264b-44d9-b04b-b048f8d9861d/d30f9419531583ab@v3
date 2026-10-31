// Tests for the stylized character assets plan
// (.claude/plans/stylized-character-assets.md). Shader/material routing and
// the no-uncanny judgment are screenshot/visual-qa verified, not unit-tested.
#include <string>

#include "CharacterParts.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("mesh field is structural: empty everywhere in core") {
    // The pack-field pattern: every CORE part is a primitive recipe, so
    // meshName is empty there; only the quaternius family (issue #139)
    // carries mesh references.
    for (const PartDef& part : partCatalog()) {
        CAPTURE(part.id);
        if (part.pack == "core") {
            CHECK(part.meshName.empty());
        } else {
            CHECK(part.pack == "quaternius");
        }
    }
}

TEST_CASE("quaternius family: mesh parts are well-formed") {
    // Issue #139: bodies and heads are mesh-backed with MEASURED bounds
    // and the sockets their children need; eyes/hair/mouth rows are
    // family-scale primitives (flat texture faces arrive in #140). The
    // exhaustive combination test in test_character_parts.cpp absorbs the
    // family automatically.
    int bodies = 0;
    int heads = 0;
    bool sawEyes = false;
    bool sawHair = false;
    bool sawMouth = false;
    for (const PartDef& part : partCatalog()) {
        if (part.pack != "quaternius") continue;
        CAPTURE(part.id);
        CHECK(part.styleTag == "quaternius");
        if (part.category == PartCategory::Body) {
            ++bodies;
            CHECK(!part.meshName.empty());
            CHECK(part.localSize.x > 0.f);
            CHECK(part.localSize.y > 0.f);
            CHECK(part.localSize.z > 0.f);
            CHECK(part.sockets.count("head") == 1);
        } else if (part.category == PartCategory::Head) {
            ++heads;
            CHECK(!part.meshName.empty());
            CHECK(part.localSize.x > 0.f);
            CHECK(part.localSize.y > 0.f);
            CHECK(part.localSize.z > 0.f);
            CHECK(part.sockets.count("eyes") == 1);
            CHECK(part.sockets.count("hair") == 1);
            CHECK(part.sockets.count("mouth") == 1);
        } else if (part.category == PartCategory::Eyes) {
            sawEyes = true;
            CHECK(part.meshName.empty());
        } else if (part.category == PartCategory::Hair) {
            sawHair = true;
            CHECK(part.meshName.empty());
        } else if (part.category == PartCategory::Mouth) {
            sawMouth = true;
            CHECK(part.meshName.empty());
        }
    }
    CHECK(bodies >= 4);
    CHECK(heads >= 4);
    CHECK(sawEyes);
    CHECK(sawHair);
    CHECK(sawMouth);

    // The family is EXCLUSIVE: the "any" bridge must not reach it — core
    // primitive parts are authored at ~4x the pack's units and would be
    // wildly out of scale on a mesh body.
    const PartDef* qBody = findPart("body_q_adventurer");
    const PartDef* anyEyes = findPart("eyes_dot");
    REQUIRE(qBody);
    REQUIRE(anyEyes);
    CHECK_FALSE(styleCompatible(*qBody, *anyEyes));
}

TEST_CASE("default pool switches to quaternius; core stays loadable") {
    // Issue #139: randomizeLook (and therefore lookForPersona's hash
    // fallback and the creator's draft look) generates from the mesh
    // family only...
    for (const unsigned seed : {1u, 7u, 42u, 12345u, 999999u}) {
        const CharacterLook look = randomizeLook(seed);
        std::string why;
        CAPTURE(seed);
        CAPTURE(why);
        CHECK(lookIsValid(look, &why));
        for (int c = 0; c < kPartCategoryCount; ++c) {
            const PartDef* part = findPart(look.partIds[c]);
            REQUIRE(part);
            CHECK(part->styleTag == "quaternius");
        }
        CHECK(assembleLook(look).ok);
    }

    // ...while a look stored before the switch still validates and
    // assembles — defaults change, nothing breaks.
    CharacterLook core;
    core.part(PartCategory::Body) = "body_round";
    core.part(PartCategory::Head) = "head_round";
    core.part(PartCategory::Eyes) = "eyes_dot";
    core.part(PartCategory::Hair) = "hair_tuft";
    core.part(PartCategory::Mouth) = "mouth_smile";
    core.paletteId = "warm";
    std::string why;
    CAPTURE(why);
    CHECK(lookIsValid(core, &why));
    CHECK(assembleLook(core).ok);
}

TEST_CASE("proportion window holds for the mesh family (or new values logged)") {
    // Issue #139, deliberate size-spec decision: the quaternius pack is
    // realistically proportioned (~6 heads tall), NOT Mii-proportioned, so
    // this family gets its own head-fraction window instead of the core
    // 0.48-0.60 (test_style_pack now scopes that window to core parts).
    // Measured: 0.159 (adventurer/soldier), 0.170 (scifi), 0.269 (witch —
    // her hat is part of the head bounds). Window 0.14-0.28 pins the
    // family against accidental re-scaling in either direction.
    for (const PartDef* body : partsForCategory(PartCategory::Body, "quaternius")) {
        const auto socket = body->sockets.find("head");
        REQUIRE(socket != body->sockets.end());
        for (const PartDef* head :
             partsForCategory(PartCategory::Head, "quaternius")) {
            CAPTURE(body->id);
            CAPTURE(head->id);
            const float fraction =
                head->localSize.y / (socket->second.y + head->localSize.y);
            CHECK(fraction >= 0.14f);
            CHECK(fraction <= 0.28f);
        }
    }
}

TEST_CASE("stylized face set: creator-pickable textures per face id" *
          doctest::skip()) {
    // Step 4. TODO(stylized): FaceTexture's stylized set exposes >=8 face
    // ids, each generating a texture; mood variants exist per face
    // (happy/angry/sad/embarrassed/surprised) so the new family shows
    // moods ON the face, not as a floating billboard.
}
