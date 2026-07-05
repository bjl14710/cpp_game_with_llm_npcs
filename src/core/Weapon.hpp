#pragma once

#include <array>
#include <cstdint>

namespace llm_npc {

// The two weapon kinds available in v1. Fist is always available; Pistol
// requires ammo. Expand this enum when adding new weapon types.
enum class WeaponKind : std::uint8_t {
    Fist = 0,
    Pistol = 1,
};

// Static data for one weapon type. All values are design constants — never
// mutated at runtime.
struct WeaponDef {
    const char* name;       // display name for HUD ("Fist", "Pistol")
    int         damage;     // hit points subtracted per successful hit
    float       range;      // maximum distance (world units) a hit can land
    int         ammoMax;    // magazine/pool capacity; 0 = melee (unlimited)
    float       fireCooldown; // minimum seconds between attacks
    float       accuracy;   // 0.0–1.0 base hit probability (1.0 = always hits)
};

// Indexed by WeaponKind. Add a row here when adding a new WeaponKind variant.
inline constexpr std::array<WeaponDef, 2> kWeaponDefs{{
    // Fist
    {"Fist",   15, 1.8f,  0,  0.40f, 1.0f},
    // Pistol
    {"Pistol", 35, 30.0f, 12, 0.55f, 0.70f},
}};

// Guard: the table must have one entry per WeaponKind variant.
static_assert(std::tuple_size<decltype(kWeaponDefs)>::value == 2,
              "kWeaponDefs size must match WeaponKind variant count");

// Convenience: returns the definition for a given kind.
inline const WeaponDef& weaponDef(WeaponKind k) {
    return kWeaponDefs[static_cast<std::size_t>(k)];
}

}  // namespace llm_npc
