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

}  // namespace

const std::vector<CategorySpec>& categorySpecs() {
    // Body is the root; everything else names its parent and socket. Adding
    // a category (e.g. Accessory on the body's "hand" socket) is one row.
    static const std::vector<CategorySpec> specs = {
        {PartCategory::Body, PartCategory::Body, ""},
        {PartCategory::Head, PartCategory::Body, "head"},
        {PartCategory::Eyes, PartCategory::Head, "eyes"},
        {PartCategory::Hair, PartCategory::Head, "hair"},
    };
    return specs;
}

const std::vector<PartDef>& partCatalog() {
    // All positions are in each part's own local space, anchor at
    // bottom-center. Two style families plus "any" parts; sockets are
    // authored per part — a wider head simply declares its eye socket
    // farther forward, and every eye part inherits that for free.
    static const std::vector<PartDef> parts = {
        // Bodies (declare where a head sits on THEM).
        {"body_round", PartCategory::Body, "round", {0.90f, 1.00f, 0.52f},
         {{"head", {0.f, 0.98f, 0.f}}}},
        {"body_block", PartCategory::Body, "blocky", {0.84f, 1.00f, 0.46f},
         {{"head", {0.f, 1.00f, 0.f}}}},
        // Heads (declare where eyes and hair sit on THEM).
        {"head_round", PartCategory::Head, "round", {0.72f, 0.72f, 0.72f},
         {{"eyes", {0.f, 0.40f, 0.30f}}, {"hair", {0.f, 0.64f, 0.f}}}},
        {"head_block", PartCategory::Head, "blocky", {0.76f, 0.76f, 0.76f},
         {{"eyes", {0.f, 0.44f, 0.38f}}, {"hair", {0.f, 0.72f, 0.f}}}},
        // Eyes.
        {"eyes_dot", PartCategory::Eyes, "any", {0.34f, 0.10f, 0.08f}, {}},
        {"eyes_wide", PartCategory::Eyes, "round", {0.46f, 0.16f, 0.08f}, {}},
        {"eyes_visor", PartCategory::Eyes, "blocky", {0.58f, 0.14f, 0.08f}, {}},
        // Hair. "hair_none" is a real part with zero size so the assembly
        // never has holes and the picker has an explicit bald option.
        {"hair_none", PartCategory::Hair, "any", {0.f, 0.f, 0.f}, {}},
        {"hair_tuft", PartCategory::Hair, "round", {0.30f, 0.26f, 0.30f}, {}},
        {"hair_bowl", PartCategory::Hair, "any", {0.76f, 0.28f, 0.76f}, {}},
        {"hair_spikes", PartCategory::Hair, "blocky", {0.70f, 0.30f, 0.70f}, {}},

        // --- More variety (issue #92), additive within the same contract ---
        // Bodies (blocky bodies render as the declared box like body_block).
        {"body_slim", PartCategory::Body, "round", {0.70f, 1.04f, 0.44f},
         {{"head", {0.f, 1.02f, 0.f}}}},
        {"body_bulk", PartCategory::Body, "blocky", {1.04f, 0.94f, 0.60f},
         {{"head", {0.f, 0.92f, 0.f}}}},
        // Heads (blocky heads render as the declared box like head_block).
        {"head_oval", PartCategory::Head, "round", {0.66f, 0.84f, 0.66f},
         {{"eyes", {0.f, 0.48f, 0.28f}}, {"hair", {0.f, 0.78f, 0.f}}}},
        {"head_tall", PartCategory::Head, "blocky", {0.70f, 0.90f, 0.70f},
         {{"eyes", {0.f, 0.52f, 0.36f}}, {"hair", {0.f, 0.86f, 0.f}}}},
        // Eyes (both use the shared two-pupil eye recipe, sized by localSize).
        {"eyes_round", PartCategory::Eyes, "round", {0.44f, 0.20f, 0.08f}, {}},
        {"eyes_happy", PartCategory::Eyes, "any", {0.40f, 0.12f, 0.08f}, {}},
        // Hair.
        {"hair_pony", PartCategory::Hair, "round", {0.34f, 0.30f, 0.34f}, {}},
        {"hair_mohawk", PartCategory::Hair, "blocky", {0.26f, 0.42f, 0.62f}, {}},
    };
    return parts;
}

const std::vector<PartPalette>& paletteCatalog() {
    static const std::vector<PartPalette> palettes = {
        {"warm", {236, 188, 150}, {92, 60, 34}, {70, 120, 168}},
        {"cool", {224, 172, 138}, {36, 32, 30}, {96, 78, 140}},
        {"sunny", {242, 200, 160}, {214, 172, 60}, {188, 84, 60}},
        {"forest", {208, 158, 122}, {70, 46, 28}, {74, 128, 82}},
        {"mono", {228, 214, 198}, {150, 150, 158}, {64, 66, 74}},
        // More palettes (issue #92) — purely additive; the picker cycles them.
        {"berry", {228, 176, 150}, {88, 30, 52}, {150, 54, 92}},
        {"slate", {206, 184, 170}, {54, 60, 70}, {88, 104, 124}},
        {"mint", {224, 196, 168}, {60, 92, 74}, {96, 176, 140}},
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
    return a.styleTag == "any" || b.styleTag == "any" || a.styleTag == b.styleTag;
}

std::vector<const PartDef*> partsForCategory(PartCategory category,
                                             const std::string& styleTag) {
    std::vector<const PartDef*> out;
    for (const PartDef& part : partCatalog()) {
        if (part.category != category) continue;
        if (styleTag == "any" || part.styleTag == "any" || part.styleTag == styleTag) {
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
    const auto bodies = partsForCategory(PartCategory::Body, "any");
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
    j["palette"] = paletteId;
    return j.dump();
}

bool CharacterLook::fromJson(const std::string& json, CharacterLook& out) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (!j.is_object()) return false;
    const char* keys[kPartCategoryCount] = {"body", "head", "eyes", "hair"};
    for (int c = 0; c < kPartCategoryCount; ++c) {
        if (!j.contains(keys[c]) || !j[keys[c]].is_string()) return false;
        out.partIds[c] = j[keys[c]].get<std::string>();
    }
    if (!j.contains("palette") || !j["palette"].is_string()) return false;
    out.paletteId = j["palette"].get<std::string>();
    return true;
}

}  // namespace llm_npc
