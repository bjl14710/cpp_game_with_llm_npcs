#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "City.hpp"
#include "Config.hpp"
#include "DialogUI.hpp"
#include "DialogueSession.hpp"
#include "InputMap.hpp"
#include "KeyBindings.hpp"
#include "LlmClient.hpp"
#include "Math.hpp"
#include "Menu.hpp"
#include "Npc.hpp"
#include "PersonaLoader.hpp"
#include "Renderer3D.hpp"
#include "World.hpp"

namespace fs = std::filesystem;
using namespace llm_npc;

namespace {

constexpr float kWalkSpeed = 7.0f;        // units (~meters) per second
constexpr float kPlayerRadius = 0.45f;    // collision circle on the ground
constexpr float kTalkRadius = 3.5f;       // how close "press T to talk" works
constexpr float kNameplateRange = 28.f;   // how far name tags stay visible
constexpr float kMouseSensitivity = 0.12f;
constexpr float kMaxPitchDeg = 75.f;

// What the main loop is currently showing.
enum class AppMode { Playing, Dialogue, Menu };

// Try a few likely font paths so the game runs on stock Linux and Windows
// without bundling a font. Returns the first existing path or empty.
fs::path findSystemFont() {
    const char* candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/Library/Fonts/Arial.ttf",
    };
    for (const char* c : candidates) {
        if (fs::exists(c)) return fs::path(c);
    }
    return {};
}

// Walks up from the working directory until config/llm.cfg is found so the
// binary can be launched from build/ or the project root.
fs::path findProjectRoot() {
    fs::path root = fs::current_path();
    for (int i = 0; i < 4; ++i) {
        if (fs::exists(root / "config" / "llm.cfg")) break;
        if (root.has_parent_path()) root = root.parent_path();
    }
    return root;
}

// Unit forward vector on the ground plane for a yaw in degrees.
Vec3 flatForward(float yawDeg) {
    return normalize(Vec3{std::sin(degToRad(yawDeg)), 0.f, std::cos(degToRad(yawDeg))});
}

// Ground-plane right vector for a yaw in degrees.
Vec3 flatRight(float yawDeg) {
    const Vec3 f = flatForward(yawDeg);
    return Vec3{f.z, 0.f, -f.x};
}

// Re-centers the OS cursor so relative mouse-look deltas keep working.
void centerMouse(sf::RenderWindow& window) {
    sf::Mouse::setPosition(sf::Vector2i(static_cast<int>(window.getSize().x / 2),
                                        static_cast<int>(window.getSize().y / 2)),
                           window);
}

// Applies one frame of mouse look to the camera and re-centers the cursor.
void handleMouseLook(sf::RenderWindow& window, CameraPose& camera) {
    const sf::Vector2i center(static_cast<int>(window.getSize().x / 2),
                              static_cast<int>(window.getSize().y / 2));
    const sf::Vector2i delta = sf::Mouse::getPosition(window) - center;
    camera.yawDeg += static_cast<float>(delta.x) * kMouseSensitivity;
    camera.pitchDeg = clampf(camera.pitchDeg - static_cast<float>(delta.y) * kMouseSensitivity,
                             -kMaxPitchDeg, kMaxPitchDeg);
    if (window.hasFocus()) centerMouse(window);
}

// Draws `str` centered horizontally at height `y`, with a dark backdrop bar.
void drawCenteredHudText(sf::RenderWindow& window, const sf::Font& font,
                         const std::string& str, unsigned size, float y) {
    sf::Text text(str, font, size);
    const sf::FloatRect bounds = text.getLocalBounds();
    const float x = (static_cast<float>(window.getSize().x) - bounds.width) * 0.5f;

    sf::RectangleShape backdrop(sf::Vector2f(bounds.width + 24.f, bounds.height + 16.f));
    backdrop.setPosition(x - 12.f, y - 6.f);
    backdrop.setFillColor(sf::Color(10, 14, 22, 170));
    window.draw(backdrop);

    text.setPosition(x, y);
    text.setFillColor(sf::Color::White);
    window.draw(text);
}

}  // namespace

int main() {
    const fs::path projectRoot = findProjectRoot();
    const fs::path configDir = projectRoot / "config";

    const LlmConfig llmConfig = loadLlmConfig(configDir);
    std::cerr << "[llm_npc] model=" << llmConfig.model << " host=" << llmConfig.host << ":"
              << llmConfig.port << "\n";

    KeyBindings bindings = KeyBindings::defaults();
    const fs::path bindingsPath = configDir / "keybindings.cfg";
    bindings.load(bindingsPath);

    // World: the downtown map plus one NPC per persona file.
    LlmClient client(llmConfig);
    client.warmUp();  // preload the model so the first reply starts fast

    World world(City::makeDowntown());
    std::vector<std::string> personaErrors;
    const auto roster = loadAllPersonas(projectRoot / "personas", &personaErrors);
    for (const auto& err : personaErrors) std::cerr << "[llm_npc] persona error: " << err << "\n";
    for (const auto& loaded : roster) {
        Npc npc(loaded.persona, client);
        npc.setPlacement(loaded.position, loaded.facingDeg, loaded.spotId);
        world.addNpc(std::move(npc));
    }
    std::cerr << "[llm_npc] loaded " << world.npcs().size() << " NPCs\n";

    // Window with a legacy-GL 2.1 context (same recipe as cpp_shooter_game).
    sf::ContextSettings settings;
    settings.depthBits = 24;
    settings.stencilBits = 8;
    settings.antialiasingLevel = 4;
    settings.majorVersion = 2;
    settings.minorVersion = 1;
    sf::RenderWindow window(sf::VideoMode(1280, 720), "LLM NPC City", sf::Style::Default, settings);
    window.setVerticalSyncEnabled(true);
    window.setActive(true);

    sf::Font font;
    const fs::path fontPath = findSystemFont();
    if (fontPath.empty() || !font.loadFromFile(fontPath.string())) {
        std::cerr << "[llm_npc] no system font found; UI cannot render text.\n";
        return 1;
    }

    Renderer3D renderer;
    renderer.init();

    DialogUI dialog(font);
    DialogueSession session;
    Menu menu(bindings, bindingsPath);
    // Late replies for conversations the player already walked away from
    // still need to reach the right NPC's history.
    std::unordered_map<std::uint64_t, int> pendingRoutes;

    AppMode mode = AppMode::Playing;
    CameraPose camera;
    camera.position = Vec3{0.f, 0.f, 24.f};  // plaza south edge, facing the cart
    window.setMouseCursorVisible(false);
    centerMouse(window);

    int nearbyNpc = -1;
    sf::Clock frameClock;
    while (window.isOpen()) {
        sf::Event event{};
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::GainedFocus && mode == AppMode::Playing) centerMouse(window);

            if (mode == AppMode::Playing && event.type == sf::Event::KeyPressed) {
                const bool menuKey = event.key.code == keyFromName(bindings.key(Action::OpenMenu)) ||
                                     event.key.code == sf::Keyboard::Escape;
                if (menuKey) {
                    menu.open();
                    mode = AppMode::Menu;
                    window.setMouseCursorVisible(true);
                } else if (event.key.code == keyFromName(bindings.key(Action::Talk)) && nearbyNpc >= 0) {
                    session.open(nearbyNpc);
                    const Npc& npc = world.npcs()[static_cast<std::size_t>(nearbyNpc)];
                    dialog.reset();
                    dialog.setInputEnabled(true);
                    dialog.swallowNextTextEntered();
                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                       "You are talking to " + npc.persona().name + " (" +
                                           npc.persona().role + "). Enter sends, Esc leaves."});
                    mode = AppMode::Dialogue;
                    window.setMouseCursorVisible(true);
                }
                continue;
            }

            if (mode == AppMode::Dialogue) {
                if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                    session.close();
                    dialog.endStreaming();
                    mode = AppMode::Playing;
                    window.setMouseCursorVisible(false);
                    centerMouse(window);
                    continue;
                }
                const std::string submitted = dialog.handleEvent(event);
                if (!submitted.empty() && session.isOpen()) {
                    Npc& npc = world.npcs()[static_cast<std::size_t>(session.npcIndex())];
                    if (!npc.waiting()) {
                        dialog.appendLine({TranscriptLine::Kind::Player, "You", submitted});
                        const std::uint64_t id = npc.ask(submitted);
                        session.submitted(id);
                        pendingRoutes[id] = session.npcIndex();
                        dialog.beginStreaming(npc.persona().name);
                        dialog.setInputEnabled(false);
                    }
                }
                continue;
            }

            if (mode == AppMode::Menu) {
                switch (menu.handleEvent(event, window)) {
                    case MenuResult::Resume:
                        mode = AppMode::Playing;
                        window.setMouseCursorVisible(false);
                        centerMouse(window);
                        break;
                    case MenuResult::Quit:
                        window.close();
                        break;
                    case MenuResult::None:
                        break;
                }
            }
        }
        if (!window.isOpen()) break;

        const float dt = std::min(0.03f, frameClock.restart().asSeconds());

        if (mode == AppMode::Playing) {
            if (window.hasFocus()) handleMouseLook(window, camera);

            Vec3 wish{};
            if (isActionPressed(bindings, Action::MoveForward)) wish += flatForward(camera.yawDeg);
            if (isActionPressed(bindings, Action::MoveBackward)) wish += flatForward(camera.yawDeg) * -1.f;
            if (isActionPressed(bindings, Action::StrafeRight)) wish += flatRight(camera.yawDeg);
            if (isActionPressed(bindings, Action::StrafeLeft)) wish += flatRight(camera.yawDeg) * -1.f;
            wish = normalize(wish);
            const Vec3 target = camera.position + wish * (kWalkSpeed * dt);
            camera.position = world.city().resolveMovement(camera.position, target, kPlayerRadius);
        } else if (mode == AppMode::Menu) {
            menu.update(dt);
        }

        nearbyNpc = world.nearestNpcWithin(camera.position, kTalkRadius);

        // Streamed fragments for the open conversation appear immediately.
        for (const auto& delta : client.drainDeltas()) {
            if (session.deltaArrived(delta.id, delta.text)) {
                dialog.appendStreamingDelta(delta.text);
            }
        }

        // Completed replies land in NPC history even after the dialog closed.
        for (const auto& reply : client.drainReplies()) {
            const auto route = pendingRoutes.find(reply.id);
            if (route == pendingRoutes.end()) continue;
            Npc& npc = world.npcs()[static_cast<std::size_t>(route->second)];
            const auto text = npc.onReplyArrived(reply);
            pendingRoutes.erase(route);

            if (session.replyArrived(reply.id, reply.ok)) {
                dialog.endStreaming();
                if (text) {
                    dialog.appendLine({TranscriptLine::Kind::Npc, npc.persona().name, *text});
                } else {
                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                       "[" + npc.persona().name + " seems distracted: " +
                                           reply.errorMessage + "]"});
                }
                dialog.setInputEnabled(true);
            }
        }

        // ---- 3D pass ----
        renderer.beginFrame(window, camera);
        renderer.drawCity(world.city());
        for (std::size_t i = 0; i < world.npcs().size(); ++i) {
            const Npc& npc = world.npcs()[i];
            renderer.drawNpc(NpcVisual{npc.position(), npc.facingDeg(), static_cast<int>(i)});
        }

        // ---- SFML overlay pass ----
        window.pushGLStates();

        for (const Npc& npc : world.npcs()) {
            if (distanceXZ(camera.position, npc.position()) > kNameplateRange) continue;
            sf::Vector2f screen;
            if (!renderer.worldToScreen(npc.position() + Vec3{0.f, 2.15f, 0.f}, window, screen)) continue;
            sf::Text tag(npc.persona().name, font, 14);
            const sf::FloatRect bounds = tag.getLocalBounds();
            tag.setPosition(screen.x - bounds.width * 0.5f, screen.y - bounds.height);
            tag.setOutlineThickness(2.f);
            tag.setOutlineColor(sf::Color(0, 0, 0, 200));
            tag.setFillColor(sf::Color::White);
            window.draw(tag);
        }

        if (mode == AppMode::Playing) {
            if (nearbyNpc >= 0) {
                const Npc& npc = world.npcs()[static_cast<std::size_t>(nearbyNpc)];
                drawCenteredHudText(window, font,
                                    "[" + bindings.key(Action::Talk) + "] Talk to " + npc.persona().name,
                                    20, static_cast<float>(window.getSize().y) - 84.f);
            }
            // Crosshair dot.
            sf::CircleShape dot(2.f);
            dot.setOrigin(2.f, 2.f);
            dot.setPosition(static_cast<float>(window.getSize().x) * 0.5f,
                            static_cast<float>(window.getSize().y) * 0.5f);
            dot.setFillColor(sf::Color(255, 255, 255, 200));
            window.draw(dot);
        } else if (mode == AppMode::Dialogue) {
            sf::RectangleShape dim(sf::Vector2f(static_cast<float>(window.getSize().x),
                                                static_cast<float>(window.getSize().y)));
            dim.setFillColor(sf::Color(8, 10, 16, 150));
            window.draw(dim);
            dialog.render(window);
        } else {
            menu.render(window, font);
        }

        window.popGLStates();
        window.display();
    }
    return 0;
}
