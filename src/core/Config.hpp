#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "LlmClient.hpp"

namespace llm_npc {

// Trim ASCII whitespace from both ends.
std::string trim(const std::string& s);

// Tiny key=value config reader; same shape as cpp_racing_game/config/*.cfg.
// Lines starting with '#' (or trailing '# ...' fragments) are comments.
// Returns an empty map when the file cannot be opened.
std::unordered_map<std::string, std::string> readKv(const std::filesystem::path& path);

// Read an entire file into a string; empty string when unreadable.
std::string slurp(const std::filesystem::path& path);

// Sets a single "key = value" entry in `path` in place, rewriting only the
// matching line and leaving comments and other lines untouched. Appends a new
// "key = value" line if the key isn't already present. Returns false if the
// file cannot be written.
bool setKvValue(const std::filesystem::path& path, const std::string& key,
                const std::string& value);

// Load LLM settings from <configDir>/llm.cfg, falling back to LlmConfig
// defaults for any missing key.
LlmConfig loadLlmConfig(const std::filesystem::path& configDir);

// Visual switches from <configDir>/graphics.cfg. Both default on; a missing
// file or unrecognized value keeps the default.
struct GraphicsConfig {
    bool noirNight = true;  // noir_night: the Neo-Noir night look
    bool rain = true;       // rain: falling rain at night (needs noir_night)
};

// Parses an on/off style flag ("on"/"off", "true"/"false", "yes"/"no",
// "1"/"0", case-insensitive); returns `fallback` for anything else.
bool parseOnOff(const std::string& value, bool fallback);

// Load graphics switches from <configDir>/graphics.cfg.
GraphicsConfig loadGraphicsConfig(const std::filesystem::path& configDir);

}  // namespace llm_npc
