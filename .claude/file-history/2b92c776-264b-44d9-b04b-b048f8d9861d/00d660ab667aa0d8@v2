#pragma once

#include <string>
#include <vector>

#include "SandboxMap.hpp"
#include "Trait.hpp"
#include "WorldGenValidate.hpp"

namespace llm_npc {

// LLM world generation (plan: llm-world-generation): prompt builders and
// output parsers. The model NEVER places anything — it fills schemas; the
// validators (WorldGenValidate/validateMap) gate the one load path, and
// invalid output goes back with renderRetryFeedback (cap 3 attempts).
//
// Output contracts (closed-world: the catalogs are the whole vocabulary):
//   cast:    {"characters": ["<.persona text>", ...]}
//   map:     the SandboxMap JSON itself (the sandbox editor's own format)
//   village: {"map": {<SandboxMap>}, "characters": ["<.persona text>", ...]}
//            where generated NPCs are placed via source
//            "character:gen_<name-slug>" and stored under that id.
constexpr int kWorldGenMaxAttempts = 3;
constexpr int kWorldGenMaxCharacters = 8;

// The generation "vocabulary" blocks included in every prompt: part ids by
// category, palette ids, trait ids (cast) / piece ids with footprints and
// bounds (map).
std::string castVocabulary(const std::vector<TraitDef>& traitLibrary);
std::string mapVocabulary();

// System prompts for one generation kind. `description` is the player's
// request, appended by the caller as the user turn (with any retry
// feedback appended after it).
std::string buildCastPrompt(const std::vector<TraitDef>& traitLibrary);
std::string buildMapPrompt();
std::string buildVillagePrompt(const std::vector<TraitDef>& traitLibrary);

// Stable store id for a generated character name: "gen_" + slug.
std::string generatedCharacterId(const std::string& name);

// Parsers for the three contracts. False on malformed JSON/shape; catalog
// validity is the VALIDATORS' job, not the parsers'.
bool parseGeneratedCast(const std::string& json,
                        std::vector<GeneratedCharacter>& out);
bool parseGeneratedVillage(const std::string& json, SandboxMap& mapOut,
                           std::vector<GeneratedCharacter>& castOut);

// Cross-checks a village: every "character:gen_*" npc source in the map
// must match a generated character's id. Returned as MapErrors so they
// join the retry feedback.
std::vector<MapError> validateVillageLinks(
    const SandboxMap& map, const std::vector<GeneratedCharacter>& cast);

}  // namespace llm_npc
