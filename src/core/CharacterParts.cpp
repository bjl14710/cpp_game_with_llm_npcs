#include "CharacterParts.hpp"

#include <algorithm>

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

// The family new random looks are generated from (issue #139): the default
// pool switched from the core primitive families to the quaternius mesh
// pack. Core parts stay in the catalog — stored looks keep validating and
// the creator can still cycle onto them deliberately.
constexpr const char* kDefaultBodyStyle = "quaternius";

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

const std::vector<PartDef>& partCatalog() {
    // All positions are in each part's own local space, anchor at
    // bottom-center. Two style families plus "any" parts; sockets are
    // authored per part — a wider head simply declares its eye socket
    // farther forward, and every eye part inherits that for free.
    // Proportions follow the Mii/Tomodachi target (issue #102): heads are
    // roughly HALF the body+head silhouette (head.y / (headSocket.y +
    // head.y) in 0.48-0.60, pinned by test_style_pack.cpp), bodies short
    // and rounded under them. The 1.8u contract scale normalizes overall
    // height, so these ratios — not absolute sizes — are what changes on
    // screen.
    static const std::vector<PartDef> parts = {
        // Bodies (declare where a head sits on THEM).
        {"body_round", PartCategory::Body, "round", {0.92f, 0.78f, 0.54f},
         {{"head", {0.f, 0.76f, 0.f}}}},
        {"body_block", PartCategory::Body, "blocky", {0.86f, 0.82f, 0.48f},
         {{"head", {0.f, 0.80f, 0.f}}}},
        {"body_slim", PartCategory::Body, "round", {0.72f, 0.86f, 0.46f},
         {{"head", {0.f, 0.82f, 0.f}}}},
        {"body_bulk", PartCategory::Body, "blocky", {1.06f, 0.80f, 0.62f},
         {{"head", {0.f, 0.78f, 0.f}}}},
        {"body_pear", PartCategory::Body, "round", {0.98f, 0.76f, 0.56f},
         {{"head", {0.f, 0.74f, 0.f}}}},
        {"body_broad", PartCategory::Body, "blocky", {1.14f, 0.78f, 0.58f},
         {{"head", {0.f, 0.80f, 0.f}}}},
        // Heads (declare where eyes, hair — and later mouths — sit on THEM).
        {"head_round", PartCategory::Head, "round", {0.98f, 0.98f, 0.98f},
         {{"eyes", {0.f, 0.54f, 0.41f}},
          {"hair", {0.f, 0.88f, 0.f}},
          {"mouth", {0.f, 0.30f, 0.44f}}}},
        {"head_block", PartCategory::Head, "blocky", {1.00f, 1.00f, 1.00f},
         {{"eyes", {0.f, 0.58f, 0.50f}},
          {"hair", {0.f, 0.95f, 0.f}},
          {"mouth", {0.f, 0.30f, 0.50f}}}},
        {"head_oval", PartCategory::Head, "round", {0.90f, 1.10f, 0.90f},
         {{"eyes", {0.f, 0.62f, 0.38f}},
          {"hair", {0.f, 1.02f, 0.f}},
          {"mouth", {0.f, 0.34f, 0.41f}}}},
        {"head_tall", PartCategory::Head, "blocky", {0.94f, 1.16f, 0.94f},
         {{"eyes", {0.f, 0.66f, 0.48f}},
          {"hair", {0.f, 1.10f, 0.f}},
          {"mouth", {0.f, 0.36f, 0.47f}}}},
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
    static const std::vector<PartPalette> palettes = {
        {"warm", {247, 208, 168}, {126, 84, 54}, {255, 183, 77}},
        {"cool", {240, 196, 166}, {52, 60, 72}, {129, 140, 248}},
        {"sunny", {250, 214, 175}, {240, 196, 80}, {255, 138, 101}},
        {"forest", {232, 188, 152}, {94, 66, 42}, {129, 199, 132}},
        {"mono", {243, 225, 205}, {158, 158, 170}, {120, 130, 148}},
        {"berry", {245, 203, 178}, {120, 46, 74}, {244, 143, 177}},
        {"slate", {235, 205, 182}, {62, 70, 82}, {100, 149, 196}},
        {"mint", {242, 212, 182}, {74, 110, 88}, {128, 203, 196}},
        {"peach", {250, 218, 190}, {226, 148, 120}, {255, 171, 145}},
        {"sky", {244, 206, 180}, {140, 110, 80}, {121, 190, 235}},
        {"lilac", {238, 198, 175}, {104, 88, 130}, {197, 164, 226}},
        {"lemon", {248, 214, 182}, {206, 178, 94}, {250, 230, 140}},
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
    // Generated looks draw from the DEFAULT family only (issue #139); the
    // full catalog stays reachable through the creator's body row.
    const auto bodies = partsForCategory(PartCategory::Body, kDefaultBodyStyle);
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
