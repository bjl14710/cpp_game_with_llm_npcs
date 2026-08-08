#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Mystery.hpp"
#include "Persona.hpp"

namespace llm_npc {

// Authored mystery templates: the content generateMystery deliberately leaves
// empty (issue #186).
//
// Mystery.cpp picks who died, who did it, where and when, and stops there —
// its comment says quantity of evidence and witnesses "belongs to the storyline
// templates and their validator, not to a number guessed at this layer". This
// is that layer. A storyline supplies the ordered clue chain, the witness
// observations and the supporting-cast slots that turn a skeleton into
// something solvable.
//
// Content lives in storylines/*.storyline in the same `key = value` shape as
// personas/*.persona, traits/*.trait and banks/*.bank, for the reason the line
// bank gives: a night of generated content has to review as a normal text diff
// in a PR. See storylines/README.md for the format.
//
// NOTE: no plan document backs this milestone. Idea 7 never had an /idea run,
// so the shape here is derived from what four finished plans already require of
// it — see .claude/memory/planning-session.md.

// One castable part in the story. The template names the PART; which resident
// fills it is decided at match time by casting (#188), because a template has
// to work on any roster.
struct StorylineRole {
    std::string slotId;  // author's key, e.g. "neighbour"
    // "killer", "witness", "red_herring" or "bystander". Free text rather than
    // an enum: the validator (#187) is where unknown kinds are rejected, and
    // keeping the parser dumb means an added kind touches one file.
    std::string kind;
    std::string note;  // authoring aid; never shown to a player
};

// One beat of the argument the players are meant to reconstruct.
struct StorylineClue {
    // AUTHORED chain order, 1-based — NOT discovery order. win-cutscene.md
    // names this the requirement that is cheap now and expensive to retrofit:
    // a template producing an unordered bag of clues cannot drive a montage
    // that reads as reasoning. Parsed and kept as authored; never re-sorted.
    int order = 0;
    std::string zoneId;   // a Zones.hpp id
    std::string caption;  // <= 140 chars, so it fits a KnownFact
    std::string slotId;   // the role this clue bears on; may be empty
    bool pointsAtKiller = false;  // true clue, or deliberate red herring
};

// What one cast member claims to have seen. Attached to a slot, not a person.
struct StorylineWitness {
    std::string slotId;
    std::string zoneId;
    std::string observed;
    double atHour = 0.0;
};

// One authored mystery.
struct StorylineDef {
    std::string id;
    std::string title;
    // Smallest roster this template can be cast onto. Checked by the validator
    // against the shipped roster, and again at cast time against the live one.
    int minResidents = 0;
    std::vector<StorylineRole> roles;
    std::vector<StorylineClue> clues;
    std::vector<StorylineWitness> witnesses;
};

// Parses one .storyline file. Returns nullopt and appends a named message to
// `errors` when the file cannot yield a usable template; recoverable problems
// append a message and still return the template.
//
// PARSE ONLY. Structural rules — contiguous order, resolvable zones, at least
// one contradiction — are validateStoryline's job (#187). Keeping them apart
// keeps both diffs readable and lets a caller parse a file it intends to
// report on rather than run.
std::optional<StorylineDef> parseStorylineFile(const std::filesystem::path& path,
                                               std::vector<std::string>* errors);

// One structural problem with a template. Modelled on SandboxMap.hpp's
// MapError, and specific enough to be both an author-facing message and
// machine-readable.
struct StorylineError {
    std::string where;   // "clues[2]" / "witnesses[0]" / "roles[1]"
    std::string reason;  // "unknown zone `bakery`"
};

// Every structural problem with `story`, not just the first — the same
// behaviour validateMap has, and for the same reason: an author fixing one
// error at a time through six round trips gives up.
//
// STRUCTURAL ONLY. This does not attempt to prove the clue chain narrows to a
// unique suspect; that is constraint satisfaction, it is deliberately issue
// #189, and it is separate precisely so this can ship without it.
//
// Callers FAIL CLOSED: a template with any error is not offered to the
// generator. A broken storyline must never produce a half-built mystery.
std::vector<StorylineError> validateStoryline(const StorylineDef& story,
                                              int rosterSize);

// The kinds a role may declare. Exposed so the validator's rule and any
// authoring tool read from one list.
const std::vector<std::string>& storylineRoleKinds();

// Who actually filled each of a template's slots this match. Returned rather
// than written into MysterySetup, because the setup is the answer sheet and
// this is the cast list: the role layer and any authoring tool need it, and
// neither should have to read setup.killer to get it.
struct StorylineCast {
    // slotId -> resident name, for every slot the template declared.
    std::vector<std::pair<std::string, std::string>> assignments;

    // The resident filling `slotId`, or empty when the slot went uncast.
    const std::string& residentFor(const std::string& slotId) const;
};

// Fills the evidence and witness vectors generateMystery leaves empty, and
// works out who plays each part.
//
// THE KILLER IS NOT CHOSEN HERE. generateMystery already picked it and
// role-layer.md scopes that decision to there; a template's `kind = killer`
// slot describes the part the already-chosen killer plays, and casting binds
// that slot to setup.killer rather than picking someone. Reassigning it would
// silently break voteIsCorrect.
//
// Deterministic from `seed`, for the same two reasons generateMystery is:
// tests cannot assert on a cast without it, and the host must be able to replay
// a match without shipping the answer. Do not pass World::rng_.
//
// Casts nothing and returns an empty cast when the roster is smaller than the
// template's minResidents — a partial cast would leave clues citing residents
// who do not exist.
StorylineCast castStoryline(const StorylineDef& story,
                            const std::vector<Persona>& roster,
                            MysterySetup& setup, unsigned seed);

// Every .storyline in `dir`, sorted by filename so load order is reproducible
// across machines (directory_iterator order is unspecified).
//
// DEGRADES TO INERT. An unreadable directory yields an empty vector and one
// line on stderr — the same contract ConversationStore, RatingLog and LineBank
// hold: absorb the failure, say so once, never throw into the game loop. A
// missing storylines/ directory must leave the game playable.
std::vector<StorylineDef> loadStorylines(const std::filesystem::path& dir,
                                         std::vector<std::string>* errors = nullptr);

}  // namespace llm_npc
