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

// Load LLM settings from <configDir>/llm.cfg, falling back to LlmConfig
// defaults for any missing key.
LlmConfig loadLlmConfig(const std::filesystem::path& configDir);

}  // namespace llm_npc
