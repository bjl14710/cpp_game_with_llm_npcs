#include "RaylibRenderer.hpp"

#include <algorithm>
#include <cmath>

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

void RaylibRenderer::beginFrame(const CameraPose& pose) {
    camera_.position = {pose.position.x, pose.position.y + 1.7f, pose.position.z};
    const float pitchRad = degToRad(pose.pitchDeg);
    const float yawRad = degToRad(pose.yawDeg);
    const Vector3 dir{std::sin(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                      std::cos(yawRad) * std::cos(pitchRad)};
    camera_.target = {camera_.position.x + dir.x, camera_.position.y + dir.y,
                      camera_.position.z + dir.z};
    camera_.up = {0.f, 1.f, 0.f};
    camera_.fovy = 70.f;
    camera_.projection = CAMERA_PERSPECTIVE;
    BeginMode3D(camera_);
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
    if (straight && crossing && junction) {
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
    } else {
        for (const float c : {-kStreetCenter, kStreetCenter}) {
            DrawCube({c, 0.02f, 0.f}, kStreetWidth, 0.04f, half * 2.f, kAsphalt);
            DrawCube({0.f, 0.02f, c}, half * 2.f, 0.04f, kStreetWidth, kAsphalt);
        }
    }
    // Block slabs: plaza (center) is pale stone, park (north-east) stays
    // grass, every other block gets a sidewalk-toned slab.
    for (const float bx : {-64.f, 0.f, 64.f}) {
        for (const float bz : {-64.f, 0.f, 64.f}) {
            const bool plaza = bx == 0.f && bz == 0.f;
            const bool park = bx == 64.f && bz == 64.f;
            if (park) continue;
            DrawCube({bx, 0.04f, bz}, 48.f, 0.08f, 48.f, plaza ? kPlaza : kSidewalk);
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
        if (b.id == "fountain") {
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

    // Non-neutral moods float as an emote above the head — the same six
    // procedural faces the legacy renderer painted on, now billboarded.
    // The dead don't emote.
    if (!visual.dead && visual.face != NpcFace::Neutral) {
        DrawBillboard(camera_, assets_.faceTexture(visual.face),
                      Vector3{visual.position.x, 2.45f, visual.position.z}, 0.55f,
                      WHITE);
    }
}

void RaylibRenderer::drawViewmodel(int weaponKind, float attackFraction) {
    // Per-weapon visuals live in this table; adding a WeaponKind means
    // adding a row, never a new code branch. Sizes are camera-space meters.
    struct ViewSpec {
        Vector3 size;        // main body (fist / gun body)
        Color color;
        float thrust;        // + pushes forward on attack (punch), - recoils
        bool grip;           // draw a pistol-style grip under the body
    };
    static const ViewSpec kSpecs[] = {
        {{0.11f, 0.09f, 0.14f}, Color{224, 172, 138, 255}, 0.45f, false},  // Fist
        {{0.05f, 0.07f, 0.30f}, Color{55, 58, 66, 255}, -0.14f, true},     // Pistol
    };
    const int count = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));
    if (weaponKind < 0 || weaponKind >= count) return;
    const ViewSpec& spec = kSpecs[weaponKind];

    // Camera basis for a lower-right anchored prop.
    const Vector3 fwd{camera_.target.x - camera_.position.x,
                      camera_.target.y - camera_.position.y,
                      camera_.target.z - camera_.position.z};
    const float flen = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (flen < 1e-4f) return;
    const Vector3 f{fwd.x / flen, fwd.y / flen, fwd.z / flen};
    // Same right-vector convention as the game's strafe (flatRight in
    // main.cpp): cross(forward, up) = (-fz, 0, fx).
    const Vector3 r{-f.z, 0.f, f.x};

    const float push = spec.thrust * attackFraction;
    const Vector3 base{
        camera_.position.x + f.x * (0.85f + push) + r.x * 0.32f,
        camera_.position.y - 0.28f + f.y * (0.85f + push),
        camera_.position.z + f.z * (0.85f + push) + r.z * 0.32f};

    DrawCubeV(base, spec.size, spec.color);
    if (spec.grip) {
        DrawCubeV(Vector3{base.x - f.x * 0.10f, base.y - 0.07f, base.z - f.z * 0.10f},
                  Vector3{0.045f, 0.10f, 0.06f}, spec.color);
    }
}

void RaylibRenderer::endFrame() { EndMode3D(); }

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
