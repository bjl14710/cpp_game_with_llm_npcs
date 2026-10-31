#include <SFML/Graphics.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "DialogUI.hpp"
#include "LlmClient.hpp"
#include "Npc.hpp"
#include "Persona.hpp"

namespace fs = std::filesystem;

namespace {

// Trim ASCII whitespace from both ends.
std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Tiny key=value config reader; same shape as cpp_racing_game/config/*.cfg.
// Lines starting with '#' are comments.
std::unordered_map<std::string, std::string> readKv(const fs::path& path) {
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

std::string slurp(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream o;
    o << in.rdbuf();
    return o.str();
}

llm_npc::LlmConfig loadLlmConfig(const fs::path& configDir) {
    llm_npc::LlmConfig cfg;
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

// Try a few likely font paths so the game runs on stock Linux and Windows
// without bundling a font. Returns the first existing path or empty.
fs::path findSystemFont() {
    const char* candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* c : candidates) {
        if (fs::exists(c)) return fs::path(c);
    }
    return {};
}

// Build the companion's persona. The free-form lines come from
// personas/companion.txt so the writing voice can be iterated without a rebuild.
llm_npc::Persona buildCompanion(const fs::path& personasDir) {
    llm_npc::Persona p;
    p.name = "Wren";
    p.role = "curious traveling companion";
    p.traits = {"warm", "observant", "a little stubborn"};
    p.speakingStyle = "1-3 short sentences. Plainspoken. Asks back occasionally.";
    p.knowledgeBoundary =
        "You know the road, the weather, and rumours from villages you have passed through. "
        "You have never heard of computers, code, taxes, sports teams, or modern celebrities; "
        "if asked, say so in character.";
    std::string extra = slurp(personasDir / "companion.txt");
    if (!extra.empty()) p.extraDirectives = extra;
    return p;
}

}  // namespace

int main() {
    // Resolve project-relative paths so the binary can be launched from build/.
    fs::path exeDir = fs::current_path();
    fs::path projectRoot = exeDir;
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(projectRoot / "config" / "llm.cfg")) break;
        if (projectRoot.has_parent_path()) projectRoot = projectRoot.parent_path();
    }
    const fs::path configDir = projectRoot / "config";
    const fs::path personasDir = projectRoot / "personas";

    llm_npc::LlmConfig llmConfig = loadLlmConfig(configDir);
    std::cerr << "[llm_npc] using model=" << llmConfig.model
              << " host=" << llmConfig.host << ":" << llmConfig.port << "\n";

    sf::RenderWindow window(sf::VideoMode(1280, 720), "Companion — LLM NPC demo");
    window.setFramerateLimit(60);

    sf::Font font;
    fs::path fontPath = findSystemFont();
    if (fontPath.empty() || !font.loadFromFile(fontPath.string())) {
        std::cerr << "[llm_npc] no system font found; UI will not render text.\n";
        return 1;
    }

    llm_npc::LlmClient client(llmConfig);
    llm_npc::Npc companion(buildCompanion(personasDir), client);
    llm_npc::DialogUI ui(font);

    ui.appendLine({llm_npc::TranscriptLine::Kind::System, "",
                   "Press Enter to send. Esc to quit. Talk to " + companion.persona().name + "."});

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                window.close();
            }

            std::string submitted = ui.handleEvent(event);
            if (!submitted.empty() && !companion.waiting()) {
                ui.appendLine({llm_npc::TranscriptLine::Kind::Player, "You", submitted});
                companion.ask(submitted);
                ui.setThinking(true, companion.persona().name);
                ui.setInputEnabled(false);
            }
        }

        for (const auto& reply : client.drainReplies()) {
            auto text = companion.onReplyArrived(reply);
            if (text) {
                ui.appendLine({llm_npc::TranscriptLine::Kind::Npc, companion.persona().name, *text});
            } else if (!reply.ok) {
                ui.appendLine({llm_npc::TranscriptLine::Kind::System, "",
                               "[" + companion.persona().name + " seems distracted: " +
                                   reply.errorMessage + "]"});
            }
            ui.setThinking(false);
            ui.setInputEnabled(true);
        }

        window.clear(sf::Color(18, 18, 22));
        ui.render(window);
        window.display();
    }
    return 0;
}
