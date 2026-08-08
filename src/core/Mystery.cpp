#include "Mystery.hpp"

#include "Gossip.hpp"
#include "Zones.hpp"

namespace llm_npc {

// generateMystery is implemented (plan: opening-murder, step 1). Seeding, the
// vote check and the collision search are still stubs: they write nothing,
// answer false for everyone, and move nothing, so nothing calls this into a
// half-built state. Fill each in and un-skip its cases in
// tests/test_mystery.cpp in the same commit.

namespace {

// The murder happens the night BEFORE the match, so the town already knows
// when the players arrive and MatchClock's day one can start clean at 09:00.
//
// The band is [20:00, 24:00) and stops short of midnight deliberately. A window
// that wrapped past 00:00 would make every downstream comparison ("was this
// sighting before or after the murder?") a two-case problem for no narrative
// gain, and clockLabel would render a time that reads as the morning OF the
// match rather than the night before it.
constexpr double kMurderHourStart = 20.0;
constexpr double kMurderHourEnd = 24.0;

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

    // A fraction in [0, 1) with 1/kUnitSteps granularity.
    //
    // Integer-then-divide rather than a direct cast of `next()`, because the
    // integer stays exact on every platform and the single division that
    // follows is IEEE-defined. That keeps the cross-platform determinism
    // contract the whole struct exists for.
    double unit() {
        constexpr unsigned kUnitSteps = 100000u;
        return static_cast<double>(next() % kUnitSteps) / kUnitSteps;
    }
};

}  // namespace

MysterySetup generateMystery(const std::vector<Persona>& roster, unsigned seed) {
    MysterySetup setup;

    // Fewer than two people cannot produce victim != killer. Returning an empty
    // setup lets match start fail loudly; making one person both would produce
    // a match that runs and is incoherent, which is strictly worse.
    if (roster.size() < 2) return setup;

    Rng rng{seed ? seed : 1u};

    const std::size_t victimIndex = rng.pick(roster.size());

    // Pick the killer from the roster MINUS the victim: draw in [0, n-1) and
    // step over the victim's slot. No rejection loop, so the number of rng
    // draws does not depend on the seed — which matters, because every value
    // drawn after this point would otherwise shift.
    std::size_t killerIndex = rng.pick(roster.size() - 1);
    if (killerIndex >= victimIndex) ++killerIndex;

    setup.victim = roster[victimIndex].name;
    setup.killer = roster[killerIndex].name;

    const std::vector<ZoneDef>& zones = zonesForDowntown();
    const ZoneDef& scene = zones[rng.pick(zones.size())];
    setup.sceneZoneId = scene.id;

    setup.murderHour =
        kMurderHourStart + rng.unit() * (kMurderHourEnd - kMurderHourStart);

    // Uniform inside the scene zone's half-open bounds. unit() is [0, 1), so
    // the point is always strictly below max and zoneAt agrees with
    // sceneZoneId — the two are generated separately and a test pins that they
    // do not drift apart.
    //
    // This spot may well be inside a building: the nine zones ARE the downtown
    // blocks, and blocks hold solid AABBs. Clearing it needs a City, which this
    // function deliberately does not take, so match start calls
    // placeBodyClearOfColliders afterwards.
    setup.bodyPosition = {
        scene.minX + static_cast<float>(rng.unit()) * (scene.maxX - scene.minX),
        0.f,
        scene.minZ + static_cast<float>(rng.unit()) * (scene.maxZ - scene.minZ)};

    // Evidence and witnesses stay empty. Quantity belongs to the storyline
    // templates and their validator, not to a number guessed at this layer.
    return setup;
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
