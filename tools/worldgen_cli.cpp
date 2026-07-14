// Headless world-generation driver (plan: llm-world-generation).
//
//   worldgen_cli validate-map <file.json>
//   worldgen_cli validate-cast <file.json>          ({"characters": [...]})
//   worldgen_cli gen-cast "<description>" [model]
//   worldgen_cli gen-map "<description>" [model]
//   worldgen_cli gen-village "<description>" [model]  (map + cast combined)
//
// Generation runs the full retry loop (validators' errors fed back
// verbatim, cap kWorldGenMaxAttempts) against the configured Ollama, then
// prints the result JSON to stdout. A final stats line goes to stderr as
// JSON — tools/bench_schema_models.py keys on it:
//   [worldgen-stats] {"mode": ..., "model": ..., "attempts": N,
//                     "valid": true/false, "seconds": S}
// Exit code: 0 valid, 2 invalid after retries, 1 usage/setup errors.
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "Config.hpp"
#include "LlmClient.hpp"
#include "Trait.hpp"
#include "WorldGen.hpp"

using namespace llm_npc;

namespace {

std::filesystem::path projectRoot() {
    auto root = std::filesystem::current_path();
    for (int i = 0; i < 4; ++i) {
        if (std::filesystem::exists(root / "config" / "llm.cfg")) break;
        if (root.has_parent_path()) root = root.parent_path();
    }
    return root;
}

// Synchronous ask: submit and poll until THE reply lands.
std::string generateOnce(LlmClient& client, const std::string& system,
                         const std::string& user, bool& ok) {
    const std::uint64_t id = client.submit(system, {}, user);
    for (;;) {
        client.drainDeltas();  // discard streaming; we want the whole reply
        for (const ChatReply& reply : client.drainReplies()) {
            if (reply.id != id) continue;
            ok = reply.ok;
            return reply.ok ? reply.content : reply.errorMessage;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

std::string slurp(const char* path, bool& ok) {
    std::ifstream in(path);
    std::ostringstream buf;
    buf << in.rdbuf();
    ok = static_cast<bool>(in);
    return buf.str();
}

void printErrors(const std::vector<MapError>& mapErrors,
                 const std::vector<CastError>& castErrors) {
    for (const auto& e : mapErrors) std::cerr << e.where << ": " << e.reason << "\n";
    for (const auto& e : castErrors) std::cerr << e.where << ": " << e.reason << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: worldgen_cli <validate-map|validate-cast|gen-cast|"
                     "gen-map|gen-village> <file|description> [model]\n";
        return 1;
    }
    const std::string mode = argv[1];
    const std::string arg = argv[2];
    const auto root = projectRoot();

    std::vector<std::string> traitErrors;
    const std::vector<TraitDef> traits = loadAllTraits(root / "traits", &traitErrors);

    // ---- offline validation modes (the bench and CI hooks) ----
    if (mode == "validate-map" || mode == "validate-cast") {
        bool ok = false;
        const std::string text = slurp(arg.c_str(), ok);
        if (!ok) {
            std::cerr << "cannot read " << arg << "\n";
            return 1;
        }
        if (mode == "validate-map") {
            SandboxMap map;
            if (!SandboxMap::fromJson(text, map)) {
                std::cerr << "malformed map JSON\n";
                return 2;
            }
            const auto errors = validateMap(map);
            printErrors(errors, {});
            return errors.empty() ? 0 : 2;
        }
        std::vector<GeneratedCharacter> cast;
        if (!parseGeneratedCast(text, cast)) {
            std::cerr << "malformed cast JSON\n";
            return 2;
        }
        const auto errors = validateCast(cast, traits);
        printErrors({}, errors);
        return errors.empty() ? 0 : 2;
    }

    // ---- generation modes ----
    LlmConfig config = loadLlmConfig(root / "config");
    if (argc >= 4) config.model = argv[3];
    LlmClient client(config);

    const std::string system = mode == "gen-cast" ? buildCastPrompt(traits)
                               : mode == "gen-map" ? buildMapPrompt()
                                                   : buildVillagePrompt(traits);
    const auto start = std::chrono::steady_clock::now();
    std::string user = arg;
    bool valid = false;
    int attempt = 0;
    std::string finalJson;

    for (; attempt < kWorldGenMaxAttempts && !valid; ++attempt) {
        bool replyOk = false;
        const std::string raw = generateOnce(client, system, user, replyOk);
        if (!replyOk) {
            std::cerr << "model error: " << raw << "\n";
            break;
        }
        std::string json;
        if (!extractJsonObject(raw, json)) {
            user = arg + "\n\nYour previous output contained no JSON object. "
                         "Reply with ONLY the JSON.";
            continue;
        }
        std::vector<MapError> mapErrors;
        std::vector<CastError> castErrors;
        if (mode == "gen-cast") {
            std::vector<GeneratedCharacter> cast;
            if (!parseGeneratedCast(json, cast)) {
                mapErrors.push_back({"output", "JSON shape is not "
                                               "{\"characters\": [\"...\"]}"});
            } else {
                castErrors = validateCast(cast, traits);
            }
        } else if (mode == "gen-map") {
            SandboxMap map;
            if (!SandboxMap::fromJson(json, map)) {
                mapErrors.push_back({"output", "JSON is not a version-1 map"});
            } else {
                mapErrors = validateMap(map);
            }
        } else {  // gen-village
            SandboxMap map;
            std::vector<GeneratedCharacter> cast;
            if (!parseGeneratedVillage(json, map, cast)) {
                mapErrors.push_back(
                    {"output", "JSON shape is not {\"map\": {...}, "
                               "\"characters\": [\"...\"]}"});
            } else {
                mapErrors = validateMap(map);
                castErrors = validateCast(cast, traits);
                const auto links = validateVillageLinks(map, cast);
                mapErrors.insert(mapErrors.end(), links.begin(), links.end());
            }
        }
        if (mapErrors.empty() && castErrors.empty()) {
            valid = true;
            finalJson = json;
        } else {
            printErrors(mapErrors, castErrors);
            user = arg + "\n\n" + renderRetryFeedback(mapErrors, castErrors);
        }
    }

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::cerr << "[worldgen-stats] {\"mode\": \"" << mode << "\", \"model\": \""
              << config.model << "\", \"attempts\": " << attempt
              << ", \"valid\": " << (valid ? "true" : "false")
              << ", \"seconds\": " << seconds << "}\n";
    if (!valid) return 2;
    std::cout << finalJson << "\n";
    return 0;
}
