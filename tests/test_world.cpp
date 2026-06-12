#include <string>

#include "LlmClient.hpp"
#include "Persona.hpp"
#include "World.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A client pointed at a port nothing listens on; proximity tests never send
// requests, the NPCs just need a client reference to exist.
LlmClient& idleClient() {
    static LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    return client;
}

// Builds an NPC named `name` standing at (x, z).
Npc makeNpc(const std::string& name, float x, float z) {
    Persona p;
    p.name = name;
    Npc npc(p, idleClient());
    npc.setPlacement(Vec3{x, 0.f, z}, 0.f, "spot");
    return npc;
}

}  // namespace

TEST_CASE("nearestNpcWithin picks the closest NPC in range") {
    World world(City::makeDowntown());
    world.addNpc(makeNpc("near", 2.f, 0.f));
    world.addNpc(makeNpc("nearer", 1.f, 0.f));
    world.addNpc(makeNpc("far", 50.f, 0.f));

    const int idx = world.nearestNpcWithin(Vec3{0.f, 0.f, 0.f}, 10.f);
    REQUIRE(idx == 1);
    CHECK(world.npcs()[static_cast<std::size_t>(idx)].persona().name == "nearer");
}

TEST_CASE("nearestNpcWithin returns -1 when everyone is out of range") {
    World world(City::makeDowntown());
    world.addNpc(makeNpc("far", 50.f, 50.f));
    CHECK(world.nearestNpcWithin(Vec3{0.f, 0.f, 0.f}, 10.f) == -1);
}

TEST_CASE("nearestNpcWithin returns -1 for an empty world") {
    World world(City::makeDowntown());
    CHECK(world.nearestNpcWithin(Vec3{0.f, 0.f, 0.f}, 1000.f) == -1);
}

TEST_CASE("nearestNpcWithin ignores height differences") {
    World world(City::makeDowntown());
    world.addNpc(makeNpc("high", 3.f, 4.f));  // 5 away on the ground plane
    CHECK(world.nearestNpcWithin(Vec3{0.f, 99.f, 0.f}, 5.f) == 0);
    CHECK(world.nearestNpcWithin(Vec3{0.f, 99.f, 0.f}, 4.9f) == -1);
}

TEST_CASE("ten personas place every NPC outside building walls") {
    // Placement sanity: an NPC inside an AABB could never be approached.
    const City city = City::makeDowntown();
    World world(city);
    world.addNpc(makeNpc("a", -70.f, -36.f));  // baker, outside the bakery front
    for (const auto& npc : world.npcs()) {
        CHECK_FALSE(city.circleIntersectsAny(npc.position().x, npc.position().z, 0.3f));
    }
}
