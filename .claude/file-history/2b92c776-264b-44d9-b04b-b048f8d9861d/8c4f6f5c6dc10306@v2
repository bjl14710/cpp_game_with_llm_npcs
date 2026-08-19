// Tests for PlayerState: the wallet, buying weapons, and selection.
#include "PlayerState.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {
const ShopItem& bat() { return kWeaponCatalog[0]; }
const ShopItem& pistol() { return kWeaponCatalog[1]; }
}  // namespace

TEST_CASE("a fresh player has no money and only fists") {
    PlayerState p;
    CHECK(p.money() == 0);
    CHECK(p.owns(WeaponKind::Fists));
    CHECK_FALSE(p.owns(WeaponKind::Bat));
    CHECK_FALSE(p.owns(WeaponKind::Pistol));
    CHECK(p.current() == WeaponKind::Fists);
}

TEST_CASE("withdraw adds cash and ignores non-positive amounts") {
    PlayerState p;
    p.withdraw(500);
    CHECK(p.money() == 500);
    p.withdraw(500);
    CHECK(p.money() == 1000);
    p.withdraw(-100);  // ignored
    CHECK(p.money() == 1000);
    p.withdraw(0);     // ignored
    CHECK(p.money() == 1000);
}

TEST_CASE("buy deducts the price and grants the weapon only when affordable") {
    PlayerState p;
    CHECK_FALSE(p.buy(pistol()));   // broke: no change
    CHECK(p.money() == 0);
    CHECK_FALSE(p.owns(WeaponKind::Pistol));

    p.withdraw(300);
    CHECK(p.buy(pistol()));         // 300 >= 250
    CHECK(p.money() == 300 - pistol().price);
    CHECK(p.owns(WeaponKind::Pistol));
    CHECK(p.current() == WeaponKind::Pistol);  // wields what was bought
}

TEST_CASE("you can't buy the same weapon twice") {
    PlayerState p;
    p.withdraw(1000);
    REQUIRE(p.buy(bat()));
    const int afterFirst = p.money();
    CHECK_FALSE(p.buy(bat()));      // already owned: no second charge
    CHECK(p.money() == afterFirst);
}

TEST_CASE("select only switches to an owned weapon") {
    PlayerState p;
    CHECK(p.select(WeaponKind::Fists));        // always owned
    CHECK_FALSE(p.select(WeaponKind::Bat));    // not owned yet
    CHECK(p.current() == WeaponKind::Fists);

    p.withdraw(100);
    REQUIRE(p.buy(bat()));
    CHECK(p.select(WeaponKind::Fists));        // can switch back to fists
    CHECK(p.current() == WeaponKind::Fists);
    CHECK(p.select(WeaponKind::Bat));          // now owned
    CHECK(p.current() == WeaponKind::Bat);
}
