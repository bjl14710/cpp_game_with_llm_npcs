#include "Config.hpp"

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace llm_npc {

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::unordered_map<std::string, std::string> readKv(const std::filesystem::path& path) {
    std::unordered_map<std::string, std::string> out;
    std::ifstream in(path);
    if (!in) return out;
    std::string line;
    while (std::getline(in, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        out[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return out;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream o;
    o << in.rdbuf();
    return o.str();
}

bool setKvValue(const std::filesystem::path& path, const std::string& key,
                const std::string& value) {
    std::vector<std::string> lines;
    bool replaced = false;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            // Match a non-comment line whose first token is exactly `key`,
            // followed by optional spaces and '='. The '=' check keeps a key
            // from matching a longer one (e.g. "keep" vs "keep_alive").
            const auto begin = line.find_first_not_of(" \t");
            if (begin != std::string::npos && line[begin] != '#') {
                const std::string rest = line.substr(begin);
                if (rest.rfind(key, 0) == 0) {
                    const std::string after = trim(rest.substr(key.size()));
                    if (!after.empty() && after.front() == '=') {
                        line = key + " = " + value;
                        replaced = true;
                    }
                }
            }
            lines.push_back(line);
        }
    }
    if (!replaced) lines.push_back(key + " = " + value);

    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    for (const std::string& l : lines) out << l << "\n";
    return true;
}

LlmConfig loadLlmConfig(const std::filesystem::path& configDir) {
    LlmConfig cfg;
    auto kv = readKv(configDir / "llm.cfg");
    if (auto it = kv.find("host"); it != kv.end()) cfg.host = it->second;
    if (auto it = kv.find("port"); it != kv.end()) cfg.port = std::stoi(it->second);
    if (auto it = kv.find("model"); it != kv.end()) cfg.model = it->second;
    if (auto it = kv.find("temperature"); it != kv.end()) cfg.temperature = std::stod(it->second);
    if (auto it = kv.find("request_timeout_s"); it != kv.end()) {
        cfg.requestTimeoutSeconds = std::stoi(it->second);
    }
    if (auto it = kv.find("keep_alive"); it != kv.end()) cfg.keepAlive = it->second;
    if (auto it = kv.find("think"); it != kv.end()) cfg.think = it->second;
    if (auto it = kv.find("provider"); it != kv.end()) cfg.provider = it->second;
    if (auto it = kv.find("base_url"); it != kv.end()) cfg.baseUrl = it->second;
    // The key's env var defaults per provider, so `provider = anthropic` alone
    // reads NPC_GAME_ANTHROPIC_API_KEY and never an OpenRouter key by accident.
    // An explicit api_key_env still wins.
    if (auto it = kv.find("api_key_env"); it != kv.end()) {
        cfg.apiKeyEnv = it->second;
    } else if (cfg.provider == "anthropic") {
        cfg.apiKeyEnv = "NPC_GAME_ANTHROPIC_API_KEY";
    }
    if (auto it = kv.find("anthropic_model"); it != kv.end()) cfg.anthropicModel = it->second;
    if (auto it = kv.find("anthropic_base_url"); it != kv.end()) {
        cfg.anthropicBaseUrl = it->second;
    }
    if (auto it = kv.find("anthropic_max_tokens"); it != kv.end()) {
        cfg.anthropicMaxTokens = std::stoi(it->second);
    }
    if (auto it = kv.find("anthropic_history_turns"); it != kv.end()) {
        cfg.anthropicHistoryTurns = std::stoi(it->second);
    }
    if (auto it = kv.find("anthropic_token_budget"); it != kv.end()) {
        cfg.anthropicTokenBudget = std::stoull(it->second);
    }
    if (auto it = kv.find("anthropic_retry_backoff_ms"); it != kv.end()) {
        cfg.anthropicRetryBackoffMs = std::stoi(it->second);
    }
    if (auto it = kv.find("anthropic_timeout_s"); it != kv.end()) {
        cfg.anthropicTimeoutSeconds = std::stoi(it->second);
    }

    // Line bank. Anything other than an explicit "on" leaves it off, so a
    // typo disables the feature rather than half-enabling it.
    if (auto it = kv.find("line_bank"); it != kv.end()) cfg.lineBank = it->second == "on";
    if (auto it = kv.find("line_bank_threshold"); it != kv.end()) {
        cfg.lineBankThreshold = std::stof(it->second);
    }
    if (auto it = kv.find("line_bank_cps"); it != kv.end()) {
        cfg.lineBankCps = std::stoi(it->second);
    }

    // Resolve the cloud API key without ever storing it in version control:
    // prefer the named environment variable, then fall back to the gitignored
    // config/secrets.cfg. Left empty (cloud disabled) when neither is set.
    if (const char* env = std::getenv(cfg.apiKeyEnv.c_str()); env && *env) {
        cfg.apiKey = env;
    } else {
        auto secrets = readKv(configDir / "secrets.cfg");
        if (auto it = secrets.find("api_key"); it != secrets.end()) {
            // secrets.cfg has one key slot shared by every cloud provider, so a
            // key left over from another provider must not be sent to this one:
            // an Anthropic key going out as a Bearer token to a third-party
            // endpoint is the one way it could leave the machine in the clear.
            // Anthropic keys are prefixed sk-ant-; nothing else is.
            const bool anthropicKey = it->second.rfind("sk-ant-", 0) == 0;
            const bool anthropicProvider = cfg.provider == "anthropic";
            if (anthropicKey == anthropicProvider) {
                cfg.apiKey = it->second;
            } else {
                std::cerr << "[llm_npc] ignoring config/secrets.cfg api_key: it is "
                          << (anthropicKey ? "an Anthropic key" : "not an Anthropic key")
                          << " but provider = " << cfg.provider << "\n";
            }
        }
    }
    return cfg;
}

}  // namespace llm_npc
