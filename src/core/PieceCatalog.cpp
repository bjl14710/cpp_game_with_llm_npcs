#include "PieceCatalog.hpp"

namespace llm_npc {

const std::vector<PieceDef>& pieceCatalog() {
    // Footprints and heights are sized from City::makeDowntown's authored
    // AABBs, snapped to whole 8-unit tiles. assetId is the Building id the
    // Assets curated-model lookup keys on (with the "#n" instance suffix
    // stripped); every asset here already ships with the game. Road-paint
    // (solid=false) pieces are deliberately absent from the v1 catalog:
    // visual pieces emit no Building, and the downtown road pass is
    // authored geometry, not pieces (decision logged in
    // OVERNIGHT_REPORT.md; the schema and validator support solid=false
    // for the follow-up).
    static const std::vector<PieceDef> pieces = {
        // Shops (styleTag "shop").
        {"shop_bakery", "Bakery", "bakery", 4, 3, 12.f, true, "shop"},
        {"shop_police", "Police Station", "police", 5, 5, 16.f, true, "shop"},
        {"shop_coffee", "Coffee Shop", "coffee", 4, 3, 10.f, true, "shop"},
        {"shop_library", "Library", "library", 5, 4, 15.f, true, "shop"},
        {"shop_hardware", "Hardware Store", "hardware", 4, 4, 11.f, true, "shop"},
        // Filler blocks (styleTag "block").
        {"apt_small", "Small Apartments", "apt_b", 3, 2, 18.f, true, "block"},
        {"apt_large", "Large Apartments", "apts_c", 5, 3, 24.f, true, "block"},
        {"office", "Office Block", "office_a", 5, 3, 26.f, true, "block"},
        {"tower", "Tower", "tower", 6, 5, 30.f, true, "block"},
        // Props (styleTag "prop").
        {"fountain", "Fountain", "fountain", 1, 1, 1.5f, true, "prop"},
        {"bench", "Bench", "bench", 1, 1, 0.6f, true, "prop"},
        {"bush", "Bush", "bush_a", 1, 1, 1.1f, true, "prop"},
        {"traffic_light", "Traffic Light", "trafficlight_a", 1, 1, 4.5f, true, "prop"},
        {"hotdog_cart", "Hot-Dog Cart", "cart", 1, 1, 3.f, true, "prop"},
        {"taxi", "Taxi", "taxi_cab", 1, 1, 1.6f, true, "prop"},
        {"car_police", "Police Car", "police_car", 1, 1, 1.5f, true, "prop"},
        {"car_sedan", "Sedan", "sedan_a", 1, 1, 1.35f, true, "prop"},
        {"car_hatchback", "Hatchback", "hatchback_a", 1, 1, 1.35f, true, "prop"},
        {"dumpster", "Dumpster", "dumpster_a", 1, 1, 1.4f, true, "prop"},
        {"trash_can", "Trash Can", "trash_a", 1, 1, 0.9f, true, "prop"},
    };
    return pieces;
}

const PieceDef* findPiece(const std::string& id) {
    for (const PieceDef& piece : pieceCatalog()) {
        if (piece.id == id) return &piece;
    }
    return nullptr;
}

}  // namespace llm_npc
