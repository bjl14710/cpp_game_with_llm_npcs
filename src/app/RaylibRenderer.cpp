#include "RaylibRenderer.hpp"

#include <algorithm>
#include <cmath>

#include "raymath.h"
#include "rlgl.h"

#include "DayNight.hpp"
#include "Math.hpp"

namespace llm_npc {

namespace {

// Street geometry from City::makeDowntown's documented layout: 3x3 blocks on
// a 64-unit pitch, 16-unit streets centered between block edges at ±32.
constexpr float kStreetCenter = 32.f;
constexpr float kStreetWidth = 16.f;

// Ground/asphalt/sidewalk palette — warm, low-poly-friendly flats.
constexpr Color kGrass{116, 153, 86, 255};
constexpr Color kAsphalt{68, 70, 78, 255};
constexpr Color kPlaza{188, 178, 158, 255};
constexpr Color kSidewalk{158, 152, 140, 255};

// Draws `model` scaled UNIFORMLY to stand `worldHeight` tall, feet on the
// ground, centered on (centerX, centerZ) — the Uniform size contract:
// the asset keeps its proportions regardless of any collision box.
void drawModelUniform(const Model& model, float centerX, float centerZ,
                      float worldHeight, Color tint) {
    const BoundingBox bb = GetModelBoundingBox(model);
    const float h = bb.max.y - bb.min.y;
    if (h <= 0.f) return;
    const float s = worldHeight / h;
    const Vector3 position{centerX - (bb.min.x + bb.max.x) * 0.5f * s,
                           -bb.min.y * s,
                           centerZ - (bb.min.z + bb.max.z) * 0.5f * s};
    DrawModelEx(model, position, Vector3{0.f, 1.f, 0.f}, 0.f, Vector3{s, s, s}, tint);
}

// Draws `model` scaled and translated so its bounding box exactly fills the
// axis-aligned footprint [minX,maxX]x[minZ,maxZ] with the given height,
// sitting on the ground plane — the Fill size contract (buildings).
// Rotation-free: KayKit city pieces face +Z at identity, which matches the
// street-facing fronts of the downtown layout.
void drawModelFittedToAABB(const Model& model, float minX, float minZ, float maxX,
                           float maxZ, float height, Color tint) {
    const BoundingBox bb = GetModelBoundingBox(model);
    const Vector3 size{bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z};
    if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f) return;
    const Vector3 scale{(maxX - minX) / size.x, height / size.y, (maxZ - minZ) / size.z};
    const Vector3 position{minX - bb.min.x * scale.x, -bb.min.y * scale.y,
                           minZ - bb.min.z * scale.z};
    DrawModelEx(model, position, Vector3{0.f, 1.f, 0.f}, 0.f, scale, tint);
}

// Draws one KayKit road tile (origin-centered 2x2 slab) filling the world
// footprint worldX x worldZ at (cx, cz). The Fill scaling happens in model
// space, so a 90-degree yaw (X-running streets) swaps which model axis
// covers which world axis. Thickness is pinned rather than scaled: a true
// 8x fill of the 0.1-thick tile would raise a 0.8-unit curb over the flat
// collision plane.
void drawRoadTile(const Model& model, float cx, float cz, float worldX,
                  float worldZ, bool rotate90) {
    const BoundingBox bb = GetModelBoundingBox(model);
    const Vector3 size{bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z};
    if (size.x <= 0.f || size.y <= 0.f || size.z <= 0.f) return;
    constexpr float kRoadThickness = 0.05f;
    const Vector3 scale{(rotate90 ? worldZ : worldX) / size.x,
                        kRoadThickness / size.y,
                        (rotate90 ? worldX : worldZ) / size.z};
    // Base floats a hair above the grass plane to avoid coplanar flicker.
    DrawModelEx(model, Vector3{cx, 0.01f, cz}, Vector3{0.f, 1.f, 0.f},
                rotate90 ? 90.f : 0.f, scale, WHITE);
}

// Composite plaza fountain — the KayKit city pack has no fountain model, so
// it is built from primitives on the authored footprint: a stone basin wall,
// two smaller tiers, and translucent water discs. `worldHeight` comes from
// the fountain's SizeSpec (the single sizing source); radii derive from the
// collision AABB so visuals and collider always match.
void drawFountain(const Building& b, float worldHeight) {
    const float cx = (b.minX + b.maxX) * 0.5f;
    const float cz = (b.minZ + b.maxZ) * 0.5f;
    const float radius = std::min(b.maxX - b.minX, b.maxZ - b.minZ) * 0.5f;

    constexpr Color kStone{172, 168, 156, 255};
    constexpr Color kStoneDark{136, 132, 122, 255};
    constexpr Color kWater{70, 140, 200, 170};

    // Tier proportions (fraction of footprint radius / total height).
    const float basinH = worldHeight * 0.25f;
    const float midH = worldHeight * 0.50f;
    const float topH = worldHeight * 0.25f;

    // DrawCylinder renders capped solids, so each "water surface" is a thin
    // translucent disc floated just above its stone cap — recessed water
    // would be hidden by the cap itself.

    // Basin: outer wall filled to the brim with water.
    DrawCylinder({cx, 0.f, cz}, radius, radius, basinH, 24, kStone);
    DrawCylinderWires({cx, 0.f, cz}, radius, radius, basinH, 24, kStoneDark);
    DrawCylinder({cx, basinH, cz}, radius * 0.88f, radius * 0.88f, 0.03f, 24, kWater);

    // Middle tier: column carrying a smaller bowl with its own water disc.
    DrawCylinder({cx, basinH, cz}, radius * 0.18f, radius * 0.26f, midH, 12, kStone);
    DrawCylinder({cx, basinH + midH * 0.7f, cz}, radius * 0.45f, radius * 0.45f,
                 midH * 0.3f, 20, kStone);
    DrawCylinder({cx, basinH + midH, cz}, radius * 0.38f, radius * 0.38f, 0.03f,
                 20, kWater);

    // Top spout: a slim finial peaking at exactly worldHeight.
    DrawCylinder({cx, basinH + midH, cz}, radius * 0.07f, radius * 0.14f, topH,
                 10, kStoneDark);
}

}  // namespace

RaylibRenderer::RaylibRenderer(Assets& assets) : assets_(assets) {}

void RaylibRenderer::setTimeOfDay(float hours) {
    // Sky, fog, and light all read the same curves from the same clock —
    // fog keeps melting distant geometry into the horizon at any hour.
    const SkyColor sky = skyColorAt(hours);
    skyColor_ = Color{static_cast<unsigned char>(sky.r * 255.f),
                      static_cast<unsigned char>(sky.g * 255.f),
                      static_cast<unsigned char>(sky.b * 255.f), 255};
    lightLevel_ = lightLevelAt(hours);
    const float fogColor[4] = {sky.r, sky.g, sky.b, 1.f};
    if (const Shader* fog = assets_.fogShader()) {
        SetShaderValue(*fog, assets_.fogColorLoc(), fogColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(*fog, assets_.fogLightLoc(), &lightLevel_,
                       SHADER_UNIFORM_FLOAT);
    }
    // The character cel and outline programs fog with the same curves —
    // characters and their rims haze into the same horizon as the city.
    if (const Shader* cel = assets_.celShader()) {
        SetShaderValue(*cel, assets_.celColorLoc(), fogColor, SHADER_UNIFORM_VEC4);
        SetShaderValue(*cel, assets_.celLightLoc(), &lightLevel_,
                       SHADER_UNIFORM_FLOAT);
    }
    if (const Shader* outline = assets_.outlineShader()) {
        SetShaderValue(*outline, assets_.outlineColorLoc(), fogColor,
                       SHADER_UNIFORM_VEC4);
    }
}

void RaylibRenderer::beginFrame(const CameraPose& pose) {
    camera_.position = {pose.position.x, pose.position.y + kEyeHeight, pose.position.z};
    // Same authoritative look vector the weapon aims along (issue #91), so the
    // crosshair and the shot can never disagree.
    const Vec3 dir = lookDirection(pose.yawDeg, pose.pitchDeg);
    camera_.target = {camera_.position.x + dir.x, camera_.position.y + dir.y,
                      camera_.position.z + dir.z};
    camera_.up = {0.f, 1.f, 0.f};
    camera_.fovy = 70.f;
    camera_.projection = CAMERA_PERSPECTIVE;

    // Atmosphere: models carry the fog shader on their materials; wrapping
    // the whole 3D pass in BeginShaderMode routes the primitive batch
    // (ground plane, slabs, fountain, viewmodel) through the same shader so
    // everything hazes consistently. Fog distance needs the camera each
    // frame.
    const float eye[3] = {camera_.position.x, camera_.position.y,
                          camera_.position.z};
    if (const Shader* cel = assets_.celShader()) {
        SetShaderValue(*cel, assets_.celCameraLoc(), eye, SHADER_UNIFORM_VEC3);
    }
    if (const Shader* outline = assets_.outlineShader()) {
        SetShaderValue(*outline, assets_.outlineCameraLoc(), eye,
                       SHADER_UNIFORM_VEC3);
    }
    if (const Shader* fog = assets_.fogShader()) {
        SetShaderValue(*fog, assets_.fogCameraLoc(), eye, SHADER_UNIFORM_VEC3);
        BeginMode3D(camera_);
        BeginShaderMode(*fog);
        return;
    }
    BeginMode3D(camera_);
}

void RaylibRenderer::beginCharacterShader() {
    if (const Shader* cel = assets_.celShader()) BeginShaderMode(*cel);
}

void RaylibRenderer::endCharacterShader() {
    if (!assets_.celShader()) return;  // nothing was begun
    if (const Shader* fog = assets_.fogShader()) {
        BeginShaderMode(*fog);  // hand the batch back to the frame's shader
    } else {
        EndShaderMode();
    }
}

void RaylibRenderer::drawCity(const City& city) {
    const float half = city.halfSize();

    // Ground: grass base with the street grid and block slabs on top.
    DrawPlane({0.f, 0.f, 0.f}, {half * 2.f, half * 2.f}, kGrass);

    // Streets: KayKit road tiles when the pack is present, flat asphalt
    // strips otherwise. Tile pitch = street width (16), centers at multiples
    // of 16 so the four junctions at (+-32, +-32) land exactly on tile
    // centers; the arm tiles beside each junction (+-16, +-48) carry zebra
    // crossings where the sidewalks meet. 13 tiles cover +-104; a compressed
    // end-cap tile spans the last 6 units to the +-110 map edge.
    const Model* straight = assets_.prop("road_straight");
    const Model* crossing = assets_.prop("road_straight_crossing");
    const Model* junction = assets_.prop("road_junction");
    if (city.hasStreets() && straight && crossing && junction) {
        constexpr float kTile = 16.f;
        for (const float sc : {-kStreetCenter, kStreetCenter}) {
            for (float t = -96.f; t <= 96.f; t += kTile) {
                if (t == -kStreetCenter || t == kStreetCenter) {
                    // Both streets pass through this tile; draw the 4-way
                    // junction art once (in the Z-running pass).
                    drawRoadTile(*junction, sc, t, kStreetWidth, kTile, false);
                    continue;
                }
                const float arm = std::fabs(std::fabs(t) - kStreetCenter);
                const Model& tile = (arm == kTile) ? *crossing : *straight;
                drawRoadTile(tile, sc, t, kStreetWidth, kTile, false);
                drawRoadTile(tile, t, sc, kTile, kStreetWidth, true);
            }
            for (const float e : {-107.f, 107.f}) {
                drawRoadTile(*straight, sc, e, kStreetWidth, 6.f, false);
                drawRoadTile(*straight, e, sc, 6.f, kStreetWidth, true);
            }
        }
    } else if (city.hasStreets()) {
        for (const float c : {-kStreetCenter, kStreetCenter}) {
            DrawCube({c, 0.02f, 0.f}, kStreetWidth, 0.04f, half * 2.f, kAsphalt);
            DrawCube({0.f, 0.02f, c}, half * 2.f, 0.04f, kStreetWidth, kAsphalt);
        }
    }
    // Block slabs: plaza (center) is pale stone, park (north-east) stays
    // grass, every other block gets a sidewalk-toned slab. Sandbox cities
    // (no authored street grid) stay plain grass.
    if (city.hasStreets()) {
        for (const float bx : {-64.f, 0.f, 64.f}) {
            for (const float bz : {-64.f, 0.f, 64.f}) {
                const bool plaza = bx == 0.f && bz == 0.f;
                const bool park = bx == 64.f && bz == 64.f;
                if (park) continue;
                DrawCube({bx, 0.04f, bz}, 48.f, 0.08f, 48.f, plaza ? kPlaza : kSidewalk);
            }
        }
    }

    // Street furniture (traffic lights, bushes) renders through the same
    // buildings pass below — City authors their positions AND colliders,
    // so nothing visible can be walk-through.

    // Buildings and street props from the collision AABBs — the models scale
    // to fit the authoritative footprints, never the other way around.
    for (const Building& b : city.buildings()) {
        // The fountain is composed from primitives (no pack model). Must be
        // dispatched by id BEFORE the model lookup: modelForBuilding falls
        // back to a hashed generic building for uncurated ids.
        // Compare the BASE id: sandbox instances carry a '#' suffix
        // ("fountain#4") and must hit the same composite recipe.
        if (b.id.substr(0, b.id.find('#')) == "fountain") {
            drawFountain(b, assets_.sizeSpecFor(b).worldHeight);
            continue;
        }
        if (const Model* model = assets_.modelForBuilding(b)) {
            const Assets::SizeSpec& spec = assets_.sizeSpecFor(b);
            if (spec.mode == Assets::SizeSpec::Mode::Uniform) {
                drawModelUniform(*model, (b.minX + b.maxX) * 0.5f,
                                 (b.minZ + b.maxZ) * 0.5f, spec.worldHeight, WHITE);
            } else {
                drawModelFittedToAABB(*model, b.minX, b.minZ, b.maxX, b.maxZ, b.height,
                                      WHITE);
            }
        } else {
            // Fallback primitive, tinted like the legacy renderer's boxes.
            const unsigned char tint =
                static_cast<unsigned char>(120 + (b.facadeKind * 37) % 90);
            const Vector3 center{(b.minX + b.maxX) * 0.5f, b.height * 0.5f,
                                 (b.minZ + b.maxZ) * 0.5f};
            DrawCube(center, b.maxX - b.minX, b.height, b.maxZ - b.minZ,
                     Color{tint, tint, static_cast<unsigned char>(tint + 20), 255});
            DrawCubeWires(center, b.maxX - b.minX, b.height, b.maxZ - b.minZ,
                          Color{40, 45, 60, 255});
        }
    }
}

void RaylibRenderer::drawCharacter(const CharacterVisual& visual) {
    const Assets::CharacterAsset* character =
        assets_.characterFor(visual.variantSeed, visual.police);
    if (!character) {
        // No pack: marker cylinder, same as the pre-asset placeholder.
        const Vec3& p = visual.position;
        DrawCylinder({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{200, 120, 80, 255});
        DrawCylinderWires({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{60, 30, 20, 255});
        return;
    }

    // Pick the clip: death overrides everything, gestures interrupt,
    // then walk/idle by movement.
    int clip = visual.walking ? character->walk : character->idle;
    if (visual.gesturePhase > 0.f && character->gesture >= 0) clip = character->gesture;
    if (visual.dead && character->death >= 0) clip = character->death;

    AnimState& state = anim_[visual.variantSeed];
    if (state.clip != clip) {
        state.clip = clip;
        state.time = 0.f;
    }
    state.time += GetFrameTime();
    if (clip >= 0 && clip < character->clipCount) {
        // raylib bakes glTF animations at ~60 samples/second; loop by frame.
        const ModelAnimation& animation = character->clips[clip];
        const int frame =
            static_cast<int>(state.time * 60.f) % (animation.frameCount > 0
                                                       ? animation.frameCount
                                                       : 1);
        // CPU skinning on the shared model right before this entity's draw,
        // so several entities can reuse one model with different clocks.
        UpdateModelAnimation(character->model, animation, frame);
    }

    // Uniform height: scale so every character stands ~1.8 units tall, feet
    // on the ground, rotated to the game's facing convention (0 → +Z).
    const BoundingBox bb = GetModelBoundingBox(character->model);
    const float modelHeight = bb.max.y - bb.min.y;
    const float s = modelHeight > 0.f ? 1.8f / modelHeight : 1.f;
    const Vector3 position{visual.position.x, -bb.min.y * s, visual.position.z};
    DrawModelEx(character->model, position, Vector3{0.f, 1.f, 0.f}, visual.facingDeg,
                Vector3{s, s, s}, WHITE);

    // Cartoon rim (issue #138) — the mesh version of the #103 inverted
    // hull: redraw every mesh through the outline shader (vertices pushed
    // out along their world-space normals, solid rim color) with FRONT
    // faces culled so only the silhouette band survives. The batch drains
    // before each cull switch, exactly like the primitive hull; the CPU-
    // skinned vertices are already posed from the draw above.
    if (const Material* rim = assets_.outlineMaterial()) {
        const Matrix local = MatrixMultiply(
            MatrixScale(s, s, s),
            MatrixRotate(Vector3{0.f, 1.f, 0.f}, visual.facingDeg * DEG2RAD));
        const Matrix transform = MatrixMultiply(
            MatrixMultiply(character->model.transform, local),
            MatrixTranslate(position.x, position.y, position.z));
        rlDrawRenderBatchActive();
        rlSetCullFace(RL_CULL_FACE_FRONT);
        for (int i = 0; i < character->model.meshCount; ++i) {
            DrawMesh(character->model.meshes[i], *rim, transform);
        }
        rlDrawRenderBatchActive();
        rlSetCullFace(RL_CULL_FACE_BACK);
    }

    // Non-neutral moods float as an emote above the head — the same six
    // procedural faces the legacy renderer painted on, now billboarded.
    // The dead don't emote.
    if (!visual.dead && visual.face != NpcFace::Neutral) {
        DrawBillboard(camera_, assets_.faceTexture(visual.face),
                      Vector3{visual.position.x, 2.45f, visual.position.z}, 0.55f,
                      WHITE);
    }
}

namespace {

// Palette colors resolved once per character and handed to every recipe.
// skin/hair/outfit/pants come from the look's palette; the feature colors
// below are FIXED across all twelve palettes — an eye white is an eye
// white whatever the character is wearing.
struct RecipeColors {
    Color skin;
    Color skinDeep;  // one shade under `skin`: the nose bead
    Color hair;
    Color outfit;
    Color pants;
    Color dark;      // hard black plastic: glasses frames and visors ONLY
    Color shoe;
    Color sclera;
    Color iris;
    Color spark;
    Color mouth;
    Color blush;
};

// Design tokens for the fixed feature colors. One near-black used to paint
// pupils, lids, brows and mouths alike, which is the colour of a hole — on
// a mouth it read as a scream. Split per feature instead.
constexpr Color kSclera{253, 249, 242, 255};  // warm white, never pure WHITE
constexpr Color kIris{74, 52, 42, 255};       // warm dark brown
constexpr Color kSpark{255, 255, 255, 255};   // the one place pure white belongs
constexpr Color kMouthTone{180, 100, 92, 255};
constexpr Color kBlush{240, 168, 148, 255};
constexpr Color kShoe{63, 58, 63, 255};
constexpr Color kPlastic{38, 38, 44, 255};

// Rim color for the inverted-hull outline pass (issue #103) — warm brown
// ink rather than a cool void, and thin enough (see kHullScale) to read as
// a drawn line instead of a crack.
constexpr Color kOutlineColor{58, 44, 40, 255};

// Every field one colour: the hull pass draws each recipe as a flat
// silhouette, so no feature may show through the inflated copy.
RecipeColors flatColors(Color c) {
    return RecipeColors{c, c, c, c, c, c, c, c, c, c, c, c};
}

// The nose bead: skin, one shade deeper. Derived rather than authored so it
// tracks all twelve palettes (the reference model's shade against its warm
// skin is this ratio).
Color deepenSkin(Color skin) {
    return Color{static_cast<unsigned char>(skin.r * 0.93f),
                 static_cast<unsigned char>(skin.g * 0.89f),
                 static_cast<unsigned char>(skin.b * 0.84f), 255};
}

// TODO(stylized step 3): add the MESH branch at the top of this dispatch:
// if (!part.meshName.empty()) draw the loaded mesh at `at`, scaled so its
// measured bounds fill `dim` (the contract already resolved the numbers) —
// then return. The quaternius family flows through here with zero changes
// to assembly/picker code. TODO(stylized step 4): quaternius heads draw a
// FLAT FACE QUAD (head-oriented, not camera-billboard) at the face socket,
// textured from FaceTexture's stylized set; mood = face-texture variant,
// replacing the emote billboard for that family.
// The ONE place part ids map to shapes — the graphics-pack seam (issue
// #101). A content pack is exactly: catalog rows (PartDef/PartPalette
// with their `pack` tag) + recipe branches HERE. Nothing else — not the
// assembly, not the picker, not the render loop — changes when a pack is
// added. `at` is the part's anchor (bottom-center) and `dim` its declared
// size, both already in world units (contract scale applied); a part
// without a bespoke branch falls through to the generic declared-box
// recipe at the bottom, so new catalog rows always render.
void drawPartRecipe(const PartDef& part, const Vec3& at, const Vec3& dim,
                    const RecipeColors& c) {
    const float cy = at.y + dim.y * 0.5f;  // box center height
    // Feature parts (eyes, mouth) sit ON their socket rather than on their
    // box centre: the head authors the exact feature LINE (eyes at ~46% of
    // head height, mouths at ~28%), and centring on the box would drift
    // that line by half the part's height — by a different amount for every
    // eye style. The declared box still gives the feature its width and the
    // face its depth budget.
    const float fy = at.y;
    const float fz = at.z + dim.z * 0.35f;

    // raylib primitives are axis-symmetric; the reference figure squashes
    // several of them (a torso flattened front-to-back reads as a body, not
    // a barrel). Scale about the shape's own centre so the surrounding
    // outline-hull matrix still composes correctly.
    auto squashed = [&](Vector3 pivot, float sx, float sy, float sz,
                        auto&& draw) {
        rlPushMatrix();
        rlTranslatef(pivot.x, pivot.y, pivot.z);
        rlScalef(sx, sy, sz);
        rlTranslatef(-pivot.x, -pivot.y, -pivot.z);
        draw();
        rlPopMatrix();
    };

    // One dark ball per eye is what made the old faces read as dolls:
    // near-black spheres sitting on skin with nothing behind them and no
    // light in them. Three primitives instead — white, iris, spark, always
    // in that order. The spark is the whole trick: about twenty triangles
    // between a doll and a character. The sclera is FLATTENED front-to-back
    // so a big eye sits IN the face instead of bulging off it, while the
    // iris and spark stay proud of it. `side` mirrors the spark so both
    // eyes catch the same light.
    auto drawEye = [&](float x, float y, float z, float r, float side) {
        squashed({x, y, z}, 1.f, 1.05f, 0.60f,
                 [&] { DrawSphere({x, y, z}, r, c.sclera); });
        const Vector3 iris{x, y, z + r * 0.45f};
        squashed(iris, 1.f, 1.f, 0.62f,
                 [&] { DrawSphere(iris, r * 0.55f, c.iris); });
        DrawSphere({x - side * r * 0.27f, y + r * 0.32f, z + r * 0.78f},
                   r * 0.20f, c.spark);
    };

    // Brows, in hair tone, a touch above the eye line with the outer end
    // dropped so the expression reads relaxed rather than surprised. A face
    // without brows reads blank no matter how good the eyes are. Sized off
    // the part's WIDTH, which tracks the head; the part's height is the eye
    // style's own business and swings 2.5x across the catalog.
    auto drawBrows = [&](float dx) {
        for (const float side : {-1.f, 1.f}) {
            DrawCube({at.x + side * dx * 1.15f, fy + dim.x * 0.28f,
                      fz + dim.z * 0.10f},
                     dim.x * 0.33f, dim.x * 0.076f, dim.z * 0.70f, c.hair);
        }
    };

    // Ears, cheek warmth and a nose bead. All three are nearly free and all
    // three read as human instantly; the cheeks should be felt, not seen,
    // and the nose exists only to stop the face reading flat. `cheekZ` and
    // `noseZ` place them ON the face — a round skull and a boxy one put
    // their surface in different places.
    auto drawHeadFeatures = [&](float earX, float cheekZ, float noseZ) {
        squashed({at.x, at.y + dim.y * 0.383f, at.z + dim.z * noseZ}, 1.f, 0.82f,
                 0.80f, [&] {
                     DrawSphere({at.x, at.y + dim.y * 0.383f,
                                 at.z + dim.z * noseZ},
                                dim.x * 0.044f, c.skinDeep);
                 });
        for (const float side : {-1.f, 1.f}) {
            const Vector3 ear{at.x + side * dim.x * earX,
                              at.y + dim.y * 0.447f, at.z - dim.z * 0.019f};
            squashed(ear, 0.70f, 1.f, 0.75f,
                     [&] { DrawSphere(ear, dim.x * 0.089f, c.skin); });
            const Vector3 cheek{at.x + side * dim.x * 0.334f,
                                at.y + dim.y * 0.367f, at.z + dim.z * cheekZ};
            squashed(cheek, 1.f, 0.60f, 0.22f,
                     [&] { DrawSphere(cheek, dim.x * 0.084f, c.blush); });
        }
    };

    // A standing figure instead of a cone. The old single tapered cylinder
    // is why the silhouette never resolved at street distance: a head on a
    // cone reads as a ghost. Every number below is the design reference's
    // 1.8u figure expressed as a fraction of the body's declared box, so
    // the bands land where the handoff specifies them — shoes 0.00-0.10,
    // legs 0.10-0.52, torso and arms 0.52-1.12, neck 1.12-1.18, head above.
    // Capsule ENDPOINTS are the cylinder ends, not the extremes: a capsule
    // written straight onto a band boundary overshoots it by its radius,
    // which is what buries the neck in the chest and the legs in the hips.
    // Arms cost the most and buy the most — they are what makes the
    // silhouette a person. A short outfit sleeve over a skin forearm
    // separates the arm from the torso; an all-outfit arm reads as a
    // flipper.
    auto drawFigure = [&](float torsoR, float hipR) {
        const float w = dim.x, h = dim.y, y0 = at.y;
        const Vector3 hips{at.x, y0 + h * 0.471f, at.z};
        squashed(hips, 1.10f, 0.68f, 0.80f,
                 [&] { DrawSphere(hips, w * hipR, c.pants); });
        const Vector3 torso{at.x, y0 + h * 0.683f, at.z};
        squashed(torso, 1.05f, 1.f, 0.78f, [&] {
            DrawCapsule({at.x, y0 + h * 0.617f, at.z},
                        {at.x, y0 + h * 0.750f, at.z}, w * torsoR, 16, 8,
                        c.outfit);
        });
        DrawCylinder({at.x, y0 + h * 0.875f, at.z}, w * 0.141f, w * 0.158f,
                     h * 0.117f, 16, c.skin);
        // Arms hang just clear of the torso's flattened surface, so the
        // offsets track each body's build instead of being absolute.
        const float armX = torsoR * 1.05f + 0.041f;
        for (const float side : {-1.f, 1.f}) {
            const float legX = at.x + side * w * 0.198f;
            DrawCapsule({legX, y0 + h * 0.1625f, at.z},
                        {legX, y0 + h * 0.3625f, at.z}, w * 0.146f, 12, 8,
                        c.pants);
            DrawCapsule({legX, y0 + h * 0.053f, at.z - w * 0.023f},
                        {legX, y0 + h * 0.053f, at.z + w * 0.167f}, w * 0.110f,
                        12, 8, c.shoe);
            DrawCapsule({at.x + side * w * (armX + 0.007f), y0 + h * 0.858f, at.z},
                        {at.x + side * w * (armX - 0.007f), y0 + h * 0.817f, at.z},
                        w * 0.107f, 12, 8, c.outfit);
            DrawCapsule({at.x + side * w * (armX + 0.072f), y0 + h * 0.786f, at.z},
                        {at.x + side * w * (armX + 0.048f), y0 + h * 0.622f, at.z},
                        w * 0.084f, 12, 8, c.skin);
            const Vector3 hand{at.x + side * w * (armX + 0.121f),
                               y0 + h * 0.575f, at.z + w * 0.021f};
            squashed(hand, 0.90f, 1.f, 0.78f,
                     [&] { DrawSphere(hand, w * 0.127f, c.skin); });
        }
    };

    if (part.category == PartCategory::Body && part.pack == "core") {
        // Per-body character survives the shared figure recipe in two
        // numbers: how thick the torso is and how wide the hips are.
        // body_round carries the reference model's own values.
        float torsoR = 0.379f;
        float hipR = 0.327f;
        if (part.id == "body_slim") {
            torsoR = 0.335f;
            hipR = 0.280f;
        } else if (part.id == "body_pear") {
            torsoR = 0.350f;
            hipR = 0.390f;  // bottom-heavy
        } else if (part.id == "body_block") {
            torsoR = 0.395f;
            hipR = 0.330f;
        } else if (part.id == "body_bulk") {
            torsoR = 0.430f;
            hipR = 0.370f;
        } else if (part.id == "body_broad") {
            torsoR = 0.445f;
            hipR = 0.360f;
        }
        drawFigure(torsoR, hipR);
    } else if (part.id == "head_round") {
        DrawSphere({at.x, cy, at.z}, dim.y * 0.5f, c.skin);
        drawHeadFeatures(0.485f, 0.320f, 0.460f);
    } else if (part.id == "head_block" || part.id == "head_tall") {
        // The blocky family keeps its box skull; ears, cheeks and a nose
        // are what turn it from a crate into a face. A flat face puts its
        // surface further forward than a round one, hence the larger z.
        DrawCube({at.x, cy, at.z}, dim.x, dim.y, dim.z, c.skin);
        drawHeadFeatures(0.485f, 0.500f, 0.520f);
    } else if (part.id == "hair_tuft") {
        DrawSphere({at.x, at.y + dim.y * 0.3f, at.z}, dim.x * 0.5f, c.hair);
    } else if (part.id == "hair_bowl") {
        // Oversized cap sunk into the head reads as a bowl cut.
        DrawSphere({at.x, at.y - dim.y * 0.55f, at.z}, dim.x * 0.5f, c.hair);
    } else if (part.id == "hair_spikes") {
        for (int i = -1; i <= 1; ++i) {
            DrawCylinder({at.x + static_cast<float>(i) * dim.x * 0.30f,
                          at.y - dim.y * 0.25f, at.z},
                         0.f, dim.x * 0.16f, dim.y, 8, c.hair);
        }
    } else if (part.id == "head_oval") {
        // Vertically stretched sphere reads as a longer, oval face.
        rlPushMatrix();
        rlTranslatef(at.x, cy, at.z);
        rlScalef(1.f, dim.y / dim.x, 1.f);
        DrawSphere({0.f, 0.f, 0.f}, dim.x * 0.5f, c.skin);
        rlPopMatrix();
        drawHeadFeatures(0.485f, 0.348f, 0.486f);
    } else if (part.id == "hair_pony") {
        // A rounded cap plus a small tail behind the head.
        DrawSphere({at.x, at.y + dim.y * 0.30f, at.z}, dim.x * 0.5f, c.hair);
        DrawSphere({at.x, at.y - dim.y * 0.10f, at.z - dim.z * 0.5f},
                   dim.x * 0.34f, c.hair);
    } else if (part.id == "hair_mohawk") {
        // A single tall narrow crest running front-to-back.
        DrawCube({at.x, cy, at.z}, dim.x * 0.5f, dim.y, dim.z, c.hair);
    } else if (part.id == "hair_cap") {
        // A flat crown sunk onto the head plus a forward brim (+z is
        // the character's front, same as the eye sockets).
        DrawCylinder({at.x, at.y - dim.y * 0.40f, at.z}, dim.x * 0.50f,
                     dim.x * 0.55f, dim.y, 12, c.hair);
        DrawCube({at.x, at.y + dim.y * 0.10f, at.z + dim.z * 0.34f},
                 dim.x * 0.90f, dim.y * 0.18f, dim.z * 0.50f, c.hair);
    } else if (part.id == "hair_buzz") {
        // A tight crop: one thin disc hugging the scalp.
        DrawCylinder({at.x, at.y - dim.y * 0.30f, at.z}, dim.x * 0.48f,
                     dim.x * 0.50f, dim.y, 12, c.hair);
    } else if (part.id == "hair_bob") {
        // A sunken crown with side spheres reaching down over the ears.
        DrawSphere({at.x, at.y - dim.y * 0.35f, at.z}, dim.x * 0.50f, c.hair);
        DrawSphere({at.x - dim.x * 0.42f, at.y - dim.y * 1.10f, at.z},
                   dim.x * 0.22f, c.hair);
        DrawSphere({at.x + dim.x * 0.42f, at.y - dim.y * 1.10f, at.z},
                   dim.x * 0.22f, c.hair);
    } else if (part.id == "hair_curls") {
        // A cluster of small spheres: a lower row of three, two on top.
        for (int i = -1; i <= 1; ++i) {
            DrawSphere({at.x + static_cast<float>(i) * dim.x * 0.30f,
                        at.y + dim.y * 0.05f, at.z},
                       dim.x * 0.24f, c.hair);
        }
        DrawSphere({at.x - dim.x * 0.15f, at.y + dim.y * 0.45f, at.z},
                   dim.x * 0.22f, c.hair);
        DrawSphere({at.x + dim.x * 0.15f, at.y + dim.y * 0.45f, at.z},
                   dim.x * 0.22f, c.hair);
    } else if (part.id == "hair_bun") {
        // A rounded cap with a bun perched top-back.
        DrawSphere({at.x, at.y - dim.y * 0.25f, at.z}, dim.x * 0.52f, c.hair);
        DrawSphere({at.x, at.y + dim.y * 0.55f, at.z - dim.z * 0.35f},
                   dim.x * 0.30f, c.hair);
    } else if (part.id == "hair_side") {
        // A slab swept to one side, with a longer fall down that side.
        DrawCube({at.x - dim.x * 0.12f, cy, at.z}, dim.x * 0.76f, dim.y,
                 dim.z, c.hair);
        DrawCube({at.x - dim.x * 0.45f, at.y - dim.y * 0.60f, at.z},
                 dim.x * 0.20f, dim.y * 1.60f, dim.z * 0.80f, c.hair);
    } else if (part.id == "hair_long") {
        // Crown plus a long fall down the back.
        DrawSphere({at.x, at.y - dim.y * 0.55f, at.z}, dim.x * 0.50f, c.hair);
        DrawCube({at.x, at.y - dim.y * 1.05f, at.z - dim.z * 0.42f},
                 dim.x * 0.82f, dim.y * 1.9f, dim.z * 0.22f, c.hair);
    } else if (part.id == "hair_afro") {
        // One big proud sphere.
        DrawSphere({at.x, at.y + dim.y * 0.20f, at.z}, dim.x * 0.52f, c.hair);
    } else if (part.id == "hair_braids") {
        // Crown with a stacked braid hanging at each side.
        DrawSphere({at.x, at.y - dim.y * 0.30f, at.z}, dim.x * 0.48f, c.hair);
        for (const float side : {-1.f, 1.f}) {
            DrawSphere({at.x + side * dim.x * 0.46f, at.y - dim.y * 0.95f, at.z},
                       dim.x * 0.15f, c.hair);
            DrawSphere({at.x + side * dim.x * 0.46f, at.y - dim.y * 1.35f, at.z},
                       dim.x * 0.12f, c.hair);
        }
    } else if (part.id == "hair_flat_top") {
        DrawCube({at.x, cy, at.z}, dim.x * 0.92f, dim.y, dim.z * 0.92f, c.hair);
    } else if (part.id == "hair_helmet") {
        // A shell sunk over the skull, open at the face.
        DrawCube({at.x, at.y - dim.y * 0.30f, at.z - dim.z * 0.06f},
                 dim.x, dim.y * 1.5f, dim.z * 0.88f, c.hair);
    } else if (part.id == "hair_wave") {
        // A slab with a lip swept up over the forehead.
        DrawCube({at.x, at.y - dim.y * 0.15f, at.z}, dim.x * 0.94f,
                 dim.y * 0.7f, dim.z * 0.94f, c.hair);
        DrawCube({at.x, at.y + dim.y * 0.45f, at.z + dim.z * 0.38f},
                 dim.x * 0.55f, dim.y * 0.8f, dim.z * 0.24f, c.hair);
    } else if (part.id == "eyes_sleepy") {
        // Two low lids. A closed lid is a lash line, not a hole, so it is
        // iris-toned rather than the old near-black.
        const float dx = dim.x * 0.5f - dim.y * 0.5f;
        DrawCube({at.x - dx, fy, fz}, dim.y * 1.6f, dim.y * 0.45f, dim.z, c.iris);
        DrawCube({at.x + dx, fy, fz}, dim.y * 1.6f, dim.y * 0.45f, dim.z, c.iris);
        drawBrows(dx);
    } else if (part.id == "eyes_wink") {
        // One open eye, one closed lid.
        const float dx = dim.x * 0.5f - dim.y * 0.5f;
        drawEye(at.x - dx, fy, fz, dim.y * 0.60f, -1.f);
        DrawCube({at.x + dx, fy, fz}, dim.y * 1.5f, dim.y * 0.4f, dim.z, c.iris);
        drawBrows(dx);
    } else if (part.id == "eyes_glasses") {
        // Round wire lenses: a dark disc a shade wider than the eye, with
        // the eye drawn ON TOP of it, leaves a rim. Solid frames in FRONT
        // of the eyes read as a burglar's mask, not as spectacles — the
        // frames keep `dark` because frames really are black plastic, but
        // only the rim of one may ever cover skin.
        const float dx = dim.x * 0.5f - dim.y * 0.55f;
        for (const float side : {-1.f, 1.f}) {
            const Vector3 lens{at.x + side * dx, fy, fz + dim.z * 0.05f};
            squashed(lens, 1.f, 1.f, 0.20f,
                     [&] { DrawSphere(lens, dim.y * 0.66f, c.dark); });
            drawEye(at.x + side * dx, fy, fz + dim.z * 0.12f, dim.y * 0.54f,
                    side);
        }
        DrawCube({at.x, fy, fz + dim.z * 0.10f}, dx * 0.9f, dim.y * 0.10f,
                 dim.z * 0.30f, c.dark);
        drawBrows(dx);
    } else if (part.id == "eyes_angry") {
        // Eyes under a heavy single brow bar — this branch already does the
        // brow job deliberately, so drawBrows stays out of it.
        const float dx = dim.x * 0.5f - dim.y * 0.5f;
        drawEye(at.x - dx, fy - dim.y * 0.15f, fz, dim.y * 0.52f, -1.f);
        drawEye(at.x + dx, fy - dim.y * 0.15f, fz, dim.y * 0.52f, 1.f);
        DrawCube({at.x, fy + dim.x * 0.24f, fz + dim.z * 0.10f}, dim.x * 0.94f,
                 dim.x * 0.09f, dim.z * 0.70f, c.hair);
    } else if (part.id == "mouth_smile") {
        // A wide low bar with raised end dots reads as an upturned smile.
        DrawCube({at.x, fy - dim.y * 0.20f, fz}, dim.x * 0.68f,
                 dim.y * 0.38f, dim.z, c.mouth);
        DrawSphere({at.x - dim.x * 0.42f, fy + dim.y * 0.15f, fz},
                   dim.y * 0.30f, c.mouth);
        DrawSphere({at.x + dim.x * 0.42f, fy + dim.y * 0.15f, fz},
                   dim.y * 0.30f, c.mouth);
    } else if (part.id == "mouth_open" || part.id == "mouth_o") {
        // A flattened sphere: tall ellipse (open) or small ring (o).
        rlPushMatrix();
        rlTranslatef(at.x, fy, fz);
        rlScalef(1.f, dim.y / dim.x, 0.30f);
        DrawSphere({0.f, 0.f, 0.f}, dim.x * 0.42f, c.mouth);
        rlPopMatrix();
    } else if (part.id == "mouth_neutral") {
        DrawCube({at.x, fy, fz}, dim.x, dim.y, dim.z, c.mouth);
    } else if (part.category == PartCategory::Eyes) {
        if (part.id == "eyes_visor") {
            // A visor is meant to read as opaque: no eyes, no brows.
            DrawCube({at.x, fy, fz}, dim.x, dim.y, dim.z, c.dark);
        } else {
            // Two eyes split across the part's declared width.
            const float dx = dim.x * 0.5f - dim.y * 0.5f;
            drawEye(at.x - dx, fy, fz, dim.y * 0.60f, -1.f);
            drawEye(at.x + dx, fy, fz, dim.y * 0.60f, 1.f);
            drawBrows(dx);
        }
    } else if (part.category == PartCategory::Mouth) {
        // Generic mouth: a thin bar, so family-specific mouth ids (e.g. the
        // quaternius-scale marks) render without a bespoke recipe branch
        // each.
        DrawCube({at.x, fy, fz}, dim.x, dim.y, dim.z, c.mouth);
    } else if (part.localSize.y > 0.f) {
        // Generic recipe: any part without a bespoke shape renders as
        // its declared box (skin for heads, hair on top, outfit below)
        // so NEW catalog parts appear immediately, just plainly.
        const Color color = part.category == PartCategory::Head ? c.skin
                            : part.category == PartCategory::Hair ? c.hair
                                                                  : c.outfit;
        DrawCube({at.x, cy, at.z}, dim.x, dim.y, dim.z, color);
    }
}

// Face-pick ids -> FaceTexture style indexes (issue #140). Unknown ids
// (a stored look from a newer catalog) fall back to style 0 rather than
// failing — same demote-gracefully rule looks follow everywhere.
int stylizedEyeStyle(const std::string& id) {
    const char* ids[FaceTexture::kEyeStyleCount] = {
        "eyes_q_dot",    "eyes_q_wide", "eyes_q_round", "eyes_q_happy",
        "eyes_q_sleepy", "eyes_q_star", "eyes_q_wink",  "eyes_q_glasses"};
    for (int i = 0; i < FaceTexture::kEyeStyleCount; ++i) {
        if (id == ids[i]) return i;
    }
    return 0;
}

int stylizedMouthStyle(const std::string& id) {
    const char* ids[FaceTexture::kMouthStyleCount] = {
        "mouth_q_smile", "mouth_q_line", "mouth_q_open", "mouth_q_o"};
    for (int i = 0; i < FaceTexture::kMouthStyleCount; ++i) {
        if (id == ids[i]) return i;
    }
    return 0;
}

// The flat face decal (issue #140): a HEAD-oriented textured quad spanning
// the face sockets — drawn inside the figure's matrix, so it turns with
// facingDeg and tips with the death pose instead of billboarding at the
// camera. Alpha-blended over the mesh face, a hair proud of the surface so
// depth keeps it in front.
void drawStylizedFace(const Texture2D& texture, const PartDef& head,
                      const Vec3& headAt, float s) {
    const auto eyes = head.sockets.find("eyes");
    const auto mouth = head.sockets.find("mouth");
    if (eyes == head.sockets.end() || mouth == head.sockets.end()) return;

    const float size = head.localSize.x * 0.72f * s;  // square decal
    // Anchor the CANVAS eye line (46/128 from the top = 18/128 above quad
    // center) to the head's eyes socket — the eye line is what reads, so
    // it must land exactly; the mouth follows the canvas's fixed anatomy.
    const float cy = headAt.y + eyes->second.y * s - size * (18.f / 128.f);
    // Proud of the face by a bit MORE than the wrap sweeps back, so the
    // outer strips end flush with the cheeks instead of buried in them.
    const float zFront = headAt.z +
                         std::max(eyes->second.z, mouth->second.z) * s +
                         size * 0.075f;

    // A flat plane "clings" to the near cheek at steep viewing angles
    // (parallax against the curved face), so the decal is a shallow
    // 3-strip wrap: full-front center third, outer thirds swept back.
    const float half = size * 0.5f;
    const float third = size / 3.f;
    const float sweep = size * 0.06f;
    const float xs[4] = {headAt.x - half, headAt.x - half + third,
                         headAt.x + half - third, headAt.x + half};
    const float zs[4] = {zFront - sweep, zFront, zFront, zFront - sweep};

    rlSetTexture(texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    for (int strip = 0; strip < 3; ++strip) {
        const float u0 = static_cast<float>(strip) / 3.f;
        const float u1 = static_cast<float>(strip + 1) / 3.f;
        rlNormal3f(0.f, 0.f, 1.f);  // near-frontal; banding follows the face
        rlTexCoord2f(u0, 1.f);
        rlVertex3f(xs[strip], cy - half, zs[strip]);
        rlTexCoord2f(u1, 1.f);
        rlVertex3f(xs[strip + 1], cy - half, zs[strip + 1]);
        rlTexCoord2f(u1, 0.f);
        rlVertex3f(xs[strip + 1], cy + half, zs[strip + 1]);
        rlTexCoord2f(u0, 0.f);
        rlVertex3f(xs[strip], cy + half, zs[strip]);
    }
    rlEnd();
    rlSetTexture(0);
}

// Mesh-backed part (issue #139): draws the part's resolved meshes with
// their bounds bottom-center on the assembly anchor — the same convention
// primitive recipes use — under the one contract scale. DrawMesh composes
// its transform with the surrounding rlgl matrix stack, so the figure's
// facing/death-tip matrix applies exactly as it does to primitives.
// `override` swaps in the outline material for the rim pass; nullptr draws
// the model's own (cel-routed) materials.
void drawPartMeshes(const Assets::PartMeshes& pm, const Vec3& at, float s,
                    const Material* override) {
    const float cx = (pm.boundsMin.x + pm.boundsMax.x) * 0.5f;
    const float cz = (pm.boundsMin.z + pm.boundsMax.z) * 0.5f;
    const Matrix m = MatrixMultiply(
        MatrixMultiply(MatrixTranslate(-cx, -pm.boundsMin.y, -cz),
                       MatrixScale(s, s, s)),
        MatrixTranslate(at.x, at.y, at.z));
    for (const int idx : pm.meshes) {
        const Material& material =
            override ? *override
                     : pm.model->materials[pm.model->meshMaterial[idx]];
        DrawMesh(pm.model->meshes[idx], material, m);
    }
}

}  // namespace

void RaylibRenderer::drawCompositeCharacter(const CharacterLook& look,
                                            const Vec3& position, float facingDeg,
                                            bool walking, float phase, NpcFace face,
                                            bool dead) {
    const AssembledLook assembled = assembleLook(look);
    if (!assembled.ok) {
        // Invalid/stale look: the same marker cylinder the pack path uses.
        DrawCylinder({position.x, 0.f, position.z}, 0.35f, 0.35f, 1.8f, 12,
                     Color{200, 120, 80, 255});
        return;
    }
    // ONE uniform scale takes the whole assembly to the same height
    // contract pack characters use — parts were snapped in local space, so
    // proportions and socket alignment survive any future contract change.
    const float s = 1.8f / assembled.height;

    const PartPalette* palette = &paletteCatalog().front();
    for (const PartPalette& candidate : paletteCatalog()) {
        if (candidate.id == look.paletteId) palette = &candidate;
    }
    const Color skin{palette->skin[0], palette->skin[1], palette->skin[2], 255};
    const RecipeColors colors{
        skin,
        deepenSkin(skin),
        Color{palette->hair[0], palette->hair[1], palette->hair[2], 255},
        Color{palette->outfit[0], palette->outfit[1], palette->outfit[2], 255},
        Color{palette->pants[0], palette->pants[1], palette->pants[2], 255},
        kPlastic, kShoe,   kSclera, kIris,
        kSpark,   kMouthTone, kBlush};

    // Whole-figure transform: facing + procedural walk bob. Parts then draw
    // at their assembly-local positions and rotate correctly for free.
    // Death pose: the figure tips onto its back (a rigid tip-over, the
    // composite counterpart of the pack models' death clip), lifted a
    // little so the torso doesn't sink through the ground slab.
    // Mesh heads wear a flat face DECAL (issue #140) instead of primitive
    // eye/mouth marks, and their mood lives ON that face — no emote
    // billboard for this family.
    const PartDef* headPart = findPart(look.part(PartCategory::Head));
    const bool meshFace = headPart && !headPart->meshName.empty() &&
                          assets_.partMeshes(headPart->meshName) != nullptr;

    // Tier-B locomotion (issue #142): CPU-skin each modular file this
    // figure draws from (body and head may come from different files) to
    // Idle or Walk at this figure's clock. Files share one animation
    // library, so a mixed head keeps riding its neck. The dead freeze at
    // Idle frame 0 under the rigid tip-over; rigged clips replace the
    // procedural bob for mesh figures (bob stays for core primitives).
    std::string stems[2];
    int stemCount = 0;
    for (const PlacedPart& placed : assembled.parts) {
        if (placed.part->meshName.empty()) continue;
        const std::string stem =
            placed.part->meshName.substr(0, placed.part->meshName.find(':'));
        const bool seen = stemCount > 0 && (stems[0] == stem ||
                                            (stemCount > 1 && stems[1] == stem));
        if (!seen && stemCount < 2) stems[stemCount++] = stem;
    }
    for (int i = 0; i < stemCount; ++i) {
        assets_.poseModular(stems[i], walking && !dead, dead ? 0.f : phase);
    }
    const bool meshFigure = stemCount > 0;
    const float bob = (walking && !dead && !meshFigure)
                          ? std::fabs(std::sin(phase * 7.f)) * 0.05f
                          : 0.f;

    // Character surfaces band through the cel shader (issue #138); the
    // frame's fog shader is restored before the emote billboard below.
    beginCharacterShader();
    rlPushMatrix();
    rlTranslatef(position.x, position.y + bob + (dead ? 0.30f : 0.f), position.z);
    rlRotatef(facingDeg, 0.f, 1.f, 0.f);
    if (dead) rlRotatef(-90.f, 1.f, 0.f, 0.f);

    // Cartoon outline, classic inverted hull (issue #103): every recipe is
    // issued TWICE — first inflated about its part's center, near-black,
    // with FRONT faces culled (so only a rim of the enlarged copy survives
    // around the real geometry), then normally. The passes are grouped —
    // not interleaved per part — because the cull-face switch must drain
    // the rlgl batch; two drains per character instead of two per part.
    // The hull is thin: at 1.06 the rim was roughly a centimetre of ink
    // around every part at world scale, which cracked the figure into
    // pieces instead of drawing it.
    constexpr float kHullScale = 1.035f;
    const RecipeColors outlineColors = flatColors(kOutlineColor);
    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_FRONT);
    for (const PlacedPart& placed : assembled.parts) {
        // Outlines belong on the silhouette, not on the features: ringing
        // every eye and mouth is what produced the webbed, cracked look.
        // (Face-decal families have no eye/mouth geometry to hull either.)
        if (placed.part->category == PartCategory::Eyes ||
            placed.part->category == PartCategory::Mouth) {
            continue;
        }
        const Vec3 at = placed.position * s;
        // Mesh-backed parts rim through the normal-inflate outline shader
        // (issue #138's mesh technique) — the scale hull is for primitive
        // recipes, whose shapes have no per-vertex normals to inflate by.
        if (!placed.part->meshName.empty()) {
            if (const Assets::PartMeshes* pm =
                    assets_.partMeshes(placed.part->meshName)) {
                if (const Material* rim = assets_.outlineMaterial()) {
                    drawPartMeshes(*pm, at, s, rim);
                }
                continue;
            }
            // Unresolved mesh: its fallback box outlines like any recipe.
        }
        const Vec3 dim = placed.part->localSize * s;
        const float cy = at.y + dim.y * 0.5f;
        rlPushMatrix();  // inflate about the part's center, not the feet
        rlTranslatef(at.x, cy, at.z);
        rlScalef(kHullScale, kHullScale, kHullScale);
        rlTranslatef(-at.x, -cy, -at.z);
        drawPartRecipe(*placed.part, at, dim, outlineColors);
        rlPopMatrix();
    }
    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_BACK);

    for (const PlacedPart& placed : assembled.parts) {
        // Anchor and size in world units — the contract scale is applied
        // HERE so recipes never see it. Mesh-backed parts dispatch to
        // their resolved meshes (issue #139); a part whose mesh didn't
        // resolve draws its declared box so the figure never has holes.
        const Vec3 at = placed.position * s;
        if (meshFace && (placed.part->category == PartCategory::Eyes ||
                         placed.part->category == PartCategory::Mouth)) {
            continue;  // the face decal below carries eyes, mouth, AND mood
        }
        if (!placed.part->meshName.empty()) {
            if (const Assets::PartMeshes* pm =
                    assets_.partMeshes(placed.part->meshName)) {
                drawPartMeshes(*pm, at, s, nullptr);
                continue;
            }
        }
        drawPartRecipe(*placed.part, at, placed.part->localSize * s, colors);
    }
    if (meshFace) {
        for (const PlacedPart& placed : assembled.parts) {
            if (placed.part->category != PartCategory::Head) continue;
            drawStylizedFace(
                assets_.stylizedFace(
                    stylizedEyeStyle(look.part(PartCategory::Eyes)),
                    stylizedMouthStyle(look.part(PartCategory::Mouth)),
                    dead ? NpcFace::Neutral : face),
                *placed.part, placed.position * s, s);
        }
    }
    rlPopMatrix();
    endCharacterShader();

    // Emote billboard for CORE-family looks only, same height as the pack
    // path draws it. Mesh-face families retired it (issue #140): their
    // mood is already on the face, so a floating copy would double-report.
    // The dead don't emote.
    if (!dead && !meshFace && face != NpcFace::Neutral) {
        DrawBillboard(camera_, assets_.faceTexture(face),
                      Vector3{position.x, 2.45f, position.z}, 0.55f, WHITE);
    }
}

void RaylibRenderer::setAvatarPalette(const std::string& paletteId) {
    for (const PartPalette& palette : paletteCatalog()) {
        if (palette.id != paletteId) continue;
        avatarSkin_ = Color{palette.skin[0], palette.skin[1], palette.skin[2], 255};
        avatarOutfit_ =
            Color{palette.outfit[0], palette.outfit[1], palette.outfit[2], 255};
        return;
    }
}

bool RaylibRenderer::screenToGround(Vector2 screen, Vec3& out) const {
    const Ray ray = GetMouseRay(screen, camera_);
    if (ray.direction.y >= -1e-4f) return false;  // parallel or skyward
    const float t = -ray.position.y / ray.direction.y;
    out = Vec3{ray.position.x + ray.direction.x * t, 0.f,
               ray.position.z + ray.direction.z * t};
    return true;
}

void RaylibRenderer::drawPlacementGhost(const Building& building, bool valid) {
    const Color tint = valid ? Color{120, 230, 140, 140} : Color{235, 90, 90, 140};
    if (const Model* model = assets_.modelForBuilding(building)) {
        const Assets::SizeSpec& spec = assets_.sizeSpecFor(building);
        if (spec.mode == Assets::SizeSpec::Mode::Uniform) {
            drawModelUniform(*model, (building.minX + building.maxX) * 0.5f,
                             (building.minZ + building.maxZ) * 0.5f,
                             spec.worldHeight, tint);
        } else {
            drawModelFittedToAABB(*model, building.minX, building.minZ,
                                  building.maxX, building.maxZ, building.height,
                                  tint);
        }
    }
    // The footprint outline always draws, even model-less, so the grid
    // cell the piece will claim is unmistakable.
    const Vector3 center{(building.minX + building.maxX) * 0.5f,
                         building.height * 0.5f,
                         (building.minZ + building.maxZ) * 0.5f};
    DrawCubeWires(center, building.maxX - building.minX, building.height,
                  building.maxZ - building.minZ, tint);
}

void RaylibRenderer::drawViewmodel(int weaponKind, float attackFraction) {
    if (weaponKind < 0 || weaponKind > 1) return;
    // The fist is invisible at rest — nothing floats in front of the
    // camera — and only appears as a punch while the swing plays out.
    const bool fist = weaponKind == 0;
    if (fist && attackFraction <= 0.01f) return;

    // Everything below draws in CAMERA-LOCAL space: translate to the eye,
    // rotate by the camera's yaw and pitch, then draw axis-aligned shapes
    // with +z forward. The prop therefore pivots with the view instead of
    // reading as a world-axis-aligned floating block. In this local frame
    // +x is screen-LEFT (right-handed, y-up), so right-of-center offsets
    // are negative x.
    const Vector3 fwd{camera_.target.x - camera_.position.x,
                      camera_.target.y - camera_.position.y,
                      camera_.target.z - camera_.position.z};
    const float flen = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (flen < 1e-4f) return;
    const Vector3 f{fwd.x / flen, fwd.y / flen, fwd.z / flen};
    const float yawDeg = std::atan2(f.x, f.z) * RAD2DEG;
    const float pitchDeg = std::asin(std::max(-1.f, std::min(1.f, f.y))) * RAD2DEG;

    // The avatar's palette colors the arm and hand — the one first-person
    // surface where the player sees their own look (issue #106).
    const Color skin = avatarSkin_;
    const Color sleeve = avatarOutfit_;
    const Color gunmetal{55, 58, 66, 255};
    const Color gripTone{38, 40, 46, 255};

    // The first-person arm is a character surface too — same cel banding
    // as the third-person figures (issue #138 routing rule).
    beginCharacterShader();
    rlPushMatrix();
    rlTranslatef(camera_.position.x, camera_.position.y, camera_.position.z);
    rlRotatef(yawDeg, 0.f, 1.f, 0.f);
    rlRotatef(-pitchDeg, 1.f, 0.f, 0.f);

    // attackFraction runs 1 → 0 across the swing; sin turns that into
    // extend-then-retract for the fist. Pistol recoil (back and muzzle-up)
    // is a pose transform, so it applies to rim and color passes alike.
    const float ext = std::sin(attackFraction * PI);
    const float reach = 0.34f + 0.55f * ext;
    if (!fist) {
        rlTranslatef(0.f, 0.f, -0.07f * attackFraction);
        rlRotatef(-7.f * attackFraction, 1.f, 0.f, 0.f);
    }

    // One prop, two palettes: the rim pass re-issues every shape through
    // this inflated about ITS OWN center (the same per-part pivots the
    // body hull uses — a single whole-prop pivot leaves the near side of
    // each shape rimless).
    const auto drawProp = [&](Color sleeveC, Color skinC, Color metalC,
                              Color gripC, float inflate) {
        const auto shape = [&](Vector3 center, auto&& draw) {
            if (inflate > 1.f) {
                rlPushMatrix();
                rlTranslatef(center.x, center.y, center.z);
                rlScalef(inflate, inflate, inflate);
                rlTranslatef(-center.x, -center.y, -center.z);
            }
            draw();
            if (inflate > 1.f) rlPopMatrix();
        };
        if (fist) {
            // Sleeve forearm rising slightly toward screen center, fist at
            // the end. Local vectors — the matrix above carries them to
            // the world.
            const Vector3 fistAt{-0.22f, -0.30f + 0.08f * ext, reach};
            shape(Vector3{-0.24f, -0.35f + 0.04f * ext, (0.16f + reach) * 0.5f},
                  [&] {
                      DrawCylinderEx(Vector3{-0.26f, -0.40f, 0.16f}, fistAt,
                                     0.055f, 0.045f, 10, sleeveC);
                  });
            shape(fistAt, [&] { DrawSphere(fistAt, 0.075f, skinC); });
        } else {
            // Pistol: held silhouette (slide + barrel + grip + hand).
            shape(Vector3{-0.26f, -0.26f, 0.50f}, [&] {
                DrawCube(Vector3{-0.26f, -0.26f, 0.50f}, 0.045f, 0.075f, 0.26f,
                         metalC);
            });
            shape(Vector3{-0.26f, -0.25f, 0.675f}, [&] {
                DrawCylinderEx(Vector3{-0.26f, -0.25f, 0.60f},
                               Vector3{-0.26f, -0.25f, 0.75f}, 0.018f, 0.018f, 10,
                               metalC);
            });
            shape(Vector3{-0.26f, -0.345f, 0.44f}, [&] {
                DrawCube(Vector3{-0.26f, -0.345f, 0.44f}, 0.04f, 0.11f, 0.055f,
                         gripC);
            });
            shape(Vector3{-0.26f, -0.31f, 0.43f}, [&] {
                DrawSphere(Vector3{-0.26f, -0.31f, 0.43f}, 0.042f, skinC);  // hand
            });
        }
    };

    // Cartoon rim (issue #143 gate): the same #103 inverted hull every
    // third-person figure gets — grouped passes, batch drained around the
    // cull switches. The rim is proportionally larger than the body's
    // (1.14 vs 1.06) because the prop sits half a meter from the eye.
    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_FRONT);
    drawProp(kOutlineColor, kOutlineColor, kOutlineColor, kOutlineColor, 1.14f);
    rlDrawRenderBatchActive();
    rlSetCullFace(RL_CULL_FACE_BACK);

    drawProp(sleeve, skin, gunmetal, gripTone, 1.f);
    rlPopMatrix();
    endCharacterShader();
}

void RaylibRenderer::endFrame() {
    if (assets_.fogShader()) EndShaderMode();
    EndMode3D();
}

bool RaylibRenderer::worldToScreen(const Vec3& world, Vector2& out) const {
    // Reject points behind the camera: GetWorldToScreen would mirror them.
    const Vector3 toPoint{world.x - camera_.position.x, world.y - camera_.position.y,
                          world.z - camera_.position.z};
    const Vector3 forward{camera_.target.x - camera_.position.x,
                          camera_.target.y - camera_.position.y,
                          camera_.target.z - camera_.position.z};
    if (toPoint.x * forward.x + toPoint.y * forward.y + toPoint.z * forward.z <= 0.f) {
        return false;
    }
    out = GetWorldToScreen({world.x, world.y, world.z}, camera_);
    return true;
}

}  // namespace llm_npc
