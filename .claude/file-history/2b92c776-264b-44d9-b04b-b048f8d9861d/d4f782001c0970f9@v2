#pragma once

#include <string>
#include <vector>

namespace llm_npc {

// One placeable sandbox map piece (plan: sandbox-map-editor). A piece is a
// DATA row — adding one later must never require code: the editor palette,
// the validator, and buildCity all read this catalog, and rendering keys on
// `assetId` through the existing Assets lookup (with an instance suffix,
// e.g. "bakery#2", stripped at '#').
struct PieceDef {
    std::string id;       // stable piece id, e.g. "shop_bakery"
    std::string label;    // palette display name, e.g. "Bakery"
    std::string assetId;  // Building id the renderer/Assets key on ("bakery")
    int tilesW = 1;       // footprint in 8-unit tiles, X
    int tilesD = 1;       // footprint in 8-unit tiles, Z
    float height = 0.f;   // collider/visual height in world units
    bool solid = true;    // false = visual paint (road tiles): never collides
    std::string styleTag; // grouping for the palette ("shop", "prop", ...)
    std::string pack = "core";  // content-pack seam, same as PartDef
};

// TODO(sandbox step 1): author ~20 starter rows in PieceCatalog.cpp — the
// five named shops, apartment/office fillers, tower, fountain, bench,
// bushes, traffic light, cars, cart, dumpster, trash, road-paint tiles.
// Footprints/heights come from City::makeDowntown's authored AABBs.
const std::vector<PieceDef>& pieceCatalog();

// Piece by id; nullptr when unknown (a map from an older catalog).
const PieceDef* findPiece(const std::string& id);

}  // namespace llm_npc
