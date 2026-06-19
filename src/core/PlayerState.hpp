#pragma once

#include <array>

namespace llm_npc {

// A weapon the player can wield. Fists are always available; the rest are
// bought from shops. Shared with the combat system (see Combat.hpp).
enum class WeaponKind { Fists, Bat, Pistol };

// One purchasable item in a shop's catalog.
struct ShopItem {
    const char* id;     // stable id, e.g. "bat"
    const char* label;  // display label, e.g. "Baseball bat"
    int price;          // cost in dollars
    WeaponKind weapon;  // the weapon owning it grants
};

// The weapons the hardware store stocks. Static so it's shared by the shop UI
// and tests without duplication.
inline constexpr std::array<ShopItem, 2> kWeaponCatalog = {{
    {"bat", "Baseball bat", 50, WeaponKind::Bat},
    {"pistol", "Pistol", 250, WeaponKind::Pistol},
}};

// The player's wallet and arsenal. SFML-free so it stays unit-tested; the app
// layer drives it from the bank, the shop UI, and weapon selection.
class PlayerState {
   public:
    int money() const { return money_; }

    // Adds cash to the wallet (the bank withdrawal). Non-positive amounts are
    // ignored so a stray call can't drain the player.
    void withdraw(int amount) {
        if (amount > 0) money_ += amount;
    }

    // True when the player owns `w` (Fists are always owned).
    bool owns(WeaponKind w) const;

    // The currently wielded weapon.
    WeaponKind current() const { return current_; }

    // Selects `w` as the wielded weapon when it is owned; otherwise leaves the
    // selection unchanged. Returns true when the selection took effect.
    bool select(WeaponKind w);

    // Buys `item` when the player can afford it and does not already own it:
    // deducts the price, marks the weapon owned, and wields it. Returns false
    // (and changes nothing) when unaffordable or already owned.
    bool buy(const ShopItem& item);

   private:
    int money_ = 0;
    bool ownBat_ = false;
    bool ownPistol_ = false;
    WeaponKind current_ = WeaponKind::Fists;
};

}  // namespace llm_npc
