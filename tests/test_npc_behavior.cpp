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
    p.police = true;  // movement tests below exercise the arrest behavior
    return p;
}

Persona civilianPersona() {
    Persona p;
    p.name = "Marge";
    p.role = "baker";
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

TEST_CASE("Civilian arrest converts to calling the police") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(civilianPersona(), client);
    City city;
    const Vec3 player{0.f, 0.f, 5.f};

    instruct(npc, "You can't do that here! [[ACTION: arrest]]");
    CHECK(npc.lastAction() == NpcAction::CallPolice);
    CHECK(npc.behavior() == NpcAction::None);   // she doesn't chase
    CHECK(npc.pose() == NpcAction::Wave);       // she flags the police down

    const Vec3 before = npc.position();
    for (int i = 0; i < 20; ++i) npc.update(0.1f, player, city);
    CHECK(npc.position().x == doctest::Approx(before.x));  // still no chase
    CHECK(npc.position().z == doctest::Approx(before.z));
}

TEST_CASE("commandArrest sends a police NPC chasing without any reply") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc cop(testPersona(), client);
    City city;
    cop.setPlacement(Vec3{0.f, 0.f, 20.f}, 0.f, "police");

    cop.commandArrest();
    CHECK(cop.behavior() == NpcAction::Arrest);
    for (int i = 0; i < 600; ++i) cop.update(0.1f, Vec3{0.f, 0.f, 0.f}, city);
    CHECK(cop.hasCaughtPlayer());
}

TEST_CASE("commandReturnHome walks the NPC back to its spawn spot") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc cop(testPersona(), client);
    City city;
    cop.setPlacement(Vec3{4.f, 0.f, -36.f}, 90.f, "police");

    // Chase away from home first, then get sent back.
    cop.commandArrest();
    for (int i = 0; i < 100; ++i) cop.update(0.1f, Vec3{0.f, 0.f, 30.f}, city);
    CHECK(distanceXZ(cop.position(), Vec3{4.f, 0.f, -36.f}) > 5.f);

    cop.commandReturnHome();
    CHECK_FALSE(cop.hasCaughtPlayer());
    for (int i = 0; i < 600; ++i) cop.update(0.1f, Vec3{0.f, 0.f, 30.f}, city);
    CHECK(distanceXZ(cop.position(), Vec3{4.f, 0.f, -36.f}) < 1.f);
    CHECK(cop.behavior() == NpcAction::None);
    CHECK(cop.facingDeg() == doctest::Approx(90.f));  // spawn facing restored
}

TEST_CASE("Mood is read from the reply and decays back to neutral") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(civilianPersona(), client);
    City city;
    const Vec3 player{0.f, 0.f, 3.f};

    instruct(npc, "Oh! You shouldn't have. [[MOOD: embarrassed]]");
    CHECK(npc.mood() == NpcMood::Embarrassed);

    for (int i = 0; i < 10; ++i) npc.update(1.0f, player, city);  // 10s: still felt
    CHECK(npc.mood() == NpcMood::Embarrassed);
    for (int i = 0; i < 30; ++i) npc.update(1.0f, player, city);  // 40s total
    CHECK(npc.mood() == NpcMood::Neutral);
}

TEST_CASE("lookAt turns the NPC without moving it") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(civilianPersona(), client);
    npc.setPlacement(Vec3{0.f, 0.f, 0.f}, 0.f, "");

    npc.lookAt(Vec3{-5.f, 0.f, 0.f});  // due -X
    CHECK(npc.facingDeg() == doctest::Approx(-90.f));
    CHECK(npc.position().x == doctest::Approx(0.f));
    CHECK(npc.position().z == doctest::Approx(0.f));
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

TEST_CASE("deriveFacingFromMotion points facing along actual displacement") {
    // Combat movement (flee/hostile) writes position directly without
    // touching facing — the derivation must reorient from motion alone,
    // killing the moonwalk at the source.
    LlmClient client(LlmConfig{"localhost", 1});
    Persona p;
    p.name = "Runner";
    Npc npc(p, client);
    npc.setPlacement(Vec3{0.f, 0.f, 0.f}, 0.f, "spot");

    const Vec3 prev = npc.position();
    npc.position() = Vec3{1.f, 0.f, 0.f};  // moved toward +X in one frame
    npc.deriveFacingFromMotion(prev, 1.f / 60.f);
    CHECK(npc.facingDeg() == doctest::Approx(90.f).epsilon(0.02));  // +X = 90°

    // Diagonal motion toward -Z/+X lands between.
    const Vec3 prev2 = npc.position();
    npc.position() = npc.position() + Vec3{0.5f, 0.f, -0.5f};
    npc.deriveFacingFromMotion(prev2, 1.f / 60.f);
    CHECK(npc.facingDeg() == doctest::Approx(135.f).epsilon(0.02));
}

TEST_CASE("deriveFacingFromMotion ignores sub-threshold drift and the dead") {
    LlmClient client(LlmConfig{"localhost", 1});
    Persona p;
    p.name = "Idler";
    Npc npc(p, client);
    npc.setPlacement(Vec3{0.f, 0.f, 0.f}, 0.f, "spot");
    npc.lookAt(Vec3{-5.f, 0.f, 0.f});  // deliberate standing turn: -X = -90°
    const float held = npc.facingDeg();

    // A nudge far below walking pace must not flip the held facing.
    const Vec3 prev = npc.position();
    npc.position() = npc.position() + Vec3{0.001f, 0.f, 0.f};
    npc.deriveFacingFromMotion(prev, 1.f / 60.f);
    CHECK(npc.facingDeg() == doctest::Approx(held));

    // Corpses keep their final pose no matter how far they are moved.
    npc.takeDamage(1000);
    REQUIRE(npc.combatState() == NpcState::Dead);
    const Vec3 prev2 = npc.position();
    npc.position() = npc.position() + Vec3{0.f, 0.f, 9.f};
    npc.deriveFacingFromMotion(prev2, 1.f / 60.f);
    CHECK(npc.facingDeg() == doctest::Approx(held));
}

TEST_CASE("schedules: the injected clock decides the destination") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    const City city = City::makeDowntown();
    Npc npc(civilianPersona(), client);
    npc.setPlacement(Vec3{-70.f, 0.f, -36.f}, 0.f, "bakery");
    npc.setSchedule({{5.f, 12.f, Vec3{-70.f, 0.f, -36.f}, "baking bread"},
                     {12.f, 14.f, Vec3{-8.f, 0.f, -6.f}, "lunch at the plaza"}});
    const Vec3 farPlayer{90.f, 0.f, 90.f};

    // Morning: already at the bakery spot -> stays put, activity reported.
    npc.update(0.1f, farPlayer, city, 7.f);
    CHECK(npc.activity() == "baking bread");
    CHECK(distanceXZ(npc.position(), Vec3{-70.f, 0.f, -36.f}) < 0.5f);

    // Same NPC, different injected hour -> walks toward the plaza. The NPC
    // has no clock of its own; only the parameter changed.
    const float before = distanceXZ(npc.position(), Vec3{-8.f, 0.f, -6.f});
    for (int i = 0; i < 60; ++i) npc.update(0.1f, farPlayer, city, 12.5f);
    CHECK(npc.activity() == "lunch at the plaza");
    CHECK(distanceXZ(npc.position(), Vec3{-8.f, 0.f, -6.f}) < before);

    // No clock (negative hour) -> schedule dormant, no movement.
    const Vec3 held = npc.position();
    npc.update(0.5f, farPlayer, city);
    CHECK(distanceXZ(npc.position(), held) < 0.001f);
}

TEST_CASE("schedules: a nearby player pauses the walk; no schedule idles") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    const City city = City::makeDowntown();
    Npc npc(civilianPersona(), client);
    npc.setPlacement(Vec3{10.f, 0.f, 10.f}, 0.f, "plaza");
    npc.setSchedule({{0.f, 24.f, Vec3{10.f, 0.f, -40.f}, "errands"}});

    // Player right next to the NPC: the routine politely waits.
    const Vec3 nearPlayer{11.f, 0.f, 10.f};
    npc.update(0.5f, nearPlayer, city, 10.f);
    CHECK(distanceXZ(npc.position(), Vec3{10.f, 0.f, 10.f}) < 0.001f);
    CHECK(npc.activity() == "errands");  // the label still reports

    // Player far away: the walk resumes.
    npc.update(0.5f, Vec3{90.f, 0.f, 90.f}, city, 10.f);
    CHECK(distanceXZ(npc.position(), Vec3{10.f, 0.f, 10.f}) > 0.5f);

    // An NPC with no schedule never moves on its own.
    Npc idler(civilianPersona(), client);
    idler.setPlacement(Vec3{0.f, 0.f, 10.f}, 0.f, "plaza");
    idler.update(0.5f, Vec3{90.f, 0.f, 90.f}, city, 10.f);
    CHECK(distanceXZ(idler.position(), Vec3{0.f, 0.f, 10.f}) < 0.001f);
    CHECK(idler.activity().empty());
}

TEST_CASE("schedules: midnight-wrapping ranges and edge hours") {
    const std::vector<ScheduleEntry> schedule = {
        {22.f, 6.f, Vec3{}, "night shift"},
        {6.f, 10.f, Vec3{}, "morning"},
    };
    CHECK(activeScheduleIndex(schedule, 23.f) == 0);
    CHECK(activeScheduleIndex(schedule, 2.f) == 0);
    CHECK(activeScheduleIndex(schedule, 6.f) == 1);   // start-inclusive
    CHECK(activeScheduleIndex(schedule, 5.99f) == 0); // end-exclusive
    CHECK(activeScheduleIndex(schedule, 15.f) == -1); // gap: nothing active
}

// ---- NPCs stay on the ground (bug 3) ---------------------------------------
//
// An NPC following a jumping player used to climb into the air, and one
// fleeing a player on a roof used to burrow. The cause was a split authority:
// the step came from a THREE-dimensional normalize while the distance test
// beside it used distanceXZ, so the vertical gap entered the movement,
// City::resolveMovement wrote it through with `pos.y = to.y`, and nothing put
// it back. The drift accumulated and never recovered.

TEST_CASE("steerXZ never produces vertical motion, however high the target") {
    // The primitive the fix rests on. Directly comparable to distanceXZ, which
    // every one of these movers already used for its distance test.
    const Vec3 ground{0.f, 0.f, 0.f};
    for (const float height : {0.f, 1.7f, 12.f, -8.f, 100.f}) {
        CAPTURE(height);
        const Vec3 step = steerXZ(ground, Vec3{3.f, height, 4.f});
        CHECK(step.y == doctest::Approx(0.f));
        CHECK(length(step) == doctest::Approx(1.f));
        // Direction on the plane is unaffected by how high the target is.
        CHECK(step.x == doctest::Approx(0.6f));
        CHECK(step.z == doctest::Approx(0.8f));
    }
}

TEST_CASE("steerXZ on a vertically stacked pair yields no motion, not a nan") {
    const Vec3 step = steerXZ(Vec3{5.f, 0.f, 5.f}, Vec3{5.f, 20.f, 5.f});
    CHECK(step.x == doctest::Approx(0.f));
    CHECK(step.y == doctest::Approx(0.f));
    CHECK(step.z == doctest::Approx(0.f));
}

TEST_CASE("an NPC following a JUMPING player stays on the ground") {
    // The reported symptom. The player's y is their feet height and rises
    // while jumping, so before the fix the follow step carried it upward every
    // frame for as long as the player kept jumping.
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    // (8, 8) is open ground. (0, 0) is INSIDE a building in makeDowntown, and
    // an NPC placed there cannot move at all — resolveMovement refuses every
    // step, so a movement test starting there passes by being stuck.
    npc.setPlacement(Vec3{8.f, 0.f, 8.f}, 0.f, "plaza");
    instruct(npc, "On your six. [[ACTION: follow]]");
    REQUIRE(npc.behavior() == NpcAction::Follow);

    const City city = City::makeDowntown();
    Vec3 playerPos{28.f, 0.f, 28.f};
    for (int frame = 0; frame < 400; ++frame) {
        // A player bouncing between the ground and the top of a jump.
        playerPos.y = (frame % 40 < 20) ? 1.6f : 0.f;
        npc.update(1.f / 60.f, playerPos, city, 12.f);
        // Deliberately NO snapToGround here. It runs every frame in
        // production and would erase the drift before it accumulated, so a
        // test that called it would pass with the bug reverted — which is
        // exactly what behaviour QA caught this test doing. Without the net,
        // this fails the moment any of the six sites goes back to a 3D
        // normalize. The net has its own test below.
        CAPTURE(frame);
        REQUIRE(npc.position().y == doctest::Approx(0.f));
    }
    // It moved horizontally, so this is not passing by standing still. Measured
    // from the start rather than as distance-to-player: a building between the
    // two will legitimately stop it short, and that is not what is under test.
    CAPTURE(npc.position().x);
    CAPTURE(npc.position().z);
    CHECK(distanceXZ(npc.position(), Vec3{8.f, 0.f, 8.f}) > 1.f);
}

TEST_CASE("an NPC following a player on a ROOF gathers below, never climbs") {
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    // (8, 8) is open ground. (0, 0) is INSIDE a building in makeDowntown, and
    // an NPC placed there cannot move at all — resolveMovement refuses every
    // step, so a movement test starting there passes by being stuck.
    npc.setPlacement(Vec3{8.f, 0.f, 8.f}, 0.f, "plaza");
    instruct(npc, "On your six. [[ACTION: follow]]");
    REQUIRE(npc.behavior() == NpcAction::Follow);

    const City city = City::makeDowntown();
    const Vec3 onARoof{28.f, 9.f, 28.f};  // nine metres up
    for (int frame = 0; frame < 400; ++frame) {
        npc.update(1.f / 60.f, onARoof, city, 12.f);
        CAPTURE(frame);  // no snapToGround: see the note above
        REQUIRE(npc.position().y == doctest::Approx(0.f));
    }
}

TEST_CASE("snapToGround recovers an NPC that is already off the ground") {
    // A save, a hand-built fixture, or any future mover that forgets. The
    // authority is unconditional, so one frame is enough.
    FakeOllama fake;
    LlmClient client({/*host=*/"127.0.0.1", /*port=*/fake.port()});
    Npc npc(testPersona(), client);
    npc.setPlacement(Vec3{4.f, 37.f, 4.f}, 0.f, "plaza");
    npc.snapToGround();
    CHECK(npc.position().y == doctest::Approx(0.f));
    CHECK(npc.position().x == doctest::Approx(4.f));  // horizontal untouched
    CHECK(npc.position().z == doctest::Approx(4.f));
}
