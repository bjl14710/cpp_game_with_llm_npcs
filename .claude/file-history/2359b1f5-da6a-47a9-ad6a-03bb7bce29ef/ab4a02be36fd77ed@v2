#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

// A client pointed at a port nothing listens on — combat tests never make
// LLM requests.
LlmClient& idleClient() {
    static LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    return client;
}

Npc makeCivilian() {
    Persona p;
    p.name = "Gus";
    p.role = "hot-dog vendor";
    p.armed = false;
    Npc npc(p, idleClient());
    npc.setPlacement({0.f, 0.f, 0.f}, 0.f, "cart");
    return npc;
}

Npc makeCop() {
    Persona p;
    p.name = "Officer Dana Brooks";
    p.role = "beat cop";
    p.armed = true;
    Npc npc(p, idleClient());
    npc.setPlacement({0.f, 0.f, -36.f}, 0.f, "police");
    return npc;
}

}  // namespace

// ---- NpcState transitions -----------------------------------------------

TEST_CASE("Civilian transitions to Fleeing when hit") {
    auto npc = makeCivilian();
    CHECK(npc.combatState() == NpcState::Idle);
    npc.takeDamage(10);
    CHECK(npc.combatState() == NpcState::Fleeing);
    CHECK(npc.hp() == 90);
}

TEST_CASE("Cop transitions to Hostile when hit") {
    auto npc = makeCop();
    CHECK(npc.combatState() == NpcState::Idle);
    npc.takeDamage(10);
    CHECK(npc.combatState() == NpcState::Hostile);
    CHECK(npc.hp() == 90);
}

TEST_CASE("NPC transitions to Dead when HP reaches 0") {
    auto npc = makeCivilian();
    npc.takeDamage(100);
    CHECK(npc.combatState() == NpcState::Dead);
    CHECK(npc.hp() == 0);
}

TEST_CASE("takeDamage is no-op on Dead NPC") {
    auto npc = makeCivilian();
    npc.takeDamage(100);  // kills
    REQUIRE(npc.combatState() == NpcState::Dead);
    npc.takeDamage(50);   // should be ignored
    CHECK(npc.hp() == 0);
    CHECK(npc.combatState() == NpcState::Dead);
}

TEST_CASE("NPC HP does not go negative") {
    auto npc = makeCivilian();
    npc.takeDamage(200);
    CHECK(npc.hp() == 0);
}

TEST_CASE("Multiple partial hits accumulate correctly") {
    auto npc = makeCivilian();
    npc.takeDamage(30);
    CHECK(npc.hp() == 70);
    npc.takeDamage(30);
    CHECK(npc.hp() == 40);
    CHECK(npc.combatState() == NpcState::Fleeing);
}

TEST_CASE("Armed NPC stays Hostile after multiple hits") {
    auto npc = makeCop();
    npc.takeDamage(20);
    CHECK(npc.combatState() == NpcState::Hostile);
    npc.takeDamage(20);
    CHECK(npc.combatState() == NpcState::Hostile);
    CHECK(npc.hp() == 60);
}

TEST_CASE("isArmed matches persona armed flag") {
    CHECK_FALSE(makeCivilian().isArmed());
    CHECK(makeCop().isArmed());
}
