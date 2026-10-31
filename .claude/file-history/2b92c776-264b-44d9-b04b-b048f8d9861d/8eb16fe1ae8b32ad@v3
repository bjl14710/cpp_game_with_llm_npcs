#pragma once

#include <string>
#include <vector>

#include "SandboxMap.hpp"
#include "Trait.hpp"

namespace llm_npc {

// Validation layer for LLM-generated content (plan: llm-world-generation).
// THE deliverable of that feature: nothing generated may load except
// through these checks, and their error strings are fed back to the model
// verbatim on retry (cap 3) — specific, actionable, never auto-repaired.
// Map validation REUSES validateMap from SandboxMap.hpp; this header adds
// the cast side and the retry-feedback envelope.

// One generated character: persona text in the EXISTING .persona format
// (parsePersonaText is the validator), plus the look/trait ids it names —
// closed-world: the catalogs are the model's entire vocabulary.
struct GeneratedCharacter {
    std::string personaText;
};

struct CastError {
    std::string where;   // "characters[1]"
    std::string reason;  // "unknown part id 'hair_dreads' (nearest: hair_braids)"
};

// Validates a generated cast — the persona parses (the existing parser IS
// the validator, errors verbatim), the look validates with a nearest-id
// suggestion on unknown parts, trait ids exist in the library, names are
// unique. Returns EVERY problem; the strings double as retry feedback.
std::vector<CastError> validateCast(const std::vector<GeneratedCharacter>& cast,
                                    const std::vector<TraitDef>& traitLibrary);

// Extracts the first balanced JSON object/array from raw model output
// (fences and prose tolerated; strings and escapes respected; never a
// partial extraction).
bool extractJsonObject(const std::string& raw, std::string& jsonOut);

// Renders every error as the retry-feedback block appended to the next
// generation prompt (cap 3 retries; after that, plain failure — no
// partial loads, no auto-repair).
std::string renderRetryFeedback(const std::vector<MapError>& mapErrors,
                                const std::vector<CastError>& castErrors);

}  // namespace llm_npc
