#pragma once

#include <random>
#include <string>
#include <vector>

#include "Math.hpp"
#include "WorldState.hpp"

namespace llm_npc {

// One validated fact proposal from a conversation, plus who learned what.
// direction "npc_learned": the NPC heard it from the player (source
// "player", knower: the NPC). direction "player_learned": the NPC told
// the player (source: the NPC, knowers: NPC and player).
struct ProposedFact {
    std::string subject;   // already normalized
    std::string content;
    bool playerLearned = false;
};

// An agent's position for the propagation pass.
struct AgentAt {
    std::string name;
    Vec3 position{};
};

// Lowercases and squeezes a free-form subject into the normalized key
// space [a-z0-9_] the journal compares exactly ("The Bakery Fire!" ->
// "the_bakery_fire").
std::string normalizeSubject(const std::string& raw);

// Stable fact id from the normalized subject + content (idempotent
// commits: the same statement always hashes to the same id).
std::string factIdFor(const std::string& subject, const std::string& content);

// Parses the model's proposal — a strict JSON array of at most 2 objects
// {"subject","content","direction"} — into validated ProposedFacts.
// PROPOSE -> VALIDATE -> COMMIT: this is the validate step; anything
// malformed (not JSON, wrong types, unknown direction, empty/oversized
// fields, > 2 facts) yields an empty result and the caller commits
// nothing. LLM output never writes to the bus directly.
std::vector<ProposedFact> validateProposedFacts(const std::string& json);

// Commits one validated proposal heard in a conversation with `npcName`,
// stamping the current world time and granting knowledge per the
// direction. Returns the committed record (existing facts keep their
// original stamp/source) so callers can persist it.
KnownFact commitFact(WorldState& state, const ProposedFact& fact,
                     const std::string& npcName);

// One conservative propagation tick: for each pair of agents within
// kGossipRadius, at most ONE fact the teller has known longer than
// kGossipMinAgeSeconds transfers to a hearer who lacks it, with
// probability kGossipChance per pair. Facts only ever flip knowledge
// bits on the shared bus — there is no NPC-to-NPC message channel.
// Returns how many transfers happened (telemetry/tests).
int propagateGossip(WorldState& state, const std::vector<AgentAt>& agents,
                    std::mt19937& rng);

constexpr float kGossipRadius = 6.f;

// The PLAYER lane: facts the player introduced into a conversation. Unchanged,
// and deliberately so — these are things a player said out loud and expects to
// travel, and the whole point of the split below is that it does not touch
// this rate.
constexpr double kGossipMinAgeSeconds = 30.0 * 60.0;  // 30 game minutes
constexpr float kGossipChance = 0.35f;

// The TESTIMONY lane: everything the player did not say — seeded witness
// accounts and evidence, sourced with a persona name or "town".
//
// Both numbers come from a measurement, not from taste. A match day is 8 real
// minutes of investigation and the tick runs every 15 seconds, so a day is
// ~32 propagation ticks; the world clock compresses ~11 in-world hours into
// those 8 minutes, so one real second is about 82 game seconds.
//
// At the player rate testimony homogenises well inside day one: every resident
// ends up holding every account, and per-NPC distinctness — the thing that
// makes interrogating a second person worth doing — disappears.
//
// 6 game hours is roughly 260 real seconds, so about 17 of the day's 32 ticks
// pass before an account can move at all. That is not an arbitrary delay: it
// is the difference between "I saw something last night" and "everyone has
// heard about it", which is a real distinction in a town and the reason
// second-hand testimony is weaker evidence. `test_gossip.cpp` measures a full
// day with 20 clustered agents and asserts somebody is still in the dark.
constexpr double kTestimonyMinAgeSeconds = 6.0 * 3600.0;  // 6 game hours
constexpr float kTestimonyChance = 0.05f;

}  // namespace llm_npc
