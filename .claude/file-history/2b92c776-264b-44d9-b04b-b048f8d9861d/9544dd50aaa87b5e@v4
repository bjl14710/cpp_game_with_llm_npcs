// Tests for the gun-aim fix (issue #91, plan Part A): the player's aim is one
// authoritative look vector (yaw AND pitch) shared by the camera and the
// weapon, and the shot leaves from eye height along that full 3D direction —
// so it can never drift out of sync with the crosshair (the moonwalk class).
#include <cmath>

#include "Math.hpp"
#include "Weapon.hpp"
#include "World.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {
constexpr float kPi = 3.14159265358979323846f;
Vec3 horizForward(float yawDeg) {  // the WASD movement forward, for parity checks
    const float yr = yawDeg * kPi / 180.f;
    return Vec3{std::sin(yr), 0.f, std::cos(yr)};
}
}  // namespace

// Criterion: at zero pitch, aim equals the horizontal forward that WASD uses —
// so aim and movement never disagree on the horizontal plane.
TEST_CASE("lookDirection matches the horizontal forward at zero pitch") {
    for (float yaw : {0.f, 37.f, 90.f, 180.f, 271.f, -45.f}) {
        const Vec3 look = lookDirection(yaw, 0.f);
        const Vec3 fwd = horizForward(yaw);
        CHECK(look.x == doctest::Approx(fwd.x).epsilon(1e-4));
        CHECK(look.y == doctest::Approx(0.f));
        CHECK(look.z == doctest::Approx(fwd.z).epsilon(1e-4));
    }
}

// Criterion: yaw/pitch convention matches the camera basis:
// yaw 0 -> +Z, yaw 90 -> +X, pitch up -> +Y, pitch down -> -Y.
TEST_CASE("lookDirection yaw/pitch convention") {
    const Vec3 f = lookDirection(0.f, 0.f);
    CHECK(f.x == doctest::Approx(0.f));
    CHECK(f.z == doctest::Approx(1.f));

    const Vec3 r = lookDirection(90.f, 0.f);
    CHECK(r.x == doctest::Approx(1.f));
    CHECK(r.z == doctest::Approx(0.f).epsilon(1e-4));

    CHECK(lookDirection(0.f, 45.f).y > 0.f);   // looking up
    CHECK(lookDirection(0.f, -45.f).y < 0.f);  // looking down

    // Always unit length, at any pitch.
    for (float p : {-75.f, -30.f, 0.f, 30.f, 75.f}) {
        CHECK(length(lookDirection(20.f, p)) == doctest::Approx(1.f).epsilon(1e-4));
    }
}

// Criterion: a shot honors pitch and originates at eye height. Previously the
// projectile spawned at the feet (y==0) with the direction flattened to y==0.
TEST_CASE("playerAttack fires along the full 3D aim from eye height") {
    World world(City::makeDowntown());
    world.player().position = Vec3{0.f, 0.f, 0.f};
    world.playerSwitchWeapon(WeaponKind::Pistol);

    REQUIRE(world.playerAttack(lookDirection(0.f, -30.f)));
    REQUIRE(world.projectiles().size() == 1);
    const Projectile& p = world.projectiles().front();
    CHECK(p.direction.y < 0.f);                               // aimed downward
    CHECK(p.position.y == doctest::Approx(kEyeHeight));       // left from the eye
    CHECK(length(p.direction) == doctest::Approx(1.f).epsilon(1e-4));
}

// Regression: aim is derived from the live look every shot, not a stored
// constant — pitched-up and level shots must differ in their vertical aim.
TEST_CASE("player aim is derived per-shot, not stored") {
    World up(City::makeDowntown());
    up.player().position = Vec3{0.f, 0.f, 0.f};
    up.playerSwitchWeapon(WeaponKind::Pistol);
    REQUIRE(up.playerAttack(lookDirection(0.f, 40.f)));

    World level(City::makeDowntown());
    level.player().position = Vec3{0.f, 0.f, 0.f};
    level.playerSwitchWeapon(WeaponKind::Pistol);
    REQUIRE(level.playerAttack(lookDirection(0.f, 0.f)));

    const float upY = up.projectiles().front().direction.y;
    const float levelY = level.projectiles().front().direction.y;
    CHECK(upY > 0.f);
    CHECK(levelY == doctest::Approx(0.f));
    CHECK(upY != doctest::Approx(levelY));
}

// shortestAngleDelta drives the creator preview's ease-back-to-front (#93):
// it must always turn the short way and stay in [-180, 180].
TEST_CASE("shortestAngleDelta takes the short way around") {
    CHECK(shortestAngleDelta(0.f, 90.f) == doctest::Approx(90.f));
    CHECK(shortestAngleDelta(0.f, 270.f) == doctest::Approx(-90.f));   // short way is back
    CHECK(shortestAngleDelta(350.f, 10.f) == doctest::Approx(20.f));   // forward across 0
    CHECK(shortestAngleDelta(10.f, 350.f) == doctest::Approx(-20.f));  // back across 0
    CHECK(shortestAngleDelta(45.f, 45.f) == doctest::Approx(0.f));
    for (float from : {-200.f, -30.f, 0.f, 175.f, 540.f}) {
        for (float to : {-360.f, -1.f, 88.f, 200.f, 719.f}) {
            const float d = shortestAngleDelta(from, to);
            CHECK(d >= -180.f);
            CHECK(d <= 180.f);
        }
    }
}
