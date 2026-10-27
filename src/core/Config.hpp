#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <unordered_map>

#include "LlmClient.hpp"

namespace llm_npc {

// Trim ASCII whitespace from both ends.
std::string trim(const std::string& s);

// Consume a leading UTF-8 byte-order mark (EF BB BF) from `in`, if present.
// Call right after opening a text file: editors on Windows prepend one, and
// left in place it becomes part of the first key. Otherwise rewinds to the
// start so the stream is unchanged.
void skipUtf8Bom(std::istream& in);

// Tiny key=value config reader; same shape as cpp_racing_game/config/*.cfg.
// Lines starting with '#' (or trailing ' # ...' fragments, where the '#' is
// preceded by whitespace) are comments; a '#' inside a value, as in an API key
// "sk-abc#def", is kept. A leading UTF-8 BOM is ignored.
// Returns an empty map when the file cannot be opened.
std::unordered_map<std::string, std::string> readKv(const std::filesystem::path& path);

// Read an entire file into a string; empty string when unreadable.
std::string slurp(const std::filesystem::path& path);

// Sets a single "key = value" entry in `path` in place, rewriting only the
// matching line and leaving comments and other lines untouched. Appends a new
// "key = value" line if the key isn't already present. Returns false, leaving
// the file untouched, if the key or value contains a CR or LF (which would
// inject extra config lines) or if the file cannot be written. The write goes
// to a temp file renamed over `path`, so a failure never truncates the config.
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
