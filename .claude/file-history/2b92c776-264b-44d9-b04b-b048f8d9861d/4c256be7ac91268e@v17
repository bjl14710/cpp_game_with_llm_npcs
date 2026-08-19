// Game entry point — raylib migration step 2 of 8 (issue #36).
//
// The mode machine (Playing / Dialogue / Menu), first-person camera, WASD
// collision movement, and key bindings are back on raylib. World geometry
// and NPCs render as placeholder primitives until the asset-pack scene
// (issue #37) and characters (#38) land; Dialogue/Menu show placeholder
// overlays until the UI port (#40). The full SFML game loop being restored
// piecewise lives at: git show feature/raylib-scaffold:src/app/main.cpp
#include "raylib.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "City.hpp"
#include "Config.hpp"
#include "InputMap.hpp"
#include "KeyBindings.hpp"
#include "LlmClient.hpp"
#include "Math.hpp"
#include "Npc.hpp"
#include "PersonaLoader.hpp"
#include "World.hpp"

namespace fs = std::filesystem;
using namespace llm_npc;

namespace {

constexpr float kWalkSpeed = 7.0f;      // units (~meters) per second
constexpr float kPlayerRadius = 0.45f;  // collision circle on the ground
constexpr float kTalkRadius = 3.5f;     // how close "press T to talk" works
constexpr float kEyeHeight = 1.7f;      // camera height above the feet
constexpr float kMouseSensitivity = 0.12f;
constexpr float kMaxPitchDeg = 75.f;

// What the main loop is currently showing.
enum class AppMode { Playing, Dialogue, Menu };

// First-person pose; position is the FEET on the ground plane (y = 0) so
// collision and NPC proximity share the legacy convention.
struct PlayerPose {
    Vec3 position{0.f, 0.f, 24.f};  // plaza south edge
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

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

// Ground-plane right vector; must match the camera basis or strafe inverts.
Vec3 flatRight(float yawDeg) {
    const Vec3 f = flatForward(yawDeg);
    return Vec3{-f.z, 0.f, f.x};
}

// raylib Camera3D for the current pose: eye at head height, target one unit
// along the view direction (yaw around Y, pitch tilting it).
Camera3D cameraFor(const PlayerPose& pose) {
    Camera3D cam{};
    const float pitchRad = degToRad(pose.pitchDeg);
    const Vec3 flat = flatForward(pose.yawDeg);
    const Vec3 dir{flat.x * std::cos(pitchRad), std::sin(pitchRad), flat.z * std::cos(pitchRad)};
    cam.position = {pose.position.x, pose.position.y + kEyeHeight, pose.position.z};
    cam.target = {cam.position.x + dir.x, cam.position.y + dir.y, cam.position.z + dir.z};
    cam.up = {0.f, 1.f, 0.f};
    cam.fovy = 70.f;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

// One frame of mouse look from raylib's relative mouse delta (cursor is
// disabled while playing, so the delta is raw movement — no re-centering,
// no macOS cursor-disassociation workaround needed anymore).
void applyMouseLook(PlayerPose& pose) {
    const Vector2 delta = GetMouseDelta();
    // Mouse-right lowers yaw: the camera basis is right-handed with yaw
    // increasing toward +X (screen-left) — same derivation as the legacy
    // renderer's.
    pose.yawDeg -= delta.x * kMouseSensitivity;
    pose.pitchDeg = clampf(pose.pitchDeg - delta.y * kMouseSensitivity,
                           -kMaxPitchDeg, kMaxPitchDeg);
}

// Placeholder city drawing until the asset-pack scene (issue #37): flat
// ground, building boxes at their collision AABBs, NPC marker cylinders.
void drawPlaceholderWorld(const World& world) {
    DrawPlane({0.f, 0.f, 0.f},
              {world.city().halfSize() * 2.f, world.city().halfSize() * 2.f},
              Color{86, 125, 70, 255});
    for (const Building& b : world.city().buildings()) {
        const float w = b.maxX - b.minX;
        const float d = b.maxZ - b.minZ;
        const Vector3 center{(b.minX + b.maxX) * 0.5f, b.height * 0.5f,
                             (b.minZ + b.maxZ) * 0.5f};
        const unsigned char tint = static_cast<unsigned char>(120 + (b.facadeKind * 37) % 90);
        DrawCube(center, w, b.height, d, Color{tint, tint, static_cast<unsigned char>(tint + 20), 255});
        DrawCubeWires(center, w, b.height, d, Color{40, 45, 60, 255});
    }
    for (const Npc& npc : world.npcs()) {
        const Vec3& p = npc.position();
        DrawCylinder({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{200, 120, 80, 255});
        DrawCylinderWires({p.x, 0.f, p.z}, 0.35f, 0.35f, 1.8f, 12, Color{60, 30, 20, 255});
    }
}

// Dim the frame and center a message — the stand-in for Menu/DialogUI until
// their raylib port (issue #40).
void drawPlaceholderOverlay(const std::string& title, const std::string& hint) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{8, 10, 16, 170});
    const int titleWidth = MeasureText(title.c_str(), 32);
    DrawText(title.c_str(), (GetScreenWidth() - titleWidth) / 2,
             GetScreenHeight() / 2 - 40, 32, RAYWHITE);
    const int hintWidth = MeasureText(hint.c_str(), 18);
    DrawText(hint.c_str(), (GetScreenWidth() - hintWidth) / 2,
             GetScreenHeight() / 2 + 10, 18, Color{170, 180, 200, 255});
}

}  // namespace

int main(int argc, char** argv) {
    // --frames N: render N frames then exit 0 (scripted smoke runs).
    long maxFrames = -1;
    if (argc >= 3 && std::strcmp(argv[1], "--frames") == 0) {
        maxFrames = std::strtol(argv[2], nullptr, 10);
    }

    const fs::path projectRoot = findProjectRoot();
    const fs::path configDir = projectRoot / "config";

    KeyBindings bindings = KeyBindings::defaults();
    const fs::path bindingsPath = configDir / "keybindings.cfg";
    bindings.load(bindingsPath);

    // World: the downtown map plus one NPC per persona file. The LlmClient
    // exists because Npc requires one; dialogue traffic returns with the UI
    // port (issue #40), so no warm-up yet.
    LlmClient client(loadLlmConfig(configDir));
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

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "LLM NPC City");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Escape is a game key (menu/back), not app-quit

    const Color sky{135, 190, 235, 255};

    AppMode mode = AppMode::Playing;
    PlayerPose player;
    DisableCursor();  // relative mouse for first-person look

    int nearbyNpc = -1;
    long frames = 0;
    while (!WindowShouldClose() && (maxFrames < 0 || frames++ < maxFrames)) {
        const float dt = std::fmin(0.03f, GetFrameTime());

        // ---- input + simulation ----
        if (mode == AppMode::Playing) {
            if (IsWindowFocused()) applyMouseLook(player);

            Vec3 wish{};
            if (isActionPressed(bindings, Action::MoveForward)) wish += flatForward(player.yawDeg);
            if (isActionPressed(bindings, Action::MoveBackward)) wish += flatForward(player.yawDeg) * -1.f;
            if (isActionPressed(bindings, Action::StrafeRight)) wish += flatRight(player.yawDeg);
            if (isActionPressed(bindings, Action::StrafeLeft)) wish += flatRight(player.yawDeg) * -1.f;
            wish = normalize(wish);
            const Vec3 target = player.position + wish * (kWalkSpeed * dt);
            player.position = world.city().resolveMovement(player.position, target, kPlayerRadius);

            const bool menuKey = isActionJustPressed(bindings, Action::OpenMenu) ||
                                 IsKeyPressed(KEY_ESCAPE);
            if (menuKey) {
                mode = AppMode::Menu;
                EnableCursor();
            } else if (isActionJustPressed(bindings, Action::Talk) && nearbyNpc >= 0) {
                world.npcs()[static_cast<std::size_t>(nearbyNpc)].lookAt(player.position);
                mode = AppMode::Dialogue;
                EnableCursor();
            }
        } else {
            // Placeholder pages: Escape returns to the game.
            if (IsKeyPressed(KEY_ESCAPE)) {
                mode = AppMode::Playing;
                DisableCursor();
            }
        }

        // NPC behaviors keep running during dialogue (a companion should
        // keep following), but the pause menu freezes the world.
        if (mode != AppMode::Menu) {
            for (Npc& npc : world.npcs()) npc.update(dt, player.position, world.city());
        }
        nearbyNpc = world.nearestNpcWithin(player.position, kTalkRadius);

        // ---- render ----
        BeginDrawing();
        ClearBackground(sky);
        BeginMode3D(cameraFor(player));
        drawPlaceholderWorld(world);
        EndMode3D();

        if (mode == AppMode::Playing) {
            if (nearbyNpc >= 0) {
                const Npc& npc = world.npcs()[static_cast<std::size_t>(nearbyNpc)];
                const std::string prompt =
                    "[" + bindings.key(Action::Talk) + "] Talk to " + npc.persona().name;
                const int width = MeasureText(prompt.c_str(), 20);
                DrawText(prompt.c_str(), (GetScreenWidth() - width) / 2,
                         GetScreenHeight() - 84, 20, RAYWHITE);
            }
            DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 2.f,
                       Color{255, 255, 255, 200});
        } else if (mode == AppMode::Dialogue) {
            const std::string name =
                nearbyNpc >= 0 ? world.npcs()[static_cast<std::size_t>(nearbyNpc)].persona().name
                               : "someone";
            drawPlaceholderOverlay("Talking to " + name,
                                   "Dialogue UI arrives with issue #40 — Esc to leave.");
        } else {
            drawPlaceholderOverlay("Paused",
                                   "Menu arrives with issue #40 — Esc to resume.");
        }
        DrawFPS(24, 24);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
