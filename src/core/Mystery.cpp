#include "Mystery.hpp"

#include "Gossip.hpp"
#include "Zones.hpp"

namespace llm_npc {

// Scaffolded stubs (plan: opening-murder, steps 1-2). generateMystery returns
// an empty setup, seeding writes nothing, and voteIsCorrect answers false for
// everyone — so nothing calls this into a half-built state. Fill these in and
// un-skip tests/test_mystery.cpp in the same commit.

namespace {

// Deterministic, platform-stable, header-free — the same generator
// CharacterParts.cpp uses for randomizeLook, and for the same reason: the
// contract is "seed in, identical result out on every machine", which
// std::mt19937 gives but does not make obvious at the call site.
//
// The gotcha carried over with it: xorshift cannot start at 0, so callers must
// construct as `Rng rng{seed ? seed : 1u}`. Seed 0 is valid input to
// generateMystery, so this is load-bearing, not a nicety.
struct Rng {
    unsigned state;
    unsigned next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    // Uniform pick in [0, n).
    std::size_t pick(std::size_t n) { return n ? next() % n : 0; }
};

}  // namespace

MysterySetup generateMystery(const std::vector<Persona>& roster, unsigned seed) {
    (void)roster;
    (void)seed;
    // TODO(mystery step 1): construct `Rng rng{seed ? seed : 1u}`, pick a
    // victim index, then pick a killer index from the remaining roster so
    // victim != killer without a rejection loop. Pick sceneZoneId from
    // zonesForDowntown(), murderHour from the evening band, and a
    // bodyPosition inside that zone's bounds. Evidence and witness COUNTS are
    // deliberately not decided here — the storyline templates own quantity,
    // driven by their validator rather than guessed at this layer.
    //
    // Roster smaller than 2 cannot produce victim != killer. Return an empty
    // setup and let match start fail loudly; do NOT silently make one person
    // both.
    return {};
}

bool voteIsCorrect(const MysterySetup& setup, const std::string& accused) {
    (void)setup;
    (void)accused;
    // TODO(mystery step 5): exact compare against setup.killer, and nothing
    // else in the codebase may read that field. An empty accusation is false,
    // and so is an empty killer — a match with no ground truth must never
    // report a correct vote.
    return false;
}

void seedMysteryFacts(WorldState& state, const MysterySetup& setup) {
    (void)state;
    (void)setup;
    // TODO(mystery step 2): commit ONE death fact — subject
    // normalizeSubject(victim + " death"), content naming the victim and the
    // zone but never the killer — and grant it to every survivor plus
    // "player". Then commit each witness observation and grant it to that
    // witness ALONE, so a non-witness cannot be asked about what they never
    // saw.
    //
    // Use factIdFor(subject, content) from Gossip.hpp rather than a new hash,
    // so a seeded fact and the same statement arriving later through
    // conversation collapse to one id instead of contradicting each other.
}

void placeBodyClearOfColliders(MysterySetup& setup, const City& city) {
    (void)setup;
    (void)city;
    // TODO(mystery step 3): if city.circleIntersectsAny(x, z, radius) rejects
    // the generated spot, try a bounded number of offsets inside the same
    // zone, then fall back to the zone centre. Bounded, because a zone whose
    // every sample collides must still return — an unreachable body is a match
    // nobody can finish.
    //
    // The radius wanted here is the NPC collision circle, 0.45f. Npc.cpp has
    // it as kNpcRadius but in an anonymous namespace, so it is not reachable
    // from this file: either declare a local constant with a comment pointing
    // at the original, or promote the original. Do not silently pick a
    // different number — a body placed on a radius the NPCs do not use is a
    // body they can stand inside.
}

}  // namespace llm_npc
