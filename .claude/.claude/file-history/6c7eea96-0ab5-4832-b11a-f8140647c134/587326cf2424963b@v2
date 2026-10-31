#include "Config.hpp"

#include <fstream>
#include <sstream>

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
    return cfg;
}

}  // namespace llm_npc
