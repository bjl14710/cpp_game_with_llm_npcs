#include "PlayerState.hpp"

namespace llm_npc {

bool PlayerState::owns(WeaponKind w) const {
    switch (w) {
        case WeaponKind::Fists:  return true;  // always available
        case WeaponKind::Bat:    return ownBat_;
        case WeaponKind::Pistol: return ownPistol_;
    }
    return false;
}

bool PlayerState::select(WeaponKind w) {
    if (!owns(w)) return false;
    current_ = w;
    return true;
}

bool PlayerState::buy(const ShopItem& item) {
    if (owns(item.weapon)) return false;       // no point buying twice
    if (money_ < item.price) return false;     // can't afford it
    money_ -= item.price;
    switch (item.weapon) {
        case WeaponKind::Bat:    ownBat_ = true; break;
        case WeaponKind::Pistol: ownPistol_ = true; break;
        case WeaponKind::Fists:  break;  // fists are never sold
    }
    current_ = item.weapon;  // wield what you just bought
    return true;
}

}  // namespace llm_npc
