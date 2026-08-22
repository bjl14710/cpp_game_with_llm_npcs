// Vote-phase plaza gather (issue #156): a temporary destination override
// layered over the authored schedule.
//
// The two properties that matter are that the override OUTRANKS the schedule
// while it is set, and that releasing it puts every resident straight back on
// their authored day with nothing remembered. The second is the one that
// produces a bug worth a test: an NPC still standing in the plaza on day two
// is not a quirk.
#include <cmath>
#include <string>
#include <vector>

#include "City.hpp"
#include "LlmClient.hpp"
#include "Persona.hpp"
#include "World.hpp"
#include "Zones.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

LlmClient& idleClient() {
    static LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    return client;
}

// Where the test residents live and where their schedule sends them.
//
// BOTH ARE ON THE STREET, deliberately. The obvious choice — the bakery's own
// footprint — is inside a solid collider (City.cpp authors "bakery" as
// x [-84, -56], z [-64, -40]), so residents placed there cannot walk anywhere
// and every movement assertion in this file would pass or fail for reasons
// that have nothing to do with the gather. The street north of the block is
// clear: blocks span z [-88, -40], so z = -30 is between them.
constexpr float kHomeX = -70.f;
constexpr float kHomeZ = -30.f;
constexpr float kShiftX = -66.f;
constexpr float kShiftZ = -30.f;

// A resident who works a morning shift a few steps down the street.
Npc makeResident(const std::string& name, float x, float z) {
    Persona p;
    p.name = name;
    Npc npc(p, idleClient());
    npc.setPlacement(Vec3{x, 0.f, z}, 0.f, "spot");
    npc.setSchedule(
        {ScheduleEntry{6.f, 12.f, Vec3{kShiftX, 0.f, kShiftZ}, "baking bread"}});
    return npc;
}

// Runs the world's NPCs forward far enough to walk anywhere in downtown.
// The player is parked out of range so kSchedulePause never trips.
void walkFor(World& world, float seconds, float hour) {
    const Vec3 farAway{500.f, 0.f, 500.f};
    for (float t = 0.f; t < seconds; t += 0.1f) {
        for (Npc& npc : world.npcs()) npc.update(0.1f, farAway, world.city(), hour);
    }
}

float distXZ(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

TEST_CASE("every living resident gets a plaza slot and walks to it") {
    World world(City::makeDowntown());
    for (int i = 0; i < 6; ++i) {
        world.addNpc(makeResident("R" + std::to_string(i), kHomeX + i, kHomeZ));
    }
    world.setPlazaGather(true);
    for (const Npc& npc : world.npcs()) REQUIRE(npc.gatherTarget().has_value());

    walkFor(world, 120.f, 8.f);  // 08:00 — the bakery shift is active
    const Vec3 plaza = zoneCentre("plaza");
    for (const Npc& npc : world.npcs()) {
        // They ended up in the plaza rather than at the bakery the schedule
        // asked for: the override outranks it.
        CHECK(distXZ(npc.position(), plaza) < 12.f);
    }
}

TEST_CASE("the gather ring clears Gus's cart at the plaza centre") {
    // The cart is a solid collider spanning x [-3, 3], z [-2, 2] (City.cpp).
    // A gather point AT the computed plaza centre would be inside it, which is
    // the whole reason plazaGatherSpot exists.
    for (int i = 0; i < 21; ++i) {
        const Vec3 spot = plazaGatherSpot(i, 21);
        CHECK((std::fabs(spot.x) > 3.f || std::fabs(spot.z) > 2.f));
        // ...and still well inside the plaza block, which spans +-24.
        CHECK(std::fabs(spot.x - zoneCentre("plaza").x) < 24.f);
        CHECK(std::fabs(spot.z - zoneCentre("plaza").z) < 24.f);
    }
}

TEST_CASE("gather slots are distinct so residents do not stack on one point") {
    std::vector<Vec3> spots;
    for (int i = 0; i < 21; ++i) spots.push_back(plazaGatherSpot(i, 21));
    for (std::size_t a = 0; a < spots.size(); ++a) {
        for (std::size_t b = a + 1; b < spots.size(); ++b) {
            CHECK(distXZ(spots[a], spots[b]) > 0.5f);
        }
    }
}

TEST_CASE("degenerate index and count arguments still give a usable spot") {
    // Zones.hpp promises callers never need a special case for these, so the
    // promise gets a test. No caller passes them today: this exists so that
    // the first one that does finds the contract already pinned rather than
    // discovering it by dividing by zero.
    const int indices[] = {-100, -1, 0, 1, 5};
    const int counts[] = {-5, -1, 0, 1, 2, 21};
    for (const int index : indices) {
        for (const int count : counts) {
            const Vec3 spot = plazaGatherSpot(index, count);
            CAPTURE(index);
            CAPTURE(count);
            CHECK(std::isfinite(spot.x));
            CHECK(std::isfinite(spot.z));
            // Never inside Gus's cart...
            CHECK((std::fabs(spot.x) > 3.f || std::fabs(spot.z) > 2.f));
            // ...and never outside the plaza block.
            CHECK(std::fabs(spot.x - zoneCentre("plaza").x) < 24.f);
            CHECK(std::fabs(spot.z - zoneCentre("plaza").z) < 24.f);
        }
    }
}

TEST_CASE("the dead are not sent to the plaza") {
    World world(City::makeDowntown());
    world.addNpc(makeResident("Alive", kHomeX, kHomeZ));
    world.addNpc(makeResident("Dead", kHomeX + 2.f, kHomeZ));
    world.npcs()[1].takeDamage(1000);
    REQUIRE(world.npcs()[1].combatState() == NpcState::Dead);

    world.setPlazaGather(true);
    CHECK(world.npcs()[0].gatherTarget().has_value());
    CHECK_FALSE(world.npcs()[1].gatherTarget().has_value());
}

TEST_CASE("releasing the gather returns everyone to the authored schedule") {
    World world(City::makeDowntown());
    world.addNpc(makeResident("Marge", kHomeX, kHomeZ));
    world.setPlazaGather(true);
    walkFor(world, 120.f, 8.f);
    REQUIRE(distXZ(world.npcs()[0].position(), zoneCentre("plaza")) < 12.f);

    // The Resolution -> Investigation rollover.
    world.setPlazaGather(false);
    CHECK_FALSE(world.npcs()[0].gatherTarget().has_value());

    walkFor(world, 200.f, 8.f);
    // Back at the bakery the schedule named, not parked in the plaza.
    CHECK(distXZ(world.npcs()[0].position(), Vec3{kShiftX, 0.f, kShiftZ}) < 2.f);
}

TEST_CASE("releasing clears the override on the dead too") {
    // A resident who died holding a target must not still be holding it when
    // the next day starts, or a later revive/respawn walks a corpse's ghost to
    // the plaza.
    World world(City::makeDowntown());
    world.addNpc(makeResident("Marge", kHomeX, kHomeZ));
    world.setPlazaGather(true);
    REQUIRE(world.npcs()[0].gatherTarget().has_value());
    world.npcs()[0].takeDamage(1000);
    world.setPlazaGather(false);
    CHECK_FALSE(world.npcs()[0].gatherTarget().has_value());
}

TEST_CASE("a resident with no schedule at all still gathers") {
    // Not every persona authors a schedule. "Send every surviving NPC" means
    // every one, and the pre-#156 code returned early on an empty schedule.
    World world(City::makeDowntown());
    Persona p;
    p.name = "Drifter";
    Npc npc(p, idleClient());
    npc.setPlacement(Vec3{kHomeX, 0.f, kHomeZ}, 0.f, "spot");
    world.addNpc(std::move(npc));

    world.setPlazaGather(true);
    walkFor(world, 120.f, 8.f);
    CHECK(distXZ(world.npcs()[0].position(), zoneCentre("plaza")) < 12.f);
}

TEST_CASE("the activity label still reports the authored day while gathered") {
    // The schedule is out-ranked, not suspended: a resident walking to the
    // vote is still "baking bread" as far as the nameplate is concerned,
    // because personas/*.persona was never rewritten.
    World world(City::makeDowntown());
    world.addNpc(makeResident("Marge", kHomeX, kHomeZ));
    world.setPlazaGather(true);
    walkFor(world, 5.f, 8.f);
    CHECK(world.npcs()[0].activity() == "baking bread");
}

TEST_CASE("an NPC with no gather target moves exactly as it did before") {
    // The regression guard for the whole change: with the override unset, the
    // schedule walk must be untouched.
    World world(City::makeDowntown());
    world.addNpc(makeResident("Marge", kHomeX, kHomeZ));
    walkFor(world, 200.f, 8.f);
    CHECK(distXZ(world.npcs()[0].position(), Vec3{kShiftX, 0.f, kShiftZ}) < 2.f);
}
