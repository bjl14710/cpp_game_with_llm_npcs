#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Math.hpp"

namespace llm_npc {

// Part categories for the character creator. Extending the system is a
// data change: add an enum value, a CategorySpec row (who it attaches to,
// via which socket), and catalog parts that declare that socket — Mouth
// (issue #104) landed exactly that way.
enum class PartCategory { Body, Head, Eyes, Hair, Mouth };
constexpr int kPartCategoryCount = 5;

// How a category attaches into the assembly: every non-root category names
// its parent category and the SOCKET NAME it snaps to. The socket's
// position is authored on each parent part (in that part's local space) —
// there is deliberately no per-pair offset table anywhere.
struct CategorySpec {
    PartCategory category;
    PartCategory parent;      // ignored for the root (Body)
    const char* socket;       // "" for the root
};

// One authorable part. `localSize` is the part's bounding box with the
// anchor at the bottom-center (the same feet-on-ground convention world
// positions use); `sockets` are child attach points relative to that
// anchor. `styleTag` gates combinations: "round"/"blocky" families plus
// "any" parts that fit either.
struct PartDef {
    std::string id;
    PartCategory category = PartCategory::Body;
    std::string styleTag;
    Vec3 localSize{};
    std::unordered_map<std::string, Vec3> sockets;
    // Graphics-pack seam (plan: mii-style-visual-overhaul step 1): which
    // content pack this part ships in. A purchased/downloaded pack is
    // catalog rows + renderer recipes + palettes under a new tag — never
    // changes to assembly or picker logic. Everything built-in is "core".
    std::string pack = "core";
    // Mesh-backed parts (plan: stylized-character-assets step 3): the
    // asset name the renderer loads and draws at this part's socket,
    // contract-scaled. Empty = primitive recipe (all "core" parts). A
    // mesh part still declares localSize (its MEASURED bounds — that is
    // what sockets and the 1.8u contract consume) and sockets.
    std::string meshName = "";
};

// A named flat-color palette (RGB 0-255) the renderer maps onto recipes.
struct PartPalette {
    std::string id;
    unsigned char skin[3];
    unsigned char hair[3];
    unsigned char outfit[3];
    // Trousers. A separate column rather than a tint of `outfit`: trousers
    // in the outfit color read as a jumpsuit, and a derived tint comes out
    // muddy across the twelve palettes. Three shared tones (denim, slate,
    // clay) deliberately unify the street.
    unsigned char pants[3];
    // Same pack seam as PartDef (aggregate-initialized rows without a pack
    // value default to "core" via this initializer).
    std::string pack = "core";
};

// The look half of a created character: one part id per category plus a
// palette. Fully independent from the persona record by design.
struct CharacterLook {
    std::string partIds[kPartCategoryCount];  // indexed by PartCategory
    std::string paletteId;

    std::string& part(PartCategory c) { return partIds[static_cast<int>(c)]; }
    const std::string& part(PartCategory c) const {
        return partIds[static_cast<int>(c)];
    }

    // Compact JSON for storage; fromJson returns false on malformed input.
    std::string toJson() const;
    static bool fromJson(const std::string& json, CharacterLook& out);
};

// One placed part of a finished assembly: its anchor position in assembly
// space (unscaled part-local units; the renderer applies the single size
// contract scale afterwards).
struct PlacedPart {
    const PartDef* part = nullptr;
    Vec3 position{};
};

// A resolved look: every category placed, total height measured so ONE
// uniform scale can satisfy the character height contract.
struct AssembledLook {
    bool ok = false;
    std::string error;             // set when !ok
    std::vector<PlacedPart> parts; // assembly order: parents before children
    float height = 0.f;            // unscaled; contract scale = target / height
};

// The renderer draws a feature part (eyes, mouth) this fraction of its own
// declared depth in FRONT of the socket it hangs on — drawPartRecipe's `fz`.
// Named here, not buried in the recipe, because the socket contract and the
// recipe have to agree about where a feature actually lands: authoring a
// feature socket means reasoning about socket.z + depth * kFeatureZPush.
inline constexpr float kFeatureZPush = 0.35f;

// How far a flat mark (a mouth) is authored PAST the skull surface, so it
// never z-fights with the skin it is drawn on.
inline constexpr float kMarkStandoff = 0.010f;

// How far forward the drawn skull's surface reaches at local height `y`,
// measured from the head's own centre line. This is a CONTRACT on the
// styleTag, not a survey of the recipes: a head tagged "blocky" is drawn as
// a box, so its face is one flat plane at half its depth at every height; a
// head tagged "round" is drawn as the ellipsoid whose semi-axes are half its
// declared dims, so its surface RECEDES as a feature leaves the eye line —
// which is exactly what makes an authored-once feature z wrong once the
// feature moves. Adding a round head means honouring that contract in
// drawPartRecipe. Mesh heads own their own geometry and get the bounding
// plane, which is all this can honestly say about them.
float skullSurfaceZ(const PartDef& head, float y);

// The built-in catalogs (static data, stable addresses for the process).
const std::vector<PartDef>& partCatalog();
const std::vector<PartPalette>& paletteCatalog();
const std::vector<CategorySpec>& categorySpecs();

// Part by id; nullptr when unknown (e.g. a stored look from an older
// catalog).
const PartDef* findPart(const std::string& id);

// Style gate: equal tags, or either side is "any".
bool styleCompatible(const PartDef& a, const PartDef& b);

// Parts of one category compatible with `styleTag` ("any" matches all).
std::vector<const PartDef*> partsForCategory(PartCategory category,
                                             const std::string& styleTag);

// Validates a look: every category filled with a known part, all parts
// pairwise style-compatible, palette known. `why` gets the first problem.
bool lookIsValid(const CharacterLook& look, std::string* why = nullptr);

// Snaps the look together via the socket contracts. Fails (ok=false) when
// the look is invalid or a parent part lacks the needed socket.
AssembledLook assembleLook(const CharacterLook& look);

// Deterministic, always-valid random look for a seed (picker "Randomize").
CharacterLook randomizeLook(unsigned seed);

// The one look source for named NPCs: the authored look when it validates,
// otherwise a deterministic randomizeLook(hash(name)) — stable across runs,
// so every persona (authored or not) always spawns with a valid look from
// the SAME shared catalog the creator picks from. When an authored look is
// rejected, `whyFallback` (if non-null) gets the reason so the spawn site
// can log it.
CharacterLook lookForPersona(const std::string& name,
                             const CharacterLook* authored = nullptr,
                             std::string* whyFallback = nullptr);

}  // namespace llm_npc
