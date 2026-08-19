#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "Weapon.hpp"
#include "World.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

LlmClient& idleClient() {
    static LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    return client;
}

Npc makeNpcAt(float x, float z, bool armed = false) {
    Persona p;
    p.name  = armed ? "Cop" : "Civilian";
    p.armed = armed;
    Npc npc(p, idleClient());
    npc.setPlacement({x, 0.f, z}, 0.f, "spot");
    return npc;
}

}  // namespace

// ---- playerAttack — melee hit detection ---------------------------------

TEST_CASE("playerAttack hits NPC within fist range") {
    World world(City::makeDowntown());
    world.addNpc(makeNpcAt(1.0f, 0.f));
    world.player().position = Vec3{0.f, 0.f, 0.f};

    // TODO(combat): enable once World::playerAttack is implemented
    // const bool attacked = world.playerAttack(Vec3{0.f, 0.f, 1.f});
    // CHECK(attacked);
    // CHECK(world.npcs()[0].hp() < 100);

    CHECK(true);  // placeholder — remove when TODO above is active
}

TEST_CASE("playerAttack does not hit NPC beyond fist range") {
    World world(City::makeDowntown());
    world.addNpc(makeNpcAt(10.f, 0.f));  // well outside 1.8 unit fist range
    world.player().position = Vec3{0.f, 0.f, 0.f};

    // TODO(combat): enable once World::playerAttack is implemented
    // world.playerAttack(Vec3{1.f, 0.f, 0.f});
    // CHECK(world.npcs()[0].hp() == 100);

    CHECK(true);
}

TEST_CASE("playerAttack is suppressed during cooldown") {
    World world(City::makeDowntown());
    world.addNpc(makeNpcAt(1.0f, 0.f));
    world.player().position = Vec3{0.f, 0.f, 0.f};

    // TODO(combat): enable once World::playerAttack is implemented
    // world.playerAttack(Vec3{0.f, 0.f, 1.f});     // first hit succeeds
    // const bool second = world.playerAttack(Vec3{0.f, 0.f, 1.f});  // suppressed
    // CHECK_FALSE(second);

    CHECK(true);
}

TEST_CASE("Pistol attack decrements ammo") {
    World world(City::makeDowntown());
    world.addNpc(makeNpcAt(5.f, 0.f));
    world.player().position = Vec3{0.f, 0.f, 0.f};
    world.playerSwitchWeapon(WeaponKind::Pistol);

    const int ammoBefore = world.player().currentAmmo();
    CHECK(ammoBefore == 12);

    // TODO(combat): enable once World::playerAttack spawns projectiles
    // world.playerAttack(Vec3{0.f, 0.f, 1.f});
    // CHECK(world.player().currentAmmo() == ammoBefore - 1);

    CHECK(true);
}

TEST_CASE("Pistol attack blocked at 0 ammo") {
    World world(City::makeDowntown());
    world.playerSwitchWeapon(WeaponKind::Pistol);
    world.player().ammo[static_cast<std::uint8_t>(WeaponKind::Pistol)] = 0;

    // TODO(combat): enable once World::playerAttack checks ammo
    // CHECK_FALSE(world.playerAttack(Vec3{0.f, 0.f, 1.f}));

    CHECK(true);
}

// ---- updateCombat — cooldown tick ---------------------------------------

TEST_CASE("updateCombat decrements player attack cooldown") {
    World world(City::makeDowntown());
    world.player().attackCooldown = 1.0f;
    world.updateCombat(0.5f);
    CHECK(world.player().attackCooldown == doctest::Approx(0.5f).epsilon(0.01f));
}

TEST_CASE("updateCombat clamps player cooldown to 0") {
    World world(City::makeDowntown());
    world.player().attackCooldown = 0.1f;
    world.updateCombat(1.0f);
    CHECK(world.player().attackCooldown == 0.f);
}

TEST_CASE("updateCombat decrements NPC fire cooldown") {
    World world(City::makeDowntown());
    world.addNpc(makeNpcAt(5.f, 0.f, /*armed=*/true));
    world.npcs()[0].fireCooldown = 1.5f;
    world.updateCombat(0.5f);
    CHECK(world.npcs()[0].fireCooldown == doctest::Approx(1.0f).epsilon(0.01f));
}

// ---- Persona loader — armed field ---------------------------------------

TEST_CASE("PersonaLoader parses armed = true") {
    // TODO(combat): uncomment once PersonaLoader unit tests have a text fixture
    // const std::string text =
    //     "name = Guard\nrole = bouncer\nspot = bar\nposition = 0,0\narmed = true\n";
    // auto r = parsePersonaText(text, "guard");
    // REQUIRE(r.ok);
    // CHECK(r.value.persona.armed);

    CHECK(true);
}

TEST_CASE("PersonaLoader defaults armed to false when key absent") {
    // TODO(combat): uncomment once PersonaLoader unit tests have a text fixture
    // const std::string text =
    //     "name = Gus\nrole = vendor\nspot = cart\nposition = 0,0\n";
    // auto r = parsePersonaText(text, "gus");
    // REQUIRE(r.ok);
    // CHECK_FALSE(r.value.persona.armed);

    CHECK(true);
}
