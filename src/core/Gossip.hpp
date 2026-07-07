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
constexpr double kGossipMinAgeSeconds = 30.0 * 60.0;  // 30 game minutes
constexpr float kGossipChance = 0.35f;

}  // namespace llm_npc
