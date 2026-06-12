#include <cmath>
#include <string>

#include "City.hpp"
#include "FakeOllama.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "doctest.h"

using namespace llm_npc;
using llm_npc_test::FakeOllama;

namespace {

Persona testPersona() {
    Persona p;
    p.name = "Dana";
    p.role = "beat cop";
    return p;
}

// Drives the NPC through ask() + onReplyArrived() with a crafted reply so its
// action tag is parsed and applied, without waiting on the fake server's own
// stream. The NPC submits via its own client; we just need that client backed
// by a running FakeOllama so ask() succeeds. Returns the cleaned surfaced text.
std::string instruct(Npc& npc, const std::string& replyContent) {
    const std::uint64_t id = npc.ask("(player instruction)");
    ChatReply reply;
    reply.id = id;
    reply.ok = true;
    reply.content = replyContent;
    return npc.onReplyArrived(reply).value_or("");
}

}  // namespace

TEST_CASE("Action tag from a reply sets behavior and is stripped from text") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);

    const std::string text = instruct(npc, "On your six. [[ACTION: follow]]");
    CHECK(text == "On your six.");
    CHECK(npc.behavior() == NpcAction::Follow);
    // The stripped tag must not leak into remembered context either.
    REQUIRE(npc.history().size() == 2);
    CHECK(npc.history()[1].content == "On your six.");
}

TEST_CASE("Follow walks the NPC toward the player and halts at a short distance") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    City city;  // empty, building-free world: isolates movement from collisions

    npc.setPlacement(Vec3{0.f, 0.f, 0.f}, 0.f, "");
    const Vec3 player{0.f, 0.f, 10.f};
    instruct(npc, "Coming. [[ACTION: follow]]");

    const float startDist = distanceXZ(npc.position(), player);
    for (int i = 0; i < 600; ++i) npc.update(0.05f, player, city);  // ~30s
    const float endDist = distanceXZ(npc.position(), player);

    CHECK(endDist < startDist);          // it actually closed the gap
    CHECK(endDist > 1.5f);               // but stopped a polite distance away
    CHECK(endDist < 3.5f);
    CHECK(npc.facingDeg() == doctest::Approx(0.f).epsilon(0.01));  // faces +Z toward player
}

TEST_CASE("Face turns to the player without moving") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    City city;

    npc.setPlacement(Vec3{0.f, 0.f, 0.f}, 0.f, "");
    const Vec3 player{5.f, 0.f, 0.f};  // due +X of the NPC
    instruct(npc, "Looking. [[ACTION: face]]");

    npc.update(0.05f, player, city);
    CHECK(npc.position().x == doctest::Approx(0.f));
    CHECK(npc.position().z == doctest::Approx(0.f));
    CHECK(npc.facingDeg() == doctest::Approx(90.f).epsilon(0.01));  // +X is yaw 90
}

TEST_CASE("Arrest closes faster than follow and latches a catch") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    City city;
    const Vec3 player{0.f, 0.f, 0.f};

    // Same start, same ticks: the chaser should travel farther than a walker.
    Npc walker(testPersona(), client);
    Npc chaser(testPersona(), client);
    walker.setPlacement(Vec3{0.f, 0.f, 30.f}, 0.f, "");
    chaser.setPlacement(Vec3{0.f, 0.f, 30.f}, 0.f, "");
    instruct(walker, "[[ACTION: follow]]");
    instruct(chaser, "Stop right there! [[ACTION: arrest]]");
    for (int i = 0; i < 5; ++i) {
        walker.update(0.1f, player, city);
        chaser.update(0.1f, player, city);
    }
    CHECK(distanceXZ(chaser.position(), player) < distanceXZ(walker.position(), player));

    // Run the chase to completion: the NPC catches the player and holds.
    for (int i = 0; i < 600; ++i) chaser.update(0.1f, player, city);
    CHECK(chaser.hasCaughtPlayer());
    CHECK(chaser.behavior() == NpcAction::Stop);
}

TEST_CASE("Gestures pose the NPC and expire on their own") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    City city;
    const Vec3 player{0.f, 0.f, 3.f};

    instruct(npc, "Right hand up. [[ACTION: raise_hand]]");
    CHECK(npc.pose() == NpcAction::RaiseHand);

    npc.update(1.0f, player, city);
    CHECK(npc.pose() == NpcAction::RaiseHand);  // still holding after 1s

    for (int i = 0; i < 20; ++i) npc.update(1.0f, player, city);  // +20s, past the hold
    CHECK(npc.pose() == NpcAction::None);

    // A gesture is an overlay: it must not have set a movement behavior.
    CHECK(npc.behavior() == NpcAction::None);
}
