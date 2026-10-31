#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Math.hpp"
#include "Persona.hpp"

namespace llm_npc {

// A persona plus its placement in the city, loaded from personas/<id>.persona.
struct LoadedPersona {
    std::string id;         // filename stem, e.g. "baker"
    Persona persona;        // identity rendered into the system prompt
    Vec3 position;          // world position on the ground plane (y = 0)
    float facingDeg = 0.f;  // initial facing, degrees around +y (0 = +z)
    std::string spotId;     // landmark tag, e.g. "bakery" or "park"
};

// Result of parsing one persona definition.
struct PersonaParseResult {
    bool ok = false;
    LoadedPersona value;
    std::string error;  // human-readable reason when !ok
};

// Parses one persona definition. Format: a key=value header, then an optional
// '---' line followed by free-form extra system-prompt directives:
//
//   name = Marge Holloway        (required)
//   role = bakery owner
//   traits = warm, gossipy       (comma-separated)
//   style = 1-3 short sentences
//   knowledge = Knows the neighborhood; nothing of world news.
//   spot = bakery
//   position = -40, 28           (x, z world coordinates)
//   facing = 180                 (degrees)
//   ---
//   Everything after the --- line is passed to the model verbatim.
//
// Header lines are taken literally (no comment stripping) so persona text can
// contain any character except a leading 'key =' shape.
PersonaParseResult parsePersonaText(const std::string& text, const std::string& id);

// Parses a single .persona file; the persona id is the filename stem.
PersonaParseResult parsePersonaFile(const std::filesystem::path& path);

// Loads every *.persona file in dir, sorted by filename for a deterministic
// roster order. Files that fail to parse are skipped; their error messages
// are appended to `errors` when it is non-null.
std::vector<LoadedPersona> loadAllPersonas(const std::filesystem::path& dir,
                                           std::vector<std::string>* errors = nullptr);

}  // namespace llm_npc
