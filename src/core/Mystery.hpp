#pragma once

#include <string>
#include <vector>

#include "City.hpp"
#include "Math.hpp"
#include "Persona.hpp"
#include "WorldState.hpp"

namespace llm_npc {

// The opening murder: ground truth for one detective match (plan:
// opening-murder).
//
// At match start one resident is already dead. This picks who, picks who did
// it, decides where and when, and records what there is to find — as a real
// data structure the vote can be checked against rather than as prose.
//
// Two existing pieces make this smaller than it sounds. NpcState::Dead already
// renders a collapsed figure (RaylibRenderer suppresses the mood emote for it —
// "The dead don't emote"), so no corpse work is needed. And KnownFact already
// has the right shape for seeded knowledge, so the town learning about the
// death is a handful of grantKnowledge calls rather than a new channel.
//
// THE LEAK RULE. MysterySetup is host-only and has three defences:
//
//   1. It never enters WorldState. WorldState is the thing designed to be
//      shared and gossiped; only the CONSEQUENCES of the mystery go in.
//   2. It is never serialized. WorldSnapshot carries players and NPCs, and
//      adding mystery fields would ship the answer to every client.
//   3. `killer` is read through voteIsCorrect() only — one place to audit, so
//      a stray setup.killer in rendering or dialogue code stands out in review.
//
// THE ACCEPTED v1 LIMITATION, stated plainly: with no concealment layer you can
// walk up to the killer and ask, and they will not deny it. That is deliberate.
// The deliverable here is a mystery the vote can be CHECKED against. Making it
// unsolvable-by-asking is the role layer; making it solvable-by-reasoning is
// the storyline templates.

// Something findable at a place. `pointsAtKiller` distinguishes a true clue
// from a deliberate red herring, so a storyline template can plant both without
// the generator having to know which is which.
struct Evidence {
    std::string id;           // stable key, unique within one setup
    std::string zoneId;       // where it is found; a Zones.hpp id
    std::string description;  // <= 140 chars, so it fits a KnownFact content
    bool pointsAtKiller = false;
};

// A resident who saw something. The observation is what they can be made to
// say; it is NOT automatically true and NOT automatically shared — seeding
// grants it to this witness alone.
struct Witness {
    std::string agent;      // roster persona name
    std::string sawZoneId;  // a Zones.hpp id
    std::string observed;   // <= 140 chars
    double atHour = 0.0;    // world hour, [0, 24)
};

// The authoritative answer for one match. Host-only. Never serialized, never
// rendered, never written to WorldState.
struct MysterySetup {
    std::string victim;
    std::string killer;
    std::string sceneZoneId;
    Vec3 bodyPosition{};
    double murderHour = 0.0;
    std::vector<Evidence> evidence;
    std::vector<Witness> witnesses;
};

// Builds the ground truth for a match from the roster.
//
// DETERMINISTIC by requirement, not by preference. Two reasons:
//
//   1. Tests. "Given seed 12345, the victim is X" is the only way to assert on
//      generation without mocking randomness at every call site.
//   2. Multiplayer. The host generates and clients never receive it, so a
//      reproducible seed is what lets a match be replayed or audited without
//      shipping the answer over the wire.
//
// It follows that World::rng_ (the mt19937 used for combat accuracy rolls) must
// NOT be reused here: sharing it would make mystery generation depend on how
// many shots had been fired, which destroys both properties above. The
// implementation uses the same hand-rolled xorshift32 as CharacterParts.cpp,
// which is platform-stable and header-free — including its gotcha, that the
// state cannot start at 0.
//
// Seed 0 is valid input and must produce a valid setup.
MysterySetup generateMystery(const std::vector<Persona>& roster, unsigned seed);

// True only when `accused` is the killer. THE ONLY SUPPORTED READ of
// setup.killer — see defence 3 in the leak rule above. Comparison is exact on
// the persona name, the same key WorldState and the roster use.
bool voteIsCorrect(const MysterySetup& setup, const std::string& accused);

// Writes the CONSEQUENCES of the mystery onto the world bus: the death fact
// that everyone knows, and each witness's observation granted to that witness
// alone.
//
// Never writes anything naming the killer as the killer — a test asserts this
// over the committed facts, because this function is the one place where the
// host's private answer sits next to the shared bus.
//
// Idempotent: WorldState::addFact is first-teller-wins, so seeding twice
// commits the same ids and changes nothing.
void seedMysteryFacts(WorldState& state, const MysterySetup& setup);

// Moves setup.bodyPosition off any collider so the body is reachable.
//
// SPLIT OUT FROM generateMystery ON PURPOSE. The plan lists three functions and
// also requires that bodyPosition be walkable, but walkability needs a City and
// generateMystery must stay pure to be a deterministic unit under test. So the
// generator picks a spot inside sceneZoneId from the seed alone, and this
// second call — which the match-start path makes, and the determinism tests do
// not — nudges it clear of the map.
//
// Bounded retries, then the zone centre. Never leaves the body unreachable, and
// never loops unbounded on a zone that happens to be solid.
void placeBodyClearOfColliders(MysterySetup& setup, const City& city);

}  // namespace llm_npc
