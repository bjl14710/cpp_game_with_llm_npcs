// Group conversations core (plan: group-conversations). The app-side
// wiring (routing, labeled UI) is #122; memory/fact attribution is #123 —
// their stubs stay skipped until those land.
#include <string>
#include <vector>

#include "Gossip.hpp"
#include "GroupSession.hpp"
#include "WorldState.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"
#include "doctest.h"

using namespace llm_npc;

namespace {

GroupSession makeGroup() {
    GroupSession group;
    group.open({3, 7}, {"Marge Holloway", "Officer Dana Brooks"});
    return group;
}

}  // namespace

TEST_CASE("addressing: named participant gets the turn, else round-robin") {
    GroupSession group = makeGroup();
    REQUIRE(group.active());

    // Leading first name addresses; full name also addresses.
    CHECK(group.resolveNextSpeaker("Marge, what do you think?") == 3);
    CHECK(group.resolveNextSpeaker("Officer Dana Brooks — anything to add?") == 7);
    // Mid-sentence mentions do NOT address.
    const int a = group.resolveNextSpeaker("I talked to Marge's regulars today.");
    const int b = group.resolveNextSpeaker("What a town.");
    CHECK(a != b);  // unaddressed messages rotate through both participants
    // A name matching nobody is unaddressed (no error).
    const int c = group.resolveNextSpeaker("Gus, are you here?");
    CHECK((c == 3 || c == 7));
    // Addressing continues the rotation AFTER the addressed speaker.
    CHECK(group.resolveNextSpeaker("Marge?") == 3);
    CHECK(group.resolveNextSpeaker("hm.") == 7);
}

TEST_CASE("NPC-to-NPC turns cap at kMaxConsecutiveNpcTurns") {
    GroupSession group = makeGroup();
    CHECK(group.npcMayTakeFloor());
    group.noteNpcTurn();
    CHECK(group.npcMayTakeFloor());
    group.noteNpcTurn();
    CHECK_FALSE(group.npcMayTakeFloor());  // cap = 2, floor returns to player
    group.notePlayerTurn();
    CHECK(group.npcMayTakeFloor());  // player speaking resets the run
}

TEST_CASE("transcript lines carry speakers and render for prompts") {
    GroupSession group = makeGroup();
    group.addLine("Player", "Morning, both of you.");
    group.addLine("Marge Holloway", "Fresh rye's just out.");
    group.addLine("Officer Dana Brooks", "Noted. For the report.");
    REQUIRE(group.transcript().size() == 3);
    CHECK(group.transcript()[1].speaker == "Marge Holloway");
    const std::string rendered = group.renderTranscript();
    CHECK(rendered.find("Player: Morning, both of you.") != std::string::npos);
    CHECK(rendered.find("Marge Holloway: Fresh rye's just out.") <
          rendered.find("Officer Dana Brooks: Noted. For the report."));
    CHECK(group.nameOf(7) == "Officer Dana Brooks");
}

TEST_CASE("participant removal drops them and closes when empty") {
    GroupSession group = makeGroup();
    group.removeParticipant(3);
    REQUIRE(group.participants().size() == 1);
    CHECK(group.participants()[0] == 7);
    CHECK(group.active());  // one NPC + the player can finish the exchange
    group.removeParticipant(7);
    CHECK_FALSE(group.active());
}

TEST_CASE("mood-gated follow: hostile or angry NPCs refuse" ) {
    // Issue #120: the follow action is rejected in VALIDATION when the
    // NPC's state/mood says no — whatever the model emitted.
    LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    Persona p;
    p.name = "Benny";

    Npc willing(p, client);
    willing.ask("follow me");
    willing.onReplyArrived(ChatReply{1, true, "Sure thing.\n[[ACTION: follow]]", ""});
    CHECK(willing.behavior() == NpcAction::Follow);

    Npc angry(p, client);
    angry.ask("hello");
    angry.onReplyArrived(ChatReply{2, true, "Hmph.\n[[MOOD: angry]]", ""});
    CHECK(angry.mood() == NpcMood::Angry);
    angry.ask("follow me");
    angry.onReplyArrived(ChatReply{3, true, "Fine!\n[[ACTION: follow]] [[MOOD: angry]]", ""});
    CHECK(angry.behavior() != NpcAction::Follow);  // gate held
}

TEST_CASE("per-participant memories and attributed facts") {
    // #123: every group turn goes through the speaker's OWN npc.ask with
    // the labeled transcript — so each participant's private history (the
    // input to their close-time memory summary) contains the conversation
    // from their own seat, and fact extraction runs per participant with
    // facts committed under THEIR name.
    LlmClient client(LlmConfig{/*host=*/"127.0.0.1", /*port=*/1});
    Persona pm;
    pm.name = "Marge";
    Persona pd;
    pd.name = "Dana";
    Npc marge(pm, client);
    Npc dana(pd, client);

    GroupSession group;
    group.open({0, 1}, {"Marge", "Dana"});
    group.addLine("Player", "Morning, both.");

    // Marge's turn: her history records the labeled transcript + HER reply.
    const std::string context1 = group.renderTranscript();
    marge.ask(context1);
    marge.onReplyArrived(ChatReply{1, true, "Fresh rye is out. [[MOOD: happy]]", ""});
    group.addLine("Marge", "Fresh rye is out.");
    REQUIRE(marge.history().size() == 2);
    CHECK(marge.history()[0].content.find("Player: Morning, both.") != std::string::npos);
    CHECK(marge.history()[1].content.find("Fresh rye") != std::string::npos);

    // Dana's turn sees Marge's line inside HER OWN history.
    dana.ask(group.renderTranscript());
    dana.onReplyArrived(ChatReply{2, true, "Noted. [[MOOD: neutral]]", ""});
    CHECK(dana.history()[0].content.find("Marge: Fresh rye is out.") != std::string::npos);
    CHECK(dana.history()[0].content.find("Player: Morning, both.") != std::string::npos);

    // Facts committed during Dana's extraction carry DANA as the source.
    WorldState state;
    ProposedFact proposal;
    proposal.subject = "bakery_bread";
    proposal.content = "The bakery has fresh rye today.";
    proposal.playerLearned = true;
    const KnownFact record = commitFact(state, proposal, dana.persona().name);
    CHECK(record.source == "Dana");
    REQUIRE_FALSE(state.factsKnownBy("Dana").empty());
}
