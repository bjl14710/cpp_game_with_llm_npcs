#include "Mystery.hpp"

#include <cstdio>

#include "Gossip.hpp"
#include "Journal.hpp"
#include "Rng.hpp"
#include "Zones.hpp"

namespace llm_npc {

// The whole of plan opening-murder is implemented here: generation (#178),
// fact seeding (#179), body placement and starting the victim dead (#180), and
// the vote check with its compile-gated reveal (#181).

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

// Attributed source for facts nobody in particular first said. WorldState
// sources are otherwise "player" or a persona name; the death is common
// knowledge on arrival and has no first teller.
constexpr const char* kTownSource = "town";

// The NPC collision circle, duplicated rather than guessed at: a body placed on
// a radius the NPCs do not use is a body they can stand inside.
//
// This value exists FOUR times in the codebase and has already diverged, so
// "change both" is not sufficient guidance:
//   Npc.cpp:14        kNpcRadius  0.45f   anonymous namespace
//   Mystery.cpp       kBodyRadius 0.45f   here
//   SandboxMap.cpp:18 kBodyRadius 0.45f   anonymous namespace
//   World.cpp:66      kNpcRadius  0.4f    function-local, projectile hit tests
// None is reachable from the others. If the collision circle changes, the first
// three move together and World.cpp's is a separate decision.
constexpr float kBodyRadius = 0.45f;

// Grid resolution for the placement search. 24 steps over a 48-unit block is a
// sample every 2 units, which is finer than the NPC radius and so cannot skip
// over a gap wide enough to stand in.
constexpr int kLatticeSteps = 24;

// The zone table entry for `zoneId`, or nullptr. Zones.hpp exposes zoneAt and
// zoneName but no lookup by id, and a linear scan over nine boxes does not
// justify adding one to the header.
const ZoneDef* findZone(const std::string& zoneId) {
    for (const ZoneDef& zone : zonesForDowntown()) {
        if (zone.id == zoneId) return &zone;
    }
    return nullptr;
}

// Would a body at (x, z) be inside something solid? feetY 0 because the body
// lies on the ground — nothing here can be standing on a roof.
bool collides(const City& city, float x, float z) {
    return city.circleIntersectsAny(x, z, kBodyRadius, 0.f);
}

// KnownFact::content is capped at 140 chars — validateProposedFacts rejects
// anything longer outright. Seeded content is authored by us rather than by a
// model, so an over-length string is an authoring mistake, and dropping the
// fact would silently lose a clue. Clamp and say so once, matching the
// degrade-to-inert contract ConversationStore, RatingLog and LineBank use:
// absorb the failure, one line on stderr, never throw into the game loop.
std::string clampContent(std::string content) {
    constexpr std::size_t kMaxContent = 140;
    if (content.size() <= kMaxContent) return content;
    std::fprintf(stderr,
                 "[llm_npc] mystery: fact content over %zu chars, truncated: "
                 "\"%.60s...\"\n",
                 kMaxContent, content.c_str());
    content.resize(kMaxContent);
    return content;
}

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
    // A setup with no killer has no ground truth, and a match with no ground
    // truth reporting a correct vote is the worst failure this function has.
    // Checked before the comparison so empty == empty can never be a win.
    if (setup.killer.empty() || accused.empty()) return false;

    // Exact compare. No trimming, no case folding, no normalisation: this is
    // the same key WorldState::grantKnowledge and the roster use, and a lenient
    // match here would mean "marge holloway" convicts Marge while
    // "Marge  Holloway" does not — a rule players cannot see.
    return setup.killer == accused;
}

#ifdef LLM_NPC_REVEAL_KILLER
// Debug reveal. COMPILE-GATED, and deliberately not a config key or a runtime
// flag: anything readable at runtime can be flipped by whoever is hosting, and
// host advantage stops being theoretical the moment the answer is one keypress
// away. A build that can host a networked match must not define this.
//
// One of three sanctioned readers of setup.killer, enumerated in the leak rule
// at the top of Mystery.hpp — the others are voteIsCorrect above and
// castStoryline in Storyline.cpp. Anywhere else it appears in rendering,
// dialogue or networking code is a bug, and a reviewer can find them all by
// grepping one field name.
const std::string& revealKillerForDebug(const MysterySetup& setup) {
    return setup.killer;
}
#endif

void seedMysteryFacts(WorldState& state, const MysterySetup& setup,
                      const std::vector<Persona>& roster) {
    if (setup.victim.empty()) return;  // no mystery; nothing to tell anyone

    const double now = state.number("world_time_seconds");

    // ---- the death: common knowledge ------------------------------------
    //
    // Names the victim and the place, never the hour and never the killer.
    // The hour is a real clue — it is what the opening cutscene shows and what
    // an alibi is checked against — so handing it to the whole town for free
    // would remove the one thing that makes the testimony worth cross-checking.
    KnownFact death;
    death.subject = normalizeSubject(setup.victim + " death");
    death.content = clampContent(setup.victim + " was found dead at " +
                                 zoneName(setup.sceneZoneId) + ".");
    death.factId = factIdFor(death.subject, death.content);
    death.source = kTownSource;
    death.learnedAtSeconds = now;
    state.addFact(death);

    // Everyone alive, plus the player. The victim is skipped: granting the
    // dead knowledge of their own death is harmless but meaningless, and
    // leaving them out keeps factsKnownBy() honest for anything that later
    // iterates agents.
    for (const Persona& person : roster) {
        if (person.name == setup.victim) continue;
        state.grantKnowledge(person.name, death.factId);
    }
    state.grantKnowledge("player", death.factId);

    // ---- what each witness saw: private to that witness ------------------
    //
    // Granted to the witness ALONE. If everyone started knowing every
    // observation there would be nothing to investigate — this grant is the
    // difference between a mystery and a briefing.
    for (const Witness& witness : setup.witnesses) {
        if (witness.agent.empty()) continue;

        KnownFact seen;
        // The subject is WHO the testimony is about, falling back to the
        // speaker when it is about no one in particular.
        //
        // This choice is the detective mechanic (#214). Journal.hpp flags
        // "one subject, more than one content", so only facts sharing a
        // subject can ever be compared:
        //
        //   about set    -> "<person> whereabouts". Two residents giving
        //                   different accounts of one person collide, and both
        //                   sides are flagged. This is the case the mode is
        //                   built on, and it is why `about` exists.
        //   about empty  -> "<speaker> testimony". One resident changing their
        //                   own story is still caught. Not a fallback so much
        //                   as the other half of the mechanic.
        //
        // An alibi needs no special handling: a witness whose `about` is their
        // own name lands on their own whereabouts subject, so their account
        // and any sighting of them collide automatically. Nothing marks which
        // one is the lie — deliberately.
        const std::string topic = witness.about.empty()
                                      ? witness.agent + " testimony"
                                      : witness.about + " whereabouts";
        seen.subject = normalizeSubject(topic);
        // Self-testimony reads as a claim, not a sighting — "saw themselves"
        // is nonsense, and an alibi is the one testimony a player should hear
        // in the speaker's own voice. "they" throughout: the roster has no
        // pronoun field and inventing one from a name would misgender people.
        const bool selfTestimony = witness.about == witness.agent;
        seen.content = clampContent(
            selfTestimony
                // ASCII only. Fact content is rendered with raylib's built-in
                // bitmap font, which has no glyph outside ASCII 32-126 — an
                // em-dash here reaches the Journal as a literal "?".
                ? witness.agent + " says they were at " +
                      zoneName(witness.sawZoneId) + " around " +
                      clockLabel(witness.atHour * 3600.0) + ", " +
                      witness.observed + "."
                : witness.agent + " saw " + witness.observed + " at " +
                      zoneName(witness.sawZoneId) + " around " +
                      clockLabel(witness.atHour * 3600.0) + ".");
        seen.factId = factIdFor(seen.subject, seen.content);
        seen.source = witness.agent;
        seen.learnedAtSeconds = now;
        state.addFact(seen);
        state.grantKnowledge(witness.agent, seen.factId);
    }
}

void placeBodyClearOfColliders(MysterySetup& setup, const City& city) {
    const ZoneDef* scene = findZone(setup.sceneZoneId);
    if (scene == nullptr) {
        // Unknown scene zone: leave the position alone rather than move the
        // body somewhere arbitrary. Loud, because generateMystery only ever
        // picks from zonesForDowntown() — reaching here means a hand-built
        // setup or a renamed zone.
        std::fprintf(stderr,
                     "[llm_npc] mystery: unknown scene zone \"%s\" — body left "
                     "where it was generated\n",
                     setup.sceneZoneId.c_str());
        return;
    }

    if (!collides(city, setup.bodyPosition.x, setup.bodyPosition.z)) return;

    // A LATTICE SCAN, not the plan's "bounded retries". Retries would be
    // random and could miss a clear spot that exists; this walks every cell of
    // a fixed grid over the zone and takes the CLEAREST one nearest to where
    // generation intended, so the result stays close to the intended scene and
    // is fully deterministic without needing a seed.
    //
    // Cost is kLatticeSteps^2 collision tests against ~35 buildings, once per
    // match. That is nothing, and it buys a guarantee that sampling does not.
    const float stepX = (scene->maxX - scene->minX) / (kLatticeSteps + 1);
    const float stepZ = (scene->maxZ - scene->minZ) / (kLatticeSteps + 1);

    bool found = false;
    float bestX = 0.f, bestZ = 0.f, bestDistSq = 0.f;
    for (int ix = 1; ix <= kLatticeSteps; ++ix) {
        for (int iz = 1; iz <= kLatticeSteps; ++iz) {
            const float x = scene->minX + stepX * ix;
            const float z = scene->minZ + stepZ * iz;
            if (collides(city, x, z)) continue;

            const float dx = x - setup.bodyPosition.x;
            const float dz = z - setup.bodyPosition.z;
            const float distSq = dx * dx + dz * dz;
            if (!found || distSq < bestDistSq) {
                found = true;
                bestX = x;
                bestZ = z;
                bestDistSq = distSq;
            }
        }
    }

    if (found) {
        setup.bodyPosition.x = bestX;
        setup.bodyPosition.z = bestZ;
        return;
    }

    // Every cell solid. The plan's fallback is the zone centre, and this keeps
    // it — but says so, because the centre is NOT guaranteed clear. The bakery
    // block's centre sits exactly on Marge's Bakery's south edge, so a silent
    // fallback there would put the body inside a wall and look like a
    // placement rather than a failure.
    setup.bodyPosition.x = (scene->minX + scene->maxX) * 0.5f;
    setup.bodyPosition.z = (scene->minZ + scene->maxZ) * 0.5f;
    std::fprintf(stderr,
                 "[llm_npc] mystery: no clear spot in zone \"%s\" — body placed "
                 "at the zone centre, which may be inside a building\n",
                 setup.sceneZoneId.c_str());
}

bool startVictimDead(std::vector<Npc>& npcs, const MysterySetup& setup) {
    if (setup.victim.empty()) return false;
    for (Npc& npc : npcs) {
        if (npc.persona().name != setup.victim) continue;
        npc.markDeadAtStart();
        return true;
    }
    // The victim is not in the world. The caller should treat this as a failed
    // match start rather than carry on: a mystery whose victim is walking
    // around is not a mystery.
    std::fprintf(stderr,
                 "[llm_npc] mystery: victim \"%s\" is not in the NPC roster\n",
                 setup.victim.c_str());
    return false;
}

}  // namespace llm_npc
