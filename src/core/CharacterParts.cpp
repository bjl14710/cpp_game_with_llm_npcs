#include "CharacterParts.hpp"

#include <algorithm>
#include <cmath>

#include "json.hpp"

namespace llm_npc {

namespace {

// Tiny deterministic PRNG (xorshift32) so randomizeLook(seed) is stable
// across platforms — std::mt19937 would work too, but this keeps the
// contract obvious and header-free.
struct Rng {
    unsigned state;
    unsigned next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    // Uniform pick in [0, n).
    std::size_t pick(std::size_t n) { return n ? next() % n : 0; }
};

// Tag-level rule shared by styleCompatible and partsForCategory. The
// quaternius mesh family (issue #139) is EXCLUSIVE: its parts are authored
// in the pack's real glTF units (heads ~0.29 tall vs core's ~1.0), so the
// "any" bridge tag deliberately does not reach it — a core primitive bowl
// cut or Mii-scale eyes on a mesh head would be wildly out of scale.
bool tagsCompatible(const std::string& a, const std::string& b) {
    if (a == "quaternius" || b == "quaternius") return a == b;
    return a == "any" || b == "any" || a == b;
}

// The pack new random looks are generated from. Issue #139 pointed this at
// the quaternius mesh family; the appealing-character pass points it back
// at "core", because that is the family the redesign (limbs, sclera+iris
// eyes, brows, warm outline ink) landed on. The mesh family stays in the
// catalog — stored looks keep validating and the creator can still cycle
// onto it deliberately.
constexpr const char* kDefaultBodyPack = "core";

}  // namespace

const std::vector<CategorySpec>& categorySpecs() {
    // Body is the root; everything else names its parent and socket. Adding
    // a category (e.g. Accessory on the body's "hand" socket) is one row.
    static const std::vector<CategorySpec> specs = {
        {PartCategory::Body, PartCategory::Body, ""},
        {PartCategory::Head, PartCategory::Body, "head"},
        {PartCategory::Eyes, PartCategory::Head, "eyes"},
        {PartCategory::Hair, PartCategory::Head, "hair"},
        {PartCategory::Mouth, PartCategory::Head, "mouth"},
    };
    return specs;
}

float skullSurfaceZ(const PartDef& head, float y) {
    const float halfDepth = head.localSize.z * 0.5f;
    if (head.styleTag != "round") return halfDepth;  // box face, or a mesh's bounds
    // The round recipes draw a sphere sized to the declared box and centred
    // at half its height, so the silhouette is that ellipsoid: off the
    // equator the surface pulls back, and hard — head_round loses a tenth of
    // its depth between the eye line and the mouth line.
    const float halfHeight = head.localSize.y * 0.5f;
    if (halfHeight <= 0.f) return halfDepth;
    const float t = (y - halfHeight) / halfHeight;
    if (t <= -1.f || t >= 1.f) return 0.f;
    return halfDepth * std::sqrt(1.f - t * t);
}

float eyeDropY(const PartDef& eyes) {
    const float h = eyes.localSize.y;
    if (!eyes.meshName.empty()) return h * 0.5f;  // a decal, not marks
    // One branch per recipe that does NOT draw the shared pair of balls.
    if (eyes.id == "eyes_sleepy") return h * kLidHalfHeight;
    if (eyes.id == "eyes_visor") return h * 0.5f;  // draws its declared box
    // The spectacle DISC is what reaches lowest, not the eye behind it.
    if (eyes.id == "eyes_glasses") return h * kLensRadius;
    if (eyes.id == "eyes_angry")
        return h * (kAngryEyeDrop + kAngryEyeRadius * kEyeBallSquashY);
    return h * kEyeBallRadius * kEyeBallSquashY;
}

float mouthRiseY(const PartDef& mouth) {
    const float h = mouth.localSize.y;
    if (!mouth.meshName.empty()) return h * 0.5f;
    // The smile's end dots are RAISED off its bar — the bar itself sits low.
    if (mouth.id == "mouth_smile") return h * (kSmileDotRise + kSmileDotRadius);
    if (mouth.id == "mouth_open" || mouth.id == "mouth_o")
        return h * kMouthDiscRadius;
    return h * 0.5f;  // a bar drawn in its declared box
}

const std::vector<PartDef>& partCatalog() {
    // All positions are in each part's own local space, anchor at
    // bottom-center. Two style families plus "any" parts; sockets are
    // authored per part — a wider head simply declares its eye socket
    // farther forward, and every eye part inherits that for free.
    // Proportions follow the appealing-character pass (design handoff
    // "Game character visual improvements"): the body box is tall enough
    // to hold legs, so a head reads as ~a third of the body+head
    // silhouette (head.y / (headSocket.y + head.y) in 0.32-0.39, pinned by
    // test_style_pack.cpp) — still cartoon-large, no longer a head on a
    // cone. Eye sockets sit at ~46% of head height and mouths at ~28%:
    // eyes ABOVE a head's midline read adult, then alien. The 1.8u
    // contract scale normalizes overall height, so these ratios — not
    // absolute sizes — are what changes on screen.
    static const std::vector<PartDef> parts = {
        // Bodies (declare where a head sits on THEM). localSize.y spans
        // shoes -> shoulders; the recipe bands legs, torso and neck inside
        // it (see drawPartRecipe), so the head socket sits just under the
        // box top where the neck ends.
        {"body_round", PartCategory::Body, "round", {0.92f, 1.90f, 0.54f},
         {{"head", {0.f, 1.87f, 0.f}}}},
        {"body_block", PartCategory::Body, "blocky", {0.86f, 2.00f, 0.48f},
         {{"head", {0.f, 1.97f, 0.f}}}},
        {"body_slim", PartCategory::Body, "round", {0.72f, 2.10f, 0.46f},
         {{"head", {0.f, 2.07f, 0.f}}}},
        {"body_bulk", PartCategory::Body, "blocky", {1.06f, 1.95f, 0.62f},
         {{"head", {0.f, 1.92f, 0.f}}}},
        {"body_pear", PartCategory::Body, "round", {0.98f, 1.85f, 0.56f},
         {{"head", {0.f, 1.82f, 0.f}}}},
        {"body_broad", PartCategory::Body, "blocky", {1.14f, 1.90f, 0.58f},
         {{"head", {0.f, 1.87f, 0.f}}}},
        // Heads (declare where eyes, hair — and later mouths — sit on THEM).
        // A feature socket is the exact LINE the feature is drawn on, not
        // the corner of a box, so eyes.y IS the eye centre: ~48% of head
        // height on every skull. Eyes above a head's midline read adult,
        // then alien.
        // The two feature LINES are spaced, not authored independently: the
        // eyes and the mouth are both drawn between them, so the room they
        // need is what sets the distance —
        //     mouth.y = eyes.y - (maxEyeDropY + maxMouthRiseY + kFaceGapMin)
        // over every part the head's styleTag admits, which comes to 0.274
        // and is spaced at 0.275. That distance is ABSOLUTE, not a fraction
        // of the head: one shared pool of eyes and mouths hangs on all four
        // skulls at one size, so a taller head simply gets to carry its
        // mouth higher (20% of its height on head_round, 24% on head_tall)
        // rather than every head repeating the same ratio.
        // The appealing-character pass is what made this bite: it grew the
        // eyes — a spectacle lens is a third of a head-height across — while
        // leaving the sockets 0.18 apart, so 30 of the 120 combinations drew
        // the mouth INSIDE the eyes and 70 left no skin worth the name, with
        // the nose bead lost between them. Three shipped residents wore one
        // (barista, hotdog, tourist). Same class of defect as the mouth-z
        // muzzle below: a number authored once, and a neighbour moved.
        // eyes.z sits a little INSIDE the skull surface at that line (~88%
        // of the way out) so the flattened sclera reads as an eye set in a
        // face rather than a ball stuck on one. It is pulled in less on the
        // round skulls than the handoff table proposed: at 0.35 the small
        // eye styles (dot, happy, angry) disappeared inside the head, since
        // they build their radius from their own declared height.
        // mouth.z is DERIVED, and has to be RE-derived whenever the mouth
        // line moves, because a sphere is narrower further down. A mouth is a
        // mark ON skin, so its front face belongs level with
        // skullSurfaceZ(head, mouth.y):
        //     mouth.z = surface - localSize.z * (kFeatureZPush + 0.5)
        //                       + kMarkStandoff
        // The middle term (0.068 for every core mouth) is how far past the
        // socket a mouth recipe reaches — every one of them lands its front
        // at dim.z*0.5, which is exactly what makes ONE z per skull correct
        // for all four. Neither neighbour shares this rule: the eyes are sunk
        // to ~0.82 of the surface (a sclera is a ball that needs a socket)
        // and the nose bead is proud by ~0.009 (a nose sticks out).
        // Skipping the re-derivation is what put a muzzle on these two:
        // authored in #104 against a line of 0.30/0.34 and left alone when
        // the appealing-character pass moved it to 0.27/0.31.
        // The blocky pair below do NOT follow this rule and should not: their
        // face is one flat plane, so a mark floating off it still reads as
        // painted on, and sinking it to the plane would z-fight instead.
        {"head_round", PartCategory::Head, "round", {0.98f, 0.98f, 0.98f},
         {{"eyes", {0.f, 0.47f, 0.40f}},
          {"hair", {0.f, 0.88f, 0.f}},
          {"mouth", {0.f, 0.195f, 0.3333f}}}},
        {"head_block", PartCategory::Head, "blocky", {1.00f, 1.00f, 1.00f},
         {{"eyes", {0.f, 0.48f, 0.44f}},
          {"hair", {0.f, 0.95f, 0.f}},
          {"mouth", {0.f, 0.205f, 0.50f}}}},
        {"head_oval", PartCategory::Head, "round", {0.90f, 1.10f, 0.90f},
         {{"eyes", {0.f, 0.53f, 0.36f}},
          {"hair", {0.f, 1.02f, 0.f}},
          {"mouth", {0.f, 0.255f, 0.3218f}}}},  // derived — see head_round
        {"head_tall", PartCategory::Head, "blocky", {0.94f, 1.16f, 0.94f},
         {{"eyes", {0.f, 0.55f, 0.42f}},
          {"hair", {0.f, 1.10f, 0.f}},
          {"mouth", {0.f, 0.275f, 0.47f}}}},
        // Eyes, sized against the grown heads (recipes split pupils by the
        // declared width; the visor renders its declared box).
        {"eyes_dot", PartCategory::Eyes, "any", {0.42f, 0.13f, 0.10f}, {}},
        {"eyes_wide", PartCategory::Eyes, "round", {0.56f, 0.20f, 0.10f}, {}},
        {"eyes_visor", PartCategory::Eyes, "blocky", {0.72f, 0.18f, 0.10f}, {}},
        {"eyes_round", PartCategory::Eyes, "round", {0.54f, 0.25f, 0.10f}, {}},
        {"eyes_happy", PartCategory::Eyes, "any", {0.50f, 0.15f, 0.10f}, {}},
        {"eyes_sleepy", PartCategory::Eyes, "any", {0.48f, 0.10f, 0.10f}, {}},
        {"eyes_star", PartCategory::Eyes, "round", {0.52f, 0.22f, 0.10f}, {}},
        {"eyes_wink", PartCategory::Eyes, "any", {0.50f, 0.16f, 0.10f}, {}},
        {"eyes_glasses", PartCategory::Eyes, "any", {0.66f, 0.24f, 0.10f}, {}},
        {"eyes_angry", PartCategory::Eyes, "blocky", {0.56f, 0.16f, 0.10f}, {}},
        // Hair, scaled to crown the bigger heads. "hair_none" is a real
        // part with zero size so the assembly never has holes and the
        // picker has an explicit bald option.
        {"hair_none", PartCategory::Hair, "any", {0.f, 0.f, 0.f}, {}},
        {"hair_tuft", PartCategory::Hair, "round", {0.40f, 0.34f, 0.40f}, {}},
        {"hair_bowl", PartCategory::Hair, "any", {1.04f, 0.36f, 1.04f}, {}},
        {"hair_spikes", PartCategory::Hair, "blocky", {0.90f, 0.38f, 0.90f}, {}},
        {"hair_pony", PartCategory::Hair, "round", {0.44f, 0.38f, 0.44f}, {}},
        {"hair_mohawk", PartCategory::Hair, "blocky", {0.34f, 0.52f, 0.80f}, {}},
        {"hair_cap", PartCategory::Hair, "any", {0.80f, 0.28f, 1.04f}, {}},
        {"hair_buzz", PartCategory::Hair, "any", {0.80f, 0.15f, 0.80f}, {}},
        {"hair_bob", PartCategory::Hair, "round", {1.00f, 0.44f, 1.00f}, {}},
        {"hair_curls", PartCategory::Hair, "round", {0.88f, 0.38f, 0.88f}, {}},
        {"hair_bun", PartCategory::Hair, "round", {0.60f, 0.50f, 0.60f}, {}},
        {"hair_side", PartCategory::Hair, "blocky", {1.02f, 0.30f, 0.94f}, {}},
        {"hair_long", PartCategory::Hair, "round", {1.02f, 0.60f, 0.98f}, {}},
        {"hair_afro", PartCategory::Hair, "round", {1.10f, 0.70f, 1.10f}, {}},
        {"hair_braids", PartCategory::Hair, "round", {0.90f, 0.50f, 0.90f}, {}},
        {"hair_flat_top", PartCategory::Hair, "blocky", {0.95f, 0.42f, 0.95f}, {}},
        {"hair_helmet", PartCategory::Hair, "blocky", {1.06f, 0.40f, 1.06f}, {}},
        {"hair_wave", PartCategory::Hair, "any", {0.98f, 0.36f, 1.00f}, {}},
        // Mouths (issue #104) — simple Tomodachi-style marks; all "any"
        // so every face can wear every expression.
        {"mouth_smile", PartCategory::Mouth, "any", {0.30f, 0.10f, 0.08f}, {}},
        {"mouth_open", PartCategory::Mouth, "any", {0.26f, 0.18f, 0.08f}, {}},
        {"mouth_neutral", PartCategory::Mouth, "any", {0.28f, 0.06f, 0.08f}, {}},
        {"mouth_o", PartCategory::Mouth, "any", {0.16f, 0.16f, 0.08f}, {}},
        // ---- Quaternius mesh family (issue #139) ----
        // Everything below is in the pack's real glTF units, MEASURED from
        // the POSITION accessors (never eyeballed, never hand-scaled): a
        // body is the union of the file's Body+Legs+Feet nodes (~1.55
        // tall, x spans the A-pose arms), a head is its Head node. The
        // shared skeleton puts every neck at y=1.538, so all four bodies
        // declare the same head socket and heads mix across bodies. Heads
        // ship with their hair baked in ("hair_q_baked" keeps the Hair
        // category filled without drawing a second haircut); eyes and
        // mouths are primitive marks sized to these heads until the flat
        // face textures land (issue #140). The 1.8u contract scale
        // normalizes the unit difference from core exactly as designed.
        {"body_q_adventurer", PartCategory::Body, "quaternius",
         {1.650f, 1.570f, 0.460f}, {{"head", {0.f, 1.538f, 0.f}}},
         "quaternius", "Adventurer:Body+Legs+Feet"},
        {"body_q_scifi", PartCategory::Body, "quaternius",
         {1.650f, 1.542f, 0.314f}, {{"head", {0.f, 1.538f, 0.f}}},
         "quaternius", "SciFi:Body+Legs+Feet"},
        {"body_q_soldier", PartCategory::Body, "quaternius",
         {1.650f, 1.549f, 0.313f}, {{"head", {0.f, 1.538f, 0.f}}},
         "quaternius", "Soldier:Body+Legs+Feet"},
        {"body_q_witch", PartCategory::Body, "quaternius",
         {1.650f, 1.541f, 0.391f}, {{"head", {0.f, 1.538f, 0.f}}},
         "quaternius", "Witch:Body+Legs+Feet"},
        // Heads: anchor at the measured bounds' bottom-center. The witch's
        // hair hangs 0.06 below her neck line, so anchoring her bounds at
        // the socket lifts her ~0.06 model units — accepted (the bounds
        // convention stays uniform; the shift is invisible at contract
        // scale). Face sockets sit just proud of each measured face front.
        {"head_q_adventurer", PartCategory::Head, "quaternius",
         {0.220f, 0.290f, 0.231f},
         {{"eyes", {0.f, 0.142f, 0.118f}},
          {"hair", {0.f, 0.290f, 0.f}},
          {"mouth", {0.f, 0.052f, 0.120f}}},
         "quaternius", "Adventurer:Head"},
        {"head_q_scifi", PartCategory::Head, "quaternius",
         {0.231f, 0.316f, 0.291f},
         {{"eyes", {0.f, 0.140f, 0.148f}},
          {"hair", {0.f, 0.316f, 0.f}},
          {"mouth", {0.f, 0.050f, 0.148f}}},
         "quaternius", "SciFi:Head"},
        {"head_q_soldier", PartCategory::Head, "quaternius",
         {0.222f, 0.291f, 0.245f},
         {{"eyes", {0.f, 0.142f, 0.125f}},
          {"hair", {0.f, 0.291f, 0.f}},
          {"mouth", {0.f, 0.052f, 0.128f}}},
         "quaternius", "Soldier:Head"},
        {"head_q_witch", PartCategory::Head, "quaternius",
         {0.526f, 0.566f, 0.548f},
         {{"eyes", {0.f, 0.201f, 0.150f}},
          {"hair", {0.f, 0.566f, 0.f}},
          {"mouth", {0.f, 0.106f, 0.152f}}},
         "quaternius", "Witch:Head"},
        // Face picks (issue #140): for this family the Eyes/Mouth rows
        // select a flat face DECAL style (FaceTexture::bakeStylized draws
        // the actual pixels; the renderer maps these ids to its style
        // indexes) rendered as one head-oriented quad at the face sockets.
        // localSize is nominal — validity/assembly bookkeeping only. Row
        // order here mirrors the texture style order. The baked-hair
        // placeholder keeps the Hair category filled: mesh heads ship with
        // their hair.
        {"eyes_q_dot", PartCategory::Eyes, "quaternius",
         {0.095f, 0.030f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_wide", PartCategory::Eyes, "quaternius",
         {0.120f, 0.044f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_round", PartCategory::Eyes, "quaternius",
         {0.110f, 0.040f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_happy", PartCategory::Eyes, "quaternius",
         {0.105f, 0.032f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_sleepy", PartCategory::Eyes, "quaternius",
         {0.100f, 0.026f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_star", PartCategory::Eyes, "quaternius",
         {0.110f, 0.040f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_wink", PartCategory::Eyes, "quaternius",
         {0.105f, 0.030f, 0.024f}, {}, "quaternius", ""},
        {"eyes_q_glasses", PartCategory::Eyes, "quaternius",
         {0.130f, 0.048f, 0.024f}, {}, "quaternius", ""},
        {"hair_q_baked", PartCategory::Hair, "quaternius", {0.f, 0.f, 0.f},
         {}, "quaternius", ""},
        {"mouth_q_smile", PartCategory::Mouth, "quaternius",
         {0.075f, 0.024f, 0.020f}, {}, "quaternius", ""},
        {"mouth_q_line", PartCategory::Mouth, "quaternius",
         {0.070f, 0.014f, 0.020f}, {}, "quaternius", ""},
        {"mouth_q_open", PartCategory::Mouth, "quaternius",
         {0.065f, 0.045f, 0.020f}, {}, "quaternius", ""},
        {"mouth_q_o", PartCategory::Mouth, "quaternius",
         {0.045f, 0.045f, 0.020f}, {}, "quaternius", ""},
    };
    return parts;
}

const std::vector<PartPalette>& paletteCatalog() {
    // One cohesive Tomodachi-leaning pastel set (issue #102): warm skins,
    // soft saturated outfits, hair tones that read at a distance. Same ids
    // as before so stored looks keep their meaning.
    // Trousers come from three shared tones rather than twelve bespoke
    // ones: denim, slate and clay. A city where everyone's legs agree
    // reads as a crowd; twelve trouser colors read as a costume parade.
    constexpr unsigned char denim[3] = {93, 107, 138};
    constexpr unsigned char slate[3] = {84, 88, 98};
    constexpr unsigned char clay[3] = {124, 94, 80};
    static const std::vector<PartPalette> palettes = {
        {"warm", {247, 208, 168}, {126, 84, 54}, {255, 183, 77}, {denim[0], denim[1], denim[2]}},
        {"cool", {240, 196, 166}, {52, 60, 72}, {129, 140, 248}, {slate[0], slate[1], slate[2]}},
        {"sunny", {250, 214, 175}, {240, 196, 80}, {255, 138, 101}, {denim[0], denim[1], denim[2]}},
        {"forest", {232, 188, 152}, {94, 66, 42}, {129, 199, 132}, {clay[0], clay[1], clay[2]}},
        {"mono", {243, 225, 205}, {158, 158, 170}, {120, 130, 148}, {slate[0], slate[1], slate[2]}},
        {"berry", {245, 203, 178}, {120, 46, 74}, {244, 143, 177}, {slate[0], slate[1], slate[2]}},
        {"slate", {235, 205, 182}, {62, 70, 82}, {100, 149, 196}, {clay[0], clay[1], clay[2]}},
        {"mint", {242, 212, 182}, {74, 110, 88}, {128, 203, 196}, {slate[0], slate[1], slate[2]}},
        {"peach", {250, 218, 190}, {226, 148, 120}, {255, 171, 145}, {denim[0], denim[1], denim[2]}},
        {"sky", {244, 206, 180}, {140, 110, 80}, {121, 190, 235}, {clay[0], clay[1], clay[2]}},
        {"lilac", {238, 198, 175}, {104, 88, 130}, {197, 164, 226}, {slate[0], slate[1], slate[2]}},
        {"lemon", {248, 214, 182}, {206, 178, 94}, {250, 230, 140}, {denim[0], denim[1], denim[2]}},
    };
    return palettes;
}

const PartDef* findPart(const std::string& id) {
    for (const PartDef& part : partCatalog()) {
        if (part.id == id) return &part;
    }
    return nullptr;
}

bool styleCompatible(const PartDef& a, const PartDef& b) {
    return tagsCompatible(a.styleTag, b.styleTag);
}

std::vector<const PartDef*> partsForCategory(PartCategory category,
                                             const std::string& styleTag) {
    std::vector<const PartDef*> out;
    for (const PartDef& part : partCatalog()) {
        if (part.category != category) continue;
        // "any" as the QUERY means "list everything" (the creator's body
        // row); otherwise the same rule the validity gate applies.
        if (styleTag == "any" || tagsCompatible(part.styleTag, styleTag)) {
            out.push_back(&part);
        }
    }
    return out;
}

bool lookIsValid(const CharacterLook& look, std::string* why) {
    const PartDef* chosen[kPartCategoryCount] = {};
    for (int c = 0; c < kPartCategoryCount; ++c) {
        const std::string& id = look.partIds[c];
        if (id.empty()) {
            if (why) *why = "missing part for a category";
            return false;
        }
        chosen[c] = findPart(id);
        if (!chosen[c]) {
            if (why) *why = "unknown part: " + id;
            return false;
        }
        if (chosen[c]->category != static_cast<PartCategory>(c)) {
            if (why) *why = id + " is in the wrong category slot";
            return false;
        }
    }
    for (int a = 0; a < kPartCategoryCount; ++a) {
        for (int b = a + 1; b < kPartCategoryCount; ++b) {
            if (!styleCompatible(*chosen[a], *chosen[b])) {
                if (why) {
                    *why = chosen[a]->id + " and " + chosen[b]->id +
                           " mix incompatible styles";
                }
                return false;
            }
        }
    }
    bool paletteKnown = false;
    for (const PartPalette& palette : paletteCatalog()) {
        paletteKnown = paletteKnown || palette.id == look.paletteId;
    }
    if (!paletteKnown) {
        if (why) *why = "unknown palette: " + look.paletteId;
        return false;
    }
    return true;
}

AssembledLook assembleLook(const CharacterLook& look) {
    AssembledLook out;
    if (!lookIsValid(look, &out.error)) return out;

    // Resolve category anchors in spec order (parents listed before
    // children), each child at parent anchor + parent's declared socket.
    Vec3 anchors[kPartCategoryCount];
    for (const CategorySpec& spec : categorySpecs()) {
        const int c = static_cast<int>(spec.category);
        const PartDef* part = findPart(look.partIds[c]);
        if (spec.socket[0] == '\0') {
            anchors[c] = Vec3{};  // root sits on the ground at the origin
        } else {
            const int p = static_cast<int>(spec.parent);
            const PartDef* parent = findPart(look.partIds[p]);
            const auto socket = parent->sockets.find(spec.socket);
            if (socket == parent->sockets.end()) {
                out.error = parent->id + " has no '" + spec.socket + "' socket";
                return out;
            }
            anchors[c] = anchors[p] + socket->second;
        }
        out.parts.push_back({part, anchors[c]});
        out.height = std::max(out.height, anchors[c].y + part->localSize.y);
    }
    out.ok = out.height > 0.f;
    if (!out.ok) out.error = "assembled look has no height";
    return out;
}

CharacterLook randomizeLook(unsigned seed) {
    Rng rng{seed ? seed : 1u};  // xorshift can't start at 0
    CharacterLook look;

    // Body first — it sets the style family the other picks must match.
    // Generated looks draw from the DEFAULT pack only; the full catalog
    // stays reachable through the creator's body row. Filtering by pack
    // (not style tag) keeps both core families — round and blocky — in the
    // pool, which the style gate then honours for every child category.
    std::vector<const PartDef*> bodies;
    for (const PartDef* candidate : partsForCategory(PartCategory::Body, "any")) {
        if (candidate->pack == kDefaultBodyPack) bodies.push_back(candidate);
    }
    const PartDef* body = bodies[rng.pick(bodies.size())];
    look.part(PartCategory::Body) = body->id;
    for (const CategorySpec& spec : categorySpecs()) {
        if (spec.category == PartCategory::Body) continue;
        const auto options = partsForCategory(spec.category, body->styleTag);
        look.part(spec.category) = options[rng.pick(options.size())]->id;
    }
    const auto& palettes = paletteCatalog();
    look.paletteId = palettes[rng.pick(palettes.size())].id;
    return look;
}

CharacterLook lookForPersona(const std::string& name,
                             const CharacterLook* authored,
                             std::string* whyFallback) {
    if (authored) {
        std::string why;
        if (lookIsValid(*authored, &why)) return *authored;
        if (whyFallback) *whyFallback = why;
    }
    // FNV-1a over the name: deterministic across runs and platforms, so an
    // unauthored persona keeps the same face forever instead of reshuffling
    // every launch.
    unsigned hash = 2166136261u;
    for (const char ch : name) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 16777619u;
    }
    return randomizeLook(hash);
}

std::string CharacterLook::toJson() const {
    nlohmann::json j;
    j["body"] = part(PartCategory::Body);
    j["head"] = part(PartCategory::Head);
    j["eyes"] = part(PartCategory::Eyes);
    j["hair"] = part(PartCategory::Hair);
    j["mouth"] = part(PartCategory::Mouth);
    j["palette"] = paletteId;
    return j.dump();
}

bool CharacterLook::fromJson(const std::string& json, CharacterLook& out) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) return false;
    const char* keys[kPartCategoryCount] = {"body", "head", "eyes", "hair", "mouth"};
    for (int c = 0; c < kPartCategoryCount; ++c) {
        // Looks stored before the Mouth category (issue #104) have no
        // "mouth" key: default the canonical smile so the character keeps
        // its authored identity instead of demoting to the hash fallback.
        if (!j.contains(keys[c])) {
            if (static_cast<PartCategory>(c) == PartCategory::Mouth) {
                out.partIds[c] = "mouth_smile";
                continue;
            }
            return false;
        }
        if (!j[keys[c]].is_string()) return false;
        out.partIds[c] = j[keys[c]].get<std::string>();
    }
    if (!j.contains("palette") || !j["palette"].is_string()) return false;
    out.paletteId = j["palette"].get<std::string>();
    return true;
}

}  // namespace llm_npc
