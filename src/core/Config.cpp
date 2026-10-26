#include "Config.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <system_error>
#include <type_traits>
#include <vector>

namespace llm_npc {

namespace {

// Whole-string integer parse: false on empty input, trailing junk, or overflow.
bool parseIntStrict(const std::string& text, int& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(text.c_str(), &end, 10);
    if (errno == ERANGE || *end != '\0' || v < INT_MIN || v > INT_MAX) return false;
    out = static_cast<int>(v);
    return true;
}

// Whole-string floating-point parse that also rejects nan/inf, which strtod
// would otherwise accept and which would poison every later comparison.
bool parseDoubleStrict(const std::string& text, double& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || *end != '\0' || !std::isfinite(v)) return false;
    out = v;
    return true;
}

// Reads `key` from `kv` into `field` when present and numeric. A value that
// does not parse keeps the field's current (default) value and warns, so a
// typo in llm.cfg degrades the setting instead of crashing startup.
template <typename T>
void readNumber(const std::unordered_map<std::string, std::string>& kv,
                const std::string& key, T& field) {
    const auto it = kv.find(key);
    if (it == kv.end()) return;
    bool ok = false;
    if constexpr (std::is_integral_v<T>) {
        int v = 0;
        ok = parseIntStrict(it->second, v);
        if (ok) field = static_cast<T>(v);
    } else {
        double v = 0.0;
        ok = parseDoubleStrict(it->second, v);
        if (ok) field = static_cast<T>(v);
    }
    if (!ok) {
        std::cerr << "[llm_npc] llm.cfg: ignoring non-numeric " << key << " = \""
                  << it->second << "\" — keeping the default\n";
    }
}

}  // namespace

void skipUtf8Bom(std::istream& in) {
    char head[3] = {0, 0, 0};
    in.read(head, 3);
    if (in.gcount() == 3 && static_cast<unsigned char>(head[0]) == 0xEF &&
        static_cast<unsigned char>(head[1]) == 0xBB &&
        static_cast<unsigned char>(head[2]) == 0xBF) {
        return;
    }
    in.clear();
    in.seekg(0);
}

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
    skipUtf8Bom(in);
    std::string line;
    while (std::getline(in, line)) {
        // A '#' opens a comment only at the start of the line or after
        // whitespace, so values such as an API key "sk-abc#def" survive.
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '#' &&
                (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
                line.erase(i);
                break;
            }
        }
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
    // A newline in either half would start a new config line, letting one
    // value inject arbitrary keys (e.g. a hostile model name adding base_url).
    const auto hasLineBreak = [](const std::string& s) {
        return s.find_first_of("\r\n") != std::string::npos;
    };
    if (hasLineBreak(key) || hasLineBreak(value)) return false;

    std::vector<std::string> lines;
    bool replaced = false;
    {
        std::ifstream in(path);
        skipUtf8Bom(in);
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

    // Write a sibling temp file and rename it over the original, so a failed
    // or interrupted write cannot leave the config truncated.
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        for (const std::string& l : lines) out << l << "\n";
        out.close();
        if (!out) {
            std::error_code ignored;
            std::filesystem::remove(tmp, ignored);
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

LlmConfig loadLlmConfig(const std::filesystem::path& configDir) {
    LlmConfig cfg;
    auto kv = readKv(configDir / "llm.cfg");
    if (auto it = kv.find("host"); it != kv.end()) cfg.host = it->second;
    readNumber(kv, "port", cfg.port);
    if (auto it = kv.find("model"); it != kv.end()) cfg.model = it->second;
    readNumber(kv, "temperature", cfg.temperature);
    readNumber(kv, "request_timeout_s", cfg.requestTimeoutSeconds);
    readNumber(kv, "max_reply_tokens", cfg.maxReplyTokens);
    if (auto it = kv.find("keep_alive"); it != kv.end()) cfg.keepAlive = it->second;
    if (auto it = kv.find("think"); it != kv.end()) cfg.think = it->second;
    if (auto it = kv.find("provider"); it != kv.end()) cfg.provider = it->second;
    if (auto it = kv.find("base_url"); it != kv.end()) cfg.baseUrl = it->second;
    if (auto it = kv.find("api_key_env"); it != kv.end()) cfg.apiKeyEnv = it->second;

    // Line bank. Anything other than an explicit "on" leaves it off, so a
    // typo disables the feature rather than half-enabling it.
    if (auto it = kv.find("line_bank"); it != kv.end()) cfg.lineBank = it->second == "on";
    readNumber(kv, "line_bank_threshold", cfg.lineBankThreshold);
    readNumber(kv, "line_bank_cps", cfg.lineBankCps);

    // Resolve the cloud API key without ever storing it in version control:
    // prefer the named environment variable, then fall back to the gitignored
    // config/secrets.cfg. Left empty (cloud disabled) when neither is set.
    if (const char* env = std::getenv(cfg.apiKeyEnv.c_str()); env && *env) {
        cfg.apiKey = env;
    } else {
        auto secrets = readKv(configDir / "secrets.cfg");
        if (auto it = secrets.find("api_key"); it != secrets.end()) cfg.apiKey = it->second;
    }
    return cfg;
}

bool parseOnOff(const std::string& value, bool fallback) {
    std::string v = trim(value);
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "on" || v == "true" || v == "yes" || v == "1") return true;
    if (v == "off" || v == "false" || v == "no" || v == "0") return false;
    return fallback;
}

GraphicsConfig loadGraphicsConfig(const std::filesystem::path& configDir) {
    GraphicsConfig cfg;
    auto kv = readKv(configDir / "graphics.cfg");
    if (auto it = kv.find("noir_night"); it != kv.end()) {
        cfg.noirNight = parseOnOff(it->second, cfg.noirNight);
    }
    if (auto it = kv.find("rain"); it != kv.end()) cfg.rain = parseOnOff(it->second, cfg.rain);
    return cfg;
}

}  // namespace llm_npc
