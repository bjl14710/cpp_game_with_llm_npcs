// Game entry point on raylib — full parity with the SFML loop it replaced
// (that version remains at: git show feature/raylib-scaffold:src/app/main.cpp).
// Modes: Playing / Dialogue / Menu. Solo simulates locally; hosting shares
// this world through NetServer + HostChatRouter; joining renders the host's
// snapshots and routes chat through NetClient.
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets.hpp"
#include "CharacterParts.hpp"
#include "CharacterStore.hpp"
#include "City.hpp"
#include "CombatEvents.hpp"
#include "Config.hpp"
#include "ConversationStore.hpp"
#include "Cutscene.hpp"
#include "DialogUI.hpp"
#include "FactStore.hpp"
#include "Gossip.hpp"
#include "GroupSession.hpp"
#include "Journal.hpp"
#include "Montage.hpp"
#include "Mystery.hpp"
#include "Storyline.hpp"
#include "DialogueSession.hpp"
#include "HostChatRouter.hpp"
#include "InputMap.hpp"
#include "KeyBindings.hpp"
#include "LlmClient.hpp"
#include "LocationLog.hpp"
#include "Math.hpp"
#include "Menu.hpp"
#include "NetClient.hpp"
#include "NetServer.hpp"
#include "Npc.hpp"
#include "PersonaLoader.hpp"
#include "RatingLog.hpp"
#include "WorldGen.hpp"
#include "RaylibRenderer.hpp"
#include "SandboxMap.hpp"
#include "Trait.hpp"
#include "Weapon.hpp"
#include "Zones.hpp"
#include "World.hpp"

namespace fs = std::filesystem;
using namespace llm_npc;

namespace {

constexpr float kWalkSpeed = 7.0f;       // units (~meters) per second
constexpr float kPlayerRadius = 0.45f;   // collision circle on the ground
// Jump arc (user request: hop the curb-line props instead of snagging on
// them). Apex = v^2/2g ~ 1.45u: clears benches (0.6), bushes (1.1) and
// lands on car roofs (~1.35); building walls stay walls.
constexpr float kGravity = 22.f;         // units per second^2
constexpr float kJumpSpeed = 8.0f;       // takeoff speed, units per second
constexpr float kTalkRadius = 3.5f;      // how close "press T to talk" works
constexpr float kNameplateRange = 28.f;  // how far name tags stay visible
constexpr float kMouseSensitivity = 0.12f;
constexpr float kMaxPitchDeg = 75.f;
// Creator preview rotation (issue #93). Player-driven only; after this idle
// spell with no input the figure eases back to facing the camera. All tunable.
constexpr float kPreviewIdleTimeout   = 2.5f;   // seconds of no input before return
constexpr float kPreviewEaseRate      = 6.0f;   // exponential ease toward front
constexpr float kPreviewDragDegPerPx  = 0.4f;   // mouse-drag sensitivity
constexpr float kPreviewKeyDegPerSec  = 90.f;   // Left/Right key rotation speed
// How long an arrest holds the player at the police station. Short, because
// the worst on-the-books offense here is disturbing the peace.
constexpr float kJailSeconds = 10.f;

// What the main loop is currently showing.
// Every mode here needs its own branch in the 2D overlay chain near the bottom
// of the frame. SandboxEdit had none and fell through to menu.render(), which
// drew the whole paused menu over the map editor (#215). A branch that draws
// nothing is still a branch.
enum class AppMode { Playing, Dialogue, Menu, Dead, SandboxEdit, Cutscene };

// First-person pose; position is the FEET on the ground plane (y = 0).
struct LocalPlayer {
    Vec3 position{0.f, 0.f, 24.f};  // plaza south edge
    float yawDeg = 0.f;
    float pitchDeg = 0.f;
};

// A short in-world text line above an NPC's head, spawned by combat events.
struct CombatCallout {
    int npcIndex = -1;
    std::string text;
    float ttl = 3.f;  // seconds before it fades
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

// One frame of mouse look from raylib's relative mouse delta.
void applyMouseLook(LocalPlayer& pose) {
    const Vector2 delta = GetMouseDelta();
    // Mouse-right lowers yaw: right-handed basis, yaw grows toward +X.
    pose.yawDeg -= delta.x * kMouseSensitivity;
    pose.pitchDeg = clampf(pose.pitchDeg - delta.y * kMouseSensitivity,
                           -kMaxPitchDeg, kMaxPitchDeg);
}

// A short third-person description of an action an NPC just took, shown in
// the transcript so a wordless tag-only reply still gives feedback.
std::string stageDirection(NpcAction action) {
    switch (action) {
        case NpcAction::Follow: return "falls into step with you.";
        case NpcAction::Stop: return "stops and stays put.";
        case NpcAction::Face: return "turns to face you.";
        case NpcAction::RaiseHand: return "raises their right hand.";
        case NpcAction::Wave: return "waves at you.";
        case NpcAction::Arrest: return "moves to apprehend you!";
        case NpcAction::CallPolice: return "calls out for the police!";
        case NpcAction::ReturnHome:
        case NpcAction::None: return "";
    }
    return "";
}

// Maps the NPC's core mood onto the renderer's face enum.
NpcFace faceForMood(NpcMood mood) {
    switch (mood) {
        case NpcMood::Happy: return NpcFace::Happy;
        case NpcMood::Angry: return NpcFace::Angry;
        case NpcMood::Sad: return NpcFace::Sad;
        case NpcMood::Embarrassed: return NpcFace::Embarrassed;
        case NpcMood::Surprised: return NpcFace::Surprised;
        case NpcMood::Neutral: return NpcFace::Neutral;
    }
    return NpcFace::Neutral;
}

// Draws `str` centered horizontally at height `y` with a dark backdrop bar.
void drawCenteredHudText(const std::string& str, int size, float y) {
    const int width = MeasureText(str.c_str(), size);
    const float x = (static_cast<float>(GetScreenWidth()) - static_cast<float>(width)) * 0.5f;
    DrawRectangleRec(Rectangle{x - 12.f, y - 6.f, static_cast<float>(width) + 24.f,
                               static_cast<float>(size) + 16.f},
                     Color{10, 14, 22, 170});
    DrawText(str.c_str(), static_cast<int>(x), static_cast<int>(y), size, WHITE);
}

// Nameplate: centered text with an outline-ish shadow at a projected point.
void drawNameplate(const std::string& name, Vector2 screen, Color color) {
    const int width = MeasureText(name.c_str(), 14);
    const int x = static_cast<int>(screen.x) - width / 2;
    const int y = static_cast<int>(screen.y) - 14;
    DrawText(name.c_str(), x + 1, y + 1, 14, Color{0, 0, 0, 200});
    DrawText(name.c_str(), x, y, 14, color);
}

}  // namespace

int main(int argc, char** argv) {
    // --frames N [shot.png] [--camera x z yaw] [--hour H] [--map file]:
    // render N frames
    // then exit 0, optionally saving a screenshot of the last frame
    // (scripted smoke runs + visual checks). --camera overrides the default
    // plaza vantage; --hour pins the world clock so day/night-dependent
    // screenshots stay deterministic.
    long maxFrames = -1;
    const char* screenshotPath = nullptr;
    bool cameraOverride = false;
    float cameraX = 0.f, cameraZ = 0.f, cameraYaw = 180.f;
    float hourOverride = -1.f;
    const char* mapFile = nullptr;  // --map: boot into a sandbox fixture
    // --sandbox-edit: boot --map straight into the EDITOR rather than
    // play mode. Without it the editor is only reachable by pressing P,
    // which a headless --frames capture cannot do — so the editor had no
    // visual-QA path at all, which is how the menu-overlay bug survived.
    bool bootSandboxEdit = false;
    // --cutscene <id>: boot straight into a named cutscene, with playback on a
    // fixed timestep so frame N always lands at the same moment in the scene.
    // Wall-clock playback would produce a different image on every machine and
    // the captures would be worthless as a regression signal.
    const char* bootCutscene = nullptr;
    // --mystery [seed]: generate a murder, cast an authored storyline onto the
    // roster and seed the resulting knowledge. Every piece of this existed and
    // nothing called any of it — src/app/ referenced none of the mystery layer,
    // so the mode was unreachable from the game (issue #220).
    bool bootMystery = false;
    unsigned mysterySeed = 20260809u;
    // --menu <page>: boot straight into a menu page. Every page but Main is
    // reached by clicking, which a headless --frames capture cannot do, so
    // none of them has ever been photographed (#216).
    const char* bootMenuPage = nullptr;
    int arg = 1;
    if (argc >= 3 && std::strcmp(argv[1], "--frames") == 0) {
        maxFrames = std::strtol(argv[2], nullptr, 10);
        arg = 3;
        if (arg < argc && argv[arg][0] != '-') {
            screenshotPath = argv[arg++];
        }
    }
    // The scan runs whether or not --frames led, so every flag below works on
    // its own. It used to sit INSIDE the --frames branch, which meant
    // `--mystery 7`, `--cutscene opening` and `--sandbox-edit` silently did
    // nothing unless a smoke-run prefix happened to be present — the flags
    // were reachable only from the harness that had never needed them.
    {
        while (arg < argc) {
            if (arg + 3 < argc && std::strcmp(argv[arg], "--camera") == 0) {
                cameraOverride = true;
                cameraX = std::strtof(argv[arg + 1], nullptr);
                cameraZ = std::strtof(argv[arg + 2], nullptr);
                cameraYaw = std::strtof(argv[arg + 3], nullptr);
                arg += 4;
            } else if (arg + 1 < argc && std::strcmp(argv[arg], "--hour") == 0) {
                hourOverride = std::strtof(argv[arg + 1], nullptr);
                arg += 2;
            } else if (std::strcmp(argv[arg], "--sandbox-edit") == 0) {
                bootSandboxEdit = true;
                arg += 1;
            } else if (arg + 1 < argc && std::strcmp(argv[arg], "--menu") == 0) {
                bootMenuPage = argv[arg + 1];
                arg += 2;
            } else if (arg + 1 < argc && std::strcmp(argv[arg], "--cutscene") == 0) {
                bootCutscene = argv[arg + 1];
                arg += 2;
            } else if (std::strcmp(argv[arg], "--mystery") == 0) {
                bootMystery = true;
                arg += 1;
                // Optional seed; a bare --mystery keeps the default so the
                // flag is usable without one. The leading-dash check is what
                // makes `--mystery --cutscene opening` parse correctly rather
                // than swallowing the next flag as a seed.
                if (arg < argc && argv[arg][0] != '-') {
                    mysterySeed = static_cast<unsigned>(
                        std::strtoul(argv[arg], nullptr, 10));
                    arg += 1;
                }
            } else if (arg + 1 < argc && std::strcmp(argv[arg], "--map") == 0) {
                // Boot straight into a sandbox map fixture (headless smoke
                // shots for placed pieces; the in-game entry is the menu).
                mapFile = argv[arg + 1];
                arg += 2;
            } else {
                ++arg;
            }
        }
    }

    const fs::path projectRoot = findProjectRoot();
    const fs::path configDir = projectRoot / "config";

    KeyBindings bindings = KeyBindings::defaults();
    const fs::path bindingsPath = configDir / "keybindings.cfg";
    bindings.load(bindingsPath);

    // World: the downtown map plus one NPC per persona file.
    const LlmConfig llmConfig = loadLlmConfig(configDir);
    LlmClient client(llmConfig);
    // Authored replies for recurring topics, served locally instead of a round
    // trip (banks/README.md). Off by default: with no bank installed every
    // request reaches the backend exactly as it always has.
    if (llmConfig.lineBank) {
        auto bank = std::make_unique<LineBank>(projectRoot / "banks",
                                               llmConfig.lineBankThreshold);
        for (const auto& err : bank->errors()) {
            std::cerr << "[llm_npc] line bank error: " << err << "\n";
        }
        client.setLineBank(std::move(bank));
    }
    client.warmUp();  // preload the model so the first reply starts fast
    World world(City::makeDowntown());
    // Smoke runs pin the clock so day/night screenshots are deterministic.
    if (hourOverride >= 0.f) world.state().setTimeOfDayHours(hourOverride);
    std::vector<std::string> personaErrors;
    const auto roster = loadAllPersonas(projectRoot / "personas", &personaErrors);
    for (const auto& err : personaErrors) std::cerr << "[llm_npc] persona error: " << err << "\n";

    // Structured personality traits (issue #116): one shared library every
    // NPC's prompt resolves against. Malformed files are named and skipped.
    std::vector<std::string> traitErrors;
    const std::vector<TraitDef> traitLibrary =
        loadAllTraits(projectRoot / "traits", &traitErrors);
    for (const auto& err : traitErrors) {
        std::cerr << "[llm_npc] trait error: " << err << "\n";
    }
    std::cerr << "[llm_npc] loaded " << traitLibrary.size() << " traits\n";

    // Scripted camera sequences (issue #227). Degrades to inert: a missing
    // cutscenes/ directory leaves the game entirely playable, and a cutscene
    // that fails to load must never block a phase transition.
    std::vector<std::string> cutsceneErrors;
    const std::vector<CutsceneDef> cutsceneLibrary =
        loadCutscenes(projectRoot / "cutscenes", &cutsceneErrors);
    for (const auto& err : cutsceneErrors) {
        std::cerr << "[llm_npc] cutscene error: " << err << "\n";
    }
    // ONE look per NPC, index-aligned with world.npcs(). Every NPC —
    // designer persona or player-created — draws from the same shared
    // composite parts pool the creator picks from (plan:
    // shared-character-library); the rigged pack models are no longer an
    // NPC source. Looks come from the persona's authored `look =` line,
    // or the deterministic per-name fallback when absent/stale.
    // Per-NPC look registry, index-aligned with world.npcs(). POPULATION
    // happens in spawnTownRoster below (extracted so sandbox play can
    // repopulate the world through the same path — issue #110).
    std::vector<CharacterLook> npcLooks;
    CharacterStore characterStore(projectRoot / "saves" / "characters.sqlite3");
    // Cross-session NPC memory store (summaries injected during spawning).
    ConversationStore memoryStore(projectRoot / "saves" / "conversations.sqlite3");
    // History length at the last save, per NPC — sized by
    // resetNpcSideArrays below.
    std::vector<std::size_t> savedTurns;
    // In-flight summary request id → NPC index it belongs to.
    std::unordered_map<std::uint64_t, int> summaryRoutes;

    // Town gossip: structured facts on the world bus, persisted across
    // sessions (plan: gossip-facts). Facts and knowledge sets load before
    // the per-NPC gossip strings render below.
    FactStore factStore(projectRoot / "saves" / "facts.sqlite3");
    factStore.loadInto(world.state());
    // In-flight fact-extraction request id → NPC index it belongs to.
    std::unordered_map<std::uint64_t, int> factRoutes;

    // Re-renders one NPC's gossip prompt block from the facts THAT NPC has
    // heard on the bus (knowledge is per-agent by design).
    const auto refreshGossip = [&](Npc& npc) {
        std::string gossip;
        for (const KnownFact* fact : world.state().factsKnownBy(npc.persona().name)) {
            gossip += fact->content + " (heard from " + fact->source + "). ";
        }
        npc.setGossip(std::move(gossip));
    };

    // PROPOSE step of propose->validate->commit: after a conversation, ask
    // the NPC's own model for at most two structured facts as strict JSON.
    // The reply is validated by validateProposedFacts before anything is
    // committed to the bus — model output never writes directly.
    const auto requestFacts = [&](int npcIndex) {
        if (npcIndex < 0 || npcIndex >= static_cast<int>(world.npcs().size())) return;
        Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
        if (npc.history().empty()) return;
        std::string transcript;
        for (const auto& turn : npc.history()) {
            transcript += (turn.role == "user" ? "Player: " : npc.persona().name + ": ");
            transcript += turn.content + "\n";
        }
        const std::uint64_t id = client.submit(
            "You extract concrete facts from a game conversation. Reply with "
            "ONLY a JSON array (no prose, no code fences) of at most 2 "
            "objects, each {\"subject\": short_topic, \"content\": one short "
            "sentence, \"direction\": \"npc_learned\" or \"player_learned\"}. "
            "npc_learned = the player told the character something new; "
            "player_learned = the character told the player something. Only "
            "specific, memorable facts — no pleasantries. Reply [] if none.",
            {}, "Conversation:\n" + transcript);
        factRoutes[id] = npcIndex;
    };

    // Asks the NPC's own model to update its first-person memory note.
    const auto requestSummary = [&](int npcIndex) {
        if (npcIndex < 0 || npcIndex >= static_cast<int>(world.npcs().size())) return;
        Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
        if (npc.history().size() <= savedTurns[static_cast<std::size_t>(npcIndex)]) return;
        std::string transcript;
        for (const auto& turn : npc.history()) {
            transcript += (turn.role == "user" ? "Player: " : npc.persona().name + ": ");
            transcript += turn.content + "\n";
        }
        std::string request =
            "You are " + npc.persona().name + ". Here is your conversation with the "
            "player so far:\n" + transcript;
        if (!npc.memory().empty()) {
            request += "You previously remembered: " + npc.memory() + "\n";
        }
        request +=
            "In at most 3 sentences, first person, write the updated note you keep "
            "about this player: who they are, what happened, anything to remember "
            "next time. Reply with the note only.";
        const std::uint64_t id = client.submit(
            "You write concise first-person memory notes for a game character. "
            "No preamble, no directives, just the note.",
            {}, std::move(request));
        summaryRoutes[id] = npcIndex;
    };


    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "LLM NPC City");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Escape is a game key (menu/back), not app-quit

    // Models need the GL context, so Assets loads after InitWindow.
    Assets assets((projectRoot / "assets").string());
    RaylibRenderer renderer(assets);

    // The player's own avatar (issue #106): a look-only row under the
    // reserved id in the SAME store created characters use (look-only rows
    // never join the spawn pass — loadAll requires both records). Absent or
    // stale looks demote to the fixed-name deterministic fallback, exactly
    // like personas.
    CharacterLook avatarLook;
    {
        CharacterLook stored;
        const bool has = characterStore.loadLook("player_avatar", stored);
        std::string why;
        avatarLook = lookForPersona("player", has ? &stored : nullptr, &why);
        if (!why.empty()) {
            std::cerr << "[llm_npc] player avatar look invalid (" << why
                      << ") — using the default\n";
        }
    }
    renderer.setAvatarPalette(avatarLook.paletteId);

    DialogUI dialog;
    DialogueSession session;
    Menu menu(bindings, bindingsPath);
    // Late replies for conversations the player already walked away from
    // still need to reach the right NPC's history.
    std::unordered_map<std::uint64_t, int> pendingRoutes;

    // ---- Multiplayer state (all empty in solo play) ----
    std::unique_ptr<NetServer> netServer;
    std::unique_ptr<HostChatRouter> chatRouter;
    std::unique_ptr<NetClient> netClient;
    std::vector<PlayerPose> remotePlayers;              // avatars to draw (both modes)
    std::vector<NetNpcPose> netNpcs;                    // join mode: NPCs from snapshots
    std::vector<int> netMoods(world.npcs().size(), 0);  // join mode: mood per NPC
    bool netChatWaiting = false;                        // join mode: reply pending

    const auto leaveSession = [&] {
        if (netClient) {
            netClient->disconnect();
            netClient.reset();
        }
        if (netServer) {
            chatRouter.reset();
            netServer->stop();
            netServer.reset();
        }
        remotePlayers.clear();
        netNpcs.clear();
        std::fill(netMoods.begin(), netMoods.end(), 0);
        netChatWaiting = false;
    };

    Menu::MultiplayerHooks netHooks;
    netHooks.active = [&] { return netServer != nullptr || netClient != nullptr; };
    netHooks.status = [&]() -> std::string {
        if (netServer) {
            return "Hosting on port " + std::to_string(netServer->port()) + " - " +
                   std::to_string(netServer->playerCount()) + " player(s) joined";
        }
        if (netClient) {
            return netClient->connected()
                       ? "Joined as player " + std::to_string(netClient->playerId())
                       : "Connection lost: " + netClient->lastError();
        }
        return "Solo - host a game or join a friend's.";
    };
    netHooks.onHost = [&](int port) -> std::string {
        if (netClient) return "Leave the joined session first";
        if (netServer) return "";  // already hosting
        NetServer::Settings settings;
        settings.port = port;
        auto server = std::make_unique<NetServer>(settings);
        if (!server->start()) return server->lastError();
        netServer = std::move(server);
        chatRouter = std::make_unique<HostChatRouter>(world, *netServer);
        return "";
    };
    netHooks.onJoin = [&](const std::string& address) -> std::string {
        if (netServer) return "Stop hosting first";
        const auto colon = address.rfind(':');
        const std::string host = address.substr(0, colon);
        const int port = std::atoi(address.c_str() + colon + 1);  // menu validated
        auto joined = std::make_unique<NetClient>();
        if (!joined->connect(host, port, "guest", "")) return joined->lastError();
        netClient = std::move(joined);
        return "";
    };
    netHooks.onLeave = leaveSession;
    menu.setMultiplayer(netHooks);

    AppMode mode = AppMode::Playing;
    LocalPlayer player;
    // Scripted camera playback. Pure core type: it owns no raylib and draws
    // nothing — this layer reads pose() and hands it to beginFrame, then draws
    // bars, fade and caption as 2D.
    CutscenePlayer cutscene;
    // Where the player was standing when playback started, restored on exit so
    // a cutscene never teleports anyone.
    LocalPlayer preCutscenePlayer;
    AppMode preCutsceneMode = AppMode::Playing;
    // Starts `scene` and takes over the camera. Copies the def, so a generated
    // cutscene may be a temporary at the call site.
    const auto playCutscene = [&](const CutsceneDef& scene) {
        if (cutscene.active()) return;
        preCutscenePlayer = player;
        preCutsceneMode = (mode == AppMode::Cutscene) ? AppMode::Playing : mode;
        cutscene.play(scene, CameraPose{Vec3{player.position.x,
                                             player.position.y + kEyeHeight,
                                             player.position.z},
                                        player.yawDeg, player.pitchDeg});
        if (!cutscene.active()) return;  // refused: no beats, or already playing
        mode = AppMode::Cutscene;
        EnableCursor();
    };
    // Vertical motion state for jumping; position.y is the feet height and
    // everything downstream (camera, gun muzzle, net pose) derives from it.
    float playerVerticalSpeed = 0.f;
    // Smoke runs are deterministic: fixed camera (default plaza-facing, or
    // the --camera override), no look drift.
    const bool smokeRun = maxFrames >= 0;
    if (smokeRun) {
        player.yawDeg = cameraYaw;
        if (cameraOverride) {
            player.position.x = cameraX;
            player.position.z = cameraZ;
        }
    }
    DisableCursor();

    int nearbyNpc = -1;
    // Combat presentation state.
    std::vector<CombatCallout> callouts;
    float hurtFlash = 0.f;  // seconds of red vignette after being shot

    // Arrest bookkeeping: catch fires once per latch (see hasCaughtPlayer).
    float jailSecondsLeft = 0.f;
    std::vector<bool> wasCaught;
    // Previous frame's NPC positions, for walk-animation detection.
    std::vector<Vec3> npcLastPos;

    // ---- World population (issue #110) -------------------------------
    // The town roster and the per-NPC bookkeeping are (re)built through
    // these two, so sandbox play can swap maps via world.loadCity and
    // come back without leaving stale state anywhere.

    // Spawns the shipped personas + every stored created character into
    // the CURRENT world, with looks and persisted memories.
    const auto spawnTownRoster = [&]() {
        npcLooks.clear();
        for (const auto& loaded : roster) {
            Npc npc(loaded.persona, client);
            npc.setPlacement(loaded.position, loaded.facingDeg, loaded.spotId);
            npc.setSchedule(loaded.schedule);
            std::string lookWhy;
            npcLooks.push_back(lookForPersona(loaded.persona.name,
                                              loaded.hasLook ? &loaded.look : nullptr,
                                              &lookWhy));
            if (!lookWhy.empty()) {
                std::cerr << "[llm_npc] " << loaded.id << ": authored look rejected ("
                          << lookWhy << ") — using the deterministic fallback\n";
            }
            world.addNpc(std::move(npc));
        }
        // Player-created characters (plan: character-creator): the persona
        // record goes through the SAME parser as designer .persona files;
        // the stored look joins npcLooks like everyone else's.
        for (const StoredCharacter& stored : characterStore.loadAll()) {
            // Generated village characters (gen_*) belong to their maps,
            // not the town roster (issue #129).
            if (stored.characterId.rfind("gen_", 0) == 0) continue;
            const PersonaParseResult parsed =
                parsePersonaText(stored.personaText, stored.characterId);
            if (!parsed.ok) {
                std::cerr << "[llm_npc] stored persona error: " << parsed.error << "\n";
                continue;
            }
            Npc npc(parsed.value.persona, client);
            npc.setPlacement(parsed.value.position, parsed.value.facingDeg,
                             parsed.value.spotId);
            npc.setSchedule(parsed.value.schedule);
            npcLooks.push_back(stored.look);
            world.addNpc(std::move(npc));
        }
        for (Npc& npc : world.npcs()) {
            const NpcMemory memory = memoryStore.load(npc.persona().name);
            if (!memory.summary.empty()) npc.setMemory(memory.summary);
        }
        std::cerr << "[llm_npc] loaded " << world.npcs().size() << " NPCs ("
                  << world.npcs().size() - roster.size() << " player-created)\n";
    };

    // Rebuilds every per-NPC side array + gossip block for the CURRENT
    // world contents. Call after ANY repopulation.
    const auto resetNpcSideArrays = [&]() {
        savedTurns.assign(world.npcs().size(), 0);
        wasCaught.assign(world.npcs().size(), false);
        npcLastPos.resize(world.npcs().size());
        for (std::size_t i = 0; i < world.npcs().size(); ++i) {
            npcLastPos[i] = world.npcs()[i].position();
        }
        for (Npc& npc : world.npcs()) {
            refreshGossip(npc);
            npc.setTraitRegistry(&traitLibrary);
            for (const std::string& id : npc.persona().traitIds) {
                const bool known =
                    std::any_of(traitLibrary.begin(), traitLibrary.end(),
                                [&](const TraitDef& d) { return d.id == id; });
                if (!known) {
                    std::cerr << "[llm_npc] " << npc.persona().name
                              << ": unknown trait '" << id
                              << "' — ignored (stale-id demotion)\n";
                }
            }
        }
    };

    if (!mapFile) {
        spawnTownRoster();
    }
    resetNpcSideArrays();

    // ---- the mystery (issue #220) ----
    //
    // HOST-ONLY GROUND TRUTH. `mysterySetup` is the answer sheet: never written
    // to WorldState, never serialized, never rendered. The only sanctioned read
    // of the killer is voteIsCorrect. Everything a player can ever learn goes
    // onto the fact bus through seedMysteryFacts, which commits nothing that
    // names the killer as the killer.
    MysterySetup mysterySetup;
    // This match's generated opening. Empty beats means no mystery, or no
    // template on disk; both are survivable and neither blocks the match.
    CutsceneDef matchOpening;
    if (bootMystery && mapFile != nullptr) {
        // They do not compose, and the failure is silent rather than loud:
        // --map loads its city and respawns its NPCs further down, AFTER this
        // runs, so the victim would be seeded dead and then replaced by a
        // living map NPC while the facts stayed on the bus. A mystery whose
        // victim is walking around is not a mystery.
        std::cerr << "[llm_npc] --mystery ignored: --map replaces the roster "
                     "this mystery would be cast onto\n";
        bootMystery = false;
    }
    if (bootMystery) {
        // Cast onto who is actually in the world, not onto the persona files.
        // A template citing a resident who never spawned is a clue that can
        // never resolve.
        std::vector<Persona> living;
        living.reserve(world.npcs().size());
        for (const Npc& npc : world.npcs()) living.push_back(npc.persona());

        std::vector<std::string> storylineErrors;
        const std::vector<StorylineDef> storylines =
            loadStorylines(projectRoot / "storylines", &storylineErrors);
        for (const auto& err : storylineErrors) {
            std::cerr << "[llm_npc] storyline error: " << err << "\n";
        }

        // FAIL CLOSED. A template with any structural error is not offered to
        // the generator: half a mystery is worse than none, because a player
        // cannot tell the difference between an unsolvable case and a hard one.
        std::vector<const StorylineDef*> usable;
        for (const StorylineDef& story : storylines) {
            const auto problems =
                validateStoryline(story, static_cast<int>(living.size()));
            if (problems.empty()) {
                usable.push_back(&story);
                continue;
            }
            std::cerr << "[llm_npc] storyline \"" << story.id
                      << "\" rejected (" << problems.size() << " problems):\n";
            for (const StorylineError& problem : problems) {
                std::cerr << "    " << problem.where << ": " << problem.reason << "\n";
            }
        }

        if (usable.empty()) {
            std::cerr << "[llm_npc] --mystery: no usable storyline in storylines/ "
                         "— no mystery this session\n";
        } else {
            // Deterministic from the seed, like everything else in this chain:
            // a match has to be replayable without shipping the answer.
            const StorylineDef& chosen =
                *usable[mysterySeed % usable.size()];

            mysterySetup = generateMystery(living, mysterySeed);
            castStoryline(chosen, living, mysterySetup, mysterySeed);
            placeBodyClearOfColliders(mysterySetup, world.city());
            startVictimDead(world.npcs(), mysterySetup);
            seedMysteryFacts(world.state(), mysterySetup, living);

            // WITHOUT THIS THE DEMO DOES NOT WORK. refreshGossip is what puts
            // an NPC's known facts into their prompt, and resetNpcSideArrays
            // already ran it — before any of these facts existed. A witness
            // would have no idea they saw anything until a gossip tick
            // happened to reach them, which needs the fact to age past
            // kGossipMinAgeSeconds AND a proximity roll to land.
            //
            // The loop that makes this a detective game is: seeded testimony
            // -> the witness's prompt -> the player asks -> the NPC says it ->
            // fact extraction proposes it with playerLearned -> commitFact
            // grants it to the player -> the Journal shows it, flagged against
            // any account that contradicts it.
            for (Npc& npc : world.npcs()) refreshGossip(npc);

            // Build THIS match's opening from the authored template (#230).
            // Held for the whole match rather than rebuilt on demand, because
            // the Journal will replay it (#231) and a player who went back to
            // re-read the clock has to see the same clock.
            //
            // Only three fields cross this call, and that is the leak defence:
            // the killer is not in scope at the call site, so a reviewer can
            // check the rule by reading the line.
            if (const CutsceneDef* tmpl = findCutscene(cutsceneLibrary, "opening")) {
                matchOpening = buildOpeningCutscene(*tmpl, mysterySetup.sceneZoneId,
                                                    mysterySetup.murderHour,
                                                    mysterySetup.bodyPosition);
            } else {
                std::cerr << "[llm_npc] --mystery: no cutscenes/opening.cutscene "
                             "— starting without the opening\n";
            }

            // The victim and the scene are public knowledge the moment a body
            // is found, so naming them here leaks nothing. The killer is not
            // printed, and must not be — stderr is the first place a curious
            // player looks.
            std::cerr << "[llm_npc] --mystery: \"" << chosen.title << "\" seed "
                      << mysterySeed << " — " << mysterySetup.victim
                      << " found dead at " << zoneName(mysterySetup.sceneZoneId)
                      << " (" << mysterySetup.bodyPosition.x << ", "
                      << mysterySetup.bodyPosition.z << "), "
                      << mysterySetup.witnesses.size() << " witnesses, "
                      << mysterySetup.evidence.size() << " clues\n";
        }
    }

    // Character creator: persists BOTH records (independently) and spawns
    // the new citizen immediately. Declared after the per-NPC bookkeeping
    // vectors because a runtime spawn must grow them all.
    Menu::CreatorHooks creatorHooks;
    creatorHooks.onCreate = [&](const std::string& name, const std::string& backstory,
                                const std::string& traits,
                                const CharacterLook& look,
                                const std::string& traitId) -> std::string {
        std::string why;
        if (!lookIsValid(look, &why)) return why;

        LoadedPersona loaded;
        loaded.persona.name = name;
        loaded.persona.extraDirectives = backstory;
        std::istringstream traitsIn(traits);
        std::string trait;
        while (std::getline(traitsIn, trait, ',')) {
            trait = trim(trait);
            if (!trait.empty()) loaded.persona.traits.push_back(trait);
        }
        if (!traitId.empty()) loaded.persona.traitIds.push_back(traitId);
        loaded.spotId = "plaza";
        loaded.facingDeg = 180.f;
        // First clear standing spot on rings around the plaza center.
        loaded.position = Vec3{0.f, 0.f, -10.f};
        for (const float radius : {8.f, 11.f, 14.f, 17.f}) {
            bool placed = false;
            for (int step = 0; step < 12 && !placed; ++step) {
                const float angle = degToRad(static_cast<float>(step) * 30.f);
                const Vec3 spot{std::sin(angle) * radius, 0.f,
                                std::cos(angle) * radius};
                if (!world.city().circleIntersectsAny(spot.x, spot.z, 0.7f)) {
                    loaded.position = spot;
                    placed = true;
                }
            }
            if (placed) break;
        }

        const std::string characterId =
            "custom_" + std::to_string(static_cast<long long>(std::time(nullptr))) +
            "_" + std::to_string(world.npcs().size());
        loaded.id = characterId;
        const bool persisted = characterStore.savePersona(
                                   characterId, renderPersonaText(loaded)) &&
                               characterStore.saveLook(characterId, look);

        Npc npc(loaded.persona, client);
        npc.setPlacement(loaded.position, loaded.facingDeg, loaded.spotId);
        npcLooks.push_back(look);
        world.addNpc(std::move(npc));
        wasCaught.push_back(false);
        npcLastPos.push_back(loaded.position);
        savedTurns.push_back(0);

        if (!persisted) {
            std::cerr << "[llm_npc] character \"" << name
                      << "\" spawned but could not be persisted\n";
        }
        return "";
    };
    menu.setCreator(creatorHooks);

    // Avatar mode on the same creator page (issue #106): Save validates,
    // persists under the reserved id, and retints the first-person arm —
    // the surface where the player actually sees their choice.
    Menu::AvatarHooks avatarHooks;
    avatarHooks.current = [&]() { return avatarLook; };
    avatarHooks.onSave = [&](const CharacterLook& look) -> std::string {
        std::string why;
        if (!lookIsValid(look, &why)) return "Invalid look: " + why;
        avatarLook = look;
        renderer.setAvatarPalette(look.paletteId);
        if (!characterStore.saveLook("player_avatar", look)) {
            return "Saved for this session only (store unavailable)";
        }
        return "";
    };
    menu.setAvatar(avatarHooks);

    // Group conversations (issues #122-#124): additive alongside the solo
    // DialogueSession; active() discriminates. One streamed turn at a time.
    GroupSession group;
    std::uint64_t groupPendingId = 0;  // request id of the streaming turn
    double groupTurnStart = 0.0;       // for the latency log (issue #124)
    // One group turn (issue #122): the speaker gets THEIR OWN persona
    // prompt plus the labeled transcript, through the same ask/route/
    // stream pipeline solo talk uses — so directives, moods, history and
    // memory summaries all keep working per participant.
    const auto submitGroupTurn = [&](int npcIndex) {
        if (npcIndex < 0 || npcIndex >= static_cast<int>(world.npcs().size())) return;
        Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
        std::string context = "You are in a group conversation. Present: Player";
        for (const int i : group.participants()) {
            context += ", " + world.npcs()[static_cast<std::size_t>(i)].persona().name;
        }
        context += ".\nThe conversation so far, speakers labeled:\n" +
                   group.renderTranscript() +
                   "Reply with only your own next line, in character, to whoever "
                   "spoke last.";
        const std::uint64_t id = npc.ask(context);
        pendingRoutes[id] = npcIndex;
        groupPendingId = id;
        groupTurnStart = GetTime();
        dialog.beginStreaming(npc.persona().name);
        dialog.setInputEnabled(false);
    };

    // Trait rating loop (issue #118): review files only, human-curated —
    // rating a reply provably never changes a live prompt.
    RatingLog ratingLog(projectRoot / "saves" / "ratings");
    int ratedNpc = -1;                 // one rating per (npc, reply)
    std::size_t ratedHistorySize = 0;

    // ---- Sandbox editor state (issue #112) ----------------------------
    SandboxMap sandboxDoc;
    std::string sandboxSlug;
    bool sandboxActive = false;         // a sandbox map is loaded (edit OR play)
    bool sandboxOpenRequested = false;  // set by the menu hook, handled below
    std::string sandboxOpenStem;
    int sandboxPieceIndex = 0;
    bool sandboxPlacingNpc = false;     // Tab toggles piece vs NPC placement
    int sandboxNpcIndex = 0;
    std::vector<std::string> sandboxNpcSources;  // built on map open
    std::unordered_map<std::string, CharacterLook> sandboxLookCache;
    PlacedPiece sandboxGhost;           // cursor preview, shared input->render
    PlacedNpc sandboxGhostNpc;          // ditto when placing NPCs
    bool sandboxGhostValid = false;
    bool sandboxGhostLive = false;
    Vec3 sandboxCam{};                  // pan target on the ground plane
    float sandboxZoom = 45.f;
    const fs::path mapsDir = projectRoot / "saves" / "maps";
    const auto saveSandboxDoc = [&]() {
        if (!sandboxActive) return;
        std::error_code ec;
        fs::create_directories(mapsDir, ec);
        std::ofstream out(mapsDir / (sandboxSlug + ".json"));
        out << sandboxDoc.toJson();
    };
    const auto exitSandboxToTown = [&]() {
        saveSandboxDoc();
        sandboxActive = false;
        world.loadCity(City::makeDowntown());
        spawnTownRoster();
        resetNpcSideArrays();
        player.position = Vec3{0.f, 0.f, 24.f};
        playerVerticalSpeed = 0.f;
    };
    // The look a placed-NPC source renders with in the EDITOR (play mode
    // spawns real NPCs; this is just the preview), memoized per map open.
    const auto lookForSource = [&](const std::string& source) -> const CharacterLook& {
        auto it = sandboxLookCache.find(source);
        if (it != sandboxLookCache.end()) return it->second;
        LoadedPersona loaded;
        CharacterLook look;
        if (resolvePlacedNpc(PlacedNpc{source, 0.f, 0.f, 0.f}, roster,
                             characterStore, loaded)) {
            std::string why;
            look = lookForPersona(loaded.persona.name,
                                  loaded.hasLook ? &loaded.look : nullptr, &why);
        } else {
            look = lookForPersona(source);  // deterministic placeholder
        }
        return sandboxLookCache.emplace(source, std::move(look)).first->second;
    };

    // Spawns the map's placed NPCs into the CURRENT world (play mode) —
    // real personas/looks through the standard pipeline, schedules
    // suppressed by resolvePlacedNpc. Unknown sources skip with a log.
    const auto spawnMapNpcs = [&](const SandboxMap& map) {
        npcLooks.clear();
        for (const PlacedNpc& placed : map.npcs) {
            LoadedPersona loaded;
            if (!resolvePlacedNpc(placed, roster, characterStore, loaded)) {
                std::cerr << "[llm_npc] sandbox: skipping npc '" << placed.source
                          << "' (unknown or records missing)\n";
                continue;
            }
            Npc npc(loaded.persona, client);
            npc.setPlacement(loaded.position, loaded.facingDeg, loaded.spotId);
            std::string lookWhy;
            npcLooks.push_back(lookForPersona(
                loaded.persona.name, loaded.hasLook ? &loaded.look : nullptr,
                &lookWhy));
            const NpcMemory memory = memoryStore.load(loaded.persona.name);
            if (!memory.summary.empty()) npc.setMemory(memory.summary);
            world.addNpc(std::move(npc));
        }
        std::cerr << "[llm_npc] sandbox: spawned " << world.npcs().size()
                  << " placed NPCs\n";
    };

    Menu::SandboxHooks sandboxHooks;
    sandboxHooks.listMaps = [&]() {
        std::vector<std::string> stems;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(mapsDir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                stems.push_back(entry.path().stem().string());
            }
        }
        std::sort(stems.begin(), stems.end());
        return stems;
    };
    sandboxHooks.onOpen = [&](const std::string& stem) {
        sandboxOpenRequested = true;
        sandboxOpenStem = stem;
    };
    // LLM map generation (issue #129): an async generate-validate-retry
    // chain over internal requests; results save as a normal map file and
    // open in EDIT mode — always a draft the player owns, never locked.
    std::uint64_t worldgenRequestId = 0;
    int worldgenAttempt = 0;
    std::string worldgenDescription;
    std::string worldgenStatus;         // HUD line
    float worldgenStatusTtl = 0.f;
    const auto worldgenSay = [&](const std::string& s) {
        worldgenStatus = s;
        worldgenStatusTtl = 6.f;
        std::cerr << "[llm_npc] worldgen: " << s << "\n";
    };
    const auto submitWorldgen = [&](const std::string& user) {
        worldgenRequestId = client.submit(buildVillagePrompt(traitLibrary), {}, user);
    };
    sandboxHooks.onGenerate = [&](const std::string& description) -> std::string {
        if (worldgenRequestId != 0) return "Already generating...";
        worldgenDescription = description;
        worldgenAttempt = 1;
        submitWorldgen(description);
        worldgenSay("Generating '" + description.substr(0, 40) + "'...");
        return "Generating... (watch the status line)";
    };
    // Model picker (ported from the pluggable-llm branch): switch the live
    // client and persist the choice to config/llm.cfg; in-flight requests
    // finish on the old model, new ones use the new pick.
    Menu::ModelHooks modelHooks;
    modelHooks.listModels = [&]() { return client.availableModels(); };
    modelHooks.currentModel = [&]() { return client.model(); };
    modelHooks.onSelect = [&](const std::string& model) -> std::string {
        client.setModel(model);
        if (!setKvValue(configDir / "llm.cfg", "model", model)) {
            return "Switched for this session (could not write llm.cfg)";
        }
        return "";
    };
    menu.setModels(modelHooks);

    menu.setSandbox(sandboxHooks);
    {
        std::vector<std::string> traitIds;
        for (const TraitDef& trait : traitLibrary) traitIds.push_back(trait.id);
        menu.setTraitChoices(std::move(traitIds));
    }

    if (mapFile) {
        // Fixture boot (--map): compile and load the sandbox map instead
        // of the town, with its placed NPCs spawned — headless smoke shots
        // and instant play-testing of a saved map.
        std::ifstream mapIn(mapFile);
        std::ostringstream mapBuf;
        mapBuf << mapIn.rdbuf();
        SandboxMap bootMap;
        if (!mapIn || !SandboxMap::fromJson(mapBuf.str(), bootMap)) {
            std::cerr << "[llm_npc] --map: cannot read/parse " << mapFile
                      << " — starting the town instead\n";
            spawnTownRoster();
            resetNpcSideArrays();
        } else {
            for (const MapError& e : validateMap(bootMap)) {
                std::cerr << "[llm_npc] --map " << e.where << ": " << e.reason
                          << "\n";
            }
            sandboxDoc = bootMap;
            sandboxSlug = fs::path(mapFile).stem().string();
            sandboxActive = true;
            sandboxNpcSources.clear();
            for (const auto& loaded : roster) {
                sandboxNpcSources.push_back("persona:" + loaded.id);
            }
            world.loadCity(buildCity(sandboxDoc));
            spawnMapNpcs(sandboxDoc);
            resetNpcSideArrays();
            std::cerr << "[llm_npc] --map: loaded '" << bootMap.name << "' ("
                      << world.city().buildings().size() << " solid pieces, "
                      << world.npcs().size() << " NPCs)\n";
            if (bootSandboxEdit) {
                sandboxNpcSources.clear();
                for (const auto& loaded : roster) {
                    sandboxNpcSources.push_back("persona:" + loaded.id);
                }
                mode = AppMode::SandboxEdit;
                EnableCursor();
            }
        }
    }

    // --cutscene <id>: boot straight into a scene so it has a visual-QA path.
    // Fixed-step playback pins frame N to the same moment every run, which is
    // the only reason a capture can serve as a regression signal.
    // The win montage is built AT THE MOMENT OF THE WIN, not at match start:
    // it splits the clue chain by what the player actually learned, and that
    // is still changing right up to the vote. Held as a lambda rather than a
    // value for exactly that reason.
    const auto buildWinNow = [&]() -> CutsceneDef {
        const CutsceneDef* tmpl = findCutscene(cutsceneLibrary, "win");
        if (tmpl == nullptr || mysterySetup.killer.empty()) return CutsceneDef{};
        const std::vector<ClueStep> chain = solutionChain(mysterySetup);
        const MontagePlan plan = buildMontage(chain, world.state(), "player");
        // Naming the killer is correct HERE and nowhere earlier: this plays
        // after the answer is already out.
        return buildWinCutscene(*tmpl, plan, mysterySetup.killer);
    };

    if (bootMenuPage != nullptr) {
        const auto page = Menu::pageFromName(bootMenuPage);
        if (!page) {
            std::cerr << "[llm_npc] --menu: unknown page \"" << bootMenuPage
                      << "\" (main, controls, multiplayer, creator, journal, "
                         "sandbox, model)\n";
        } else {
            menu.showPage(*page);
            mode = AppMode::Menu;
            EnableCursor();
        }
    }

    if (bootCutscene != nullptr) {
        // Both of these are GENERATED per match, so playing the raw template
        // would show a player the literal text "{hour}" or "{clue}" — which is
        // exactly what shipped in #230 before a capture caught it. Prefer the
        // built one; fall back to the template so a scene is still framable
        // without a mystery.
        const bool wantsOpening = std::strcmp(bootCutscene, "opening") == 0;
        const bool wantsWin = std::strcmp(bootCutscene, "win") == 0;
        CutsceneDef generatedWin;
        if (wantsWin) generatedWin = buildWinNow();
        const CutsceneDef* scene =
            (wantsOpening && !matchOpening.beats.empty()) ? &matchOpening
            : (wantsWin && !generatedWin.beats.empty())   ? &generatedWin
                                                          : findCutscene(cutsceneLibrary, bootCutscene);
        if (scene == nullptr) {
            std::cerr << "[llm_npc] --cutscene: no cutscene named \""
                      << bootCutscene << "\" in cutscenes/\n";
        } else {
            cutscene.setFixedStep(true);
            playCutscene(*scene);
        }
    } else if (!matchOpening.beats.empty() && bootMenuPage == nullptr) {
        // A mystery starts with its opening. Smoke runs included: a scene
        // nobody ever captures is a scene nobody ever checks, which is how the
        // map editor shipped with a menu drawn over it (#215).
        //
        // Unless --menu asked for a page. This block runs after the --menu
        // block and playCutscene takes the mode, so without the guard an
        // explicit request is silently overridden — every one of the first
        // seven --menu captures came back showing the opening instead. An
        // explicit flag beats an automatic one.
        if (smokeRun) cutscene.setFixedStep(true);
        playCutscene(matchOpening);
    }

    // Journal: a pure read of the shared fact store — what the player was
    // personally told, grouped by subject, conflicts pre-flagged by core.
    Menu::JournalHooks journalHooks;
    journalHooks.entries = [&]() {
        std::vector<Menu::JournalRow> rows;
        for (const JournalEntry& entry : journalEntries(world.state())) {
            Menu::JournalRow row;
            row.subject = entry.fact->subject;
            row.content = entry.fact->content;
            row.attribution = "heard from " + entry.fact->source + " at " +
                              clockLabel(entry.fact->learnedAtSeconds);
            row.conflicting = entry.conflicting;
            rows.push_back(std::move(row));
        }
        return rows;
    };
    menu.setJournal(journalHooks);
    // Creator preview rotation (issue #93): player-driven, with an idle return
    // to facing the camera. No autonomous spin.
    float previewYaw = 0.f;
    float previewIdleSeconds = 0.f;
    bool previewWasOpen = false;  // detects the frame the Creator page opens
    bool previewDragging = false;  // latched at press: is this a preview drag or a control click?

    // Where every agent has been this session (issue #165). Ground truth,
    // written every frame and read by nothing yet.
    LocationLog locationLog;

    // Gossip propagation cadence (real seconds between ticks) and its rng
    // (seeded for reproducible town behavior in a session).
    float gossipTickTimer = 0.f;
    std::mt19937 gossipRng(20260706u);

    long frames = 0;
    while (!WindowShouldClose() && (maxFrames < 0 || frames++ < maxFrames)) {
        const float dt = std::fmin(0.03f, GetFrameTime());

        // A dropped link falls back to solo play; the menu status explains.
        if (netClient && !netClient->connected()) {
            leaveSession();
            if (mode == AppMode::Dialogue) {
                session.close();
                dialog.endStreaming();
                mode = AppMode::Playing;
                DisableCursor();
            }
        }
        const bool joined = netClient != nullptr;  // connected, per the check above

        // ---- input ----
        if (mode == AppMode::Cutscene) {
            // Playback owns the camera and swallows everything else. Only skip
            // is reachable, and only when the scene allows it.
            if (!smokeRun && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                              IsKeyPressed(KEY_ESCAPE))) {
                cutscene.skip();
            }
            if (!cutscene.advance(dt)) {
                // Restore everything playback took: the pose, the mode and the
                // cursor. A cutscene that leaves the camera moved, the mouse
                // swallowed or a mode stuck is worse than one that never ran.
                player = preCutscenePlayer;
                mode = preCutsceneMode;
                if (mode == AppMode::Playing || mode == AppMode::Dialogue) {
                    DisableCursor();
                } else {
                    EnableCursor();
                }
            }
        } else if (mode == AppMode::Playing) {
            if (IsWindowFocused() && !smokeRun) applyMouseLook(player);
            if (sandboxActive && IsKeyPressed(KEY_P)) {
                // Back from play-testing to the editor: clear the live
                // NPCs (the document is the truth in edit mode).
                world.loadCity(buildCity(sandboxDoc));
                resetNpcSideArrays();
                mode = AppMode::SandboxEdit;
                EnableCursor();
            }

            if (jailSecondsLeft <= 0.f) {  // no walking out of a sentence
                Vec3 wish{};
                if (isActionPressed(bindings, Action::MoveForward)) wish += flatForward(player.yawDeg);
                if (isActionPressed(bindings, Action::MoveBackward)) wish += flatForward(player.yawDeg) * -1.f;
                if (isActionPressed(bindings, Action::StrafeRight)) wish += flatRight(player.yawDeg);
                if (isActionPressed(bindings, Action::StrafeLeft)) wish += flatRight(player.yawDeg) * -1.f;
                wish = normalize(wish);
                const Vec3 target = player.position + wish * (kWalkSpeed * dt);
                player.position = world.city().resolveMovement(player.position, target, kPlayerRadius);

                // Vertical: gravity + jump (user request: curb-line props
                // snagged the player). Support is whatever the feet rest on
                // — the ground or a low prop's top — so a hop clears
                // benches/bushes, lands on car roofs, and walking off an
                // edge falls. The 0.05 tolerance re-catches a top the fall
                // integrated slightly past this frame.
                const float support = world.city().supportHeightAt(
                    player.position.x, player.position.z, kPlayerRadius,
                    player.position.y + 0.05f);
                const bool grounded = playerVerticalSpeed <= 0.f &&
                                      player.position.y <= support + 0.001f;
                if (grounded) {
                    player.position.y = support;
                    playerVerticalSpeed = 0.f;
                    if (isActionJustPressed(bindings, Action::Jump)) {
                        playerVerticalSpeed = kJumpSpeed;
                    }
                } else {
                    playerVerticalSpeed -= kGravity * dt;
                }
                player.position.y += playerVerticalSpeed * dt;
                if (player.position.y < 0.f) player.position.y = 0.f;  // ground floor
            }

            // Combat input (solo/host only — combat is host-authoritative
            // and not replicated to guests yet).
            if (!joined && jailSecondsLeft <= 0.f) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    // Fire along the authoritative look vector (yaw AND pitch),
                    // the same one the camera aims — so the shot goes exactly
                    // where the crosshair points (issue #91). flatForward is
                    // kept above for WASD, which is intentionally horizontal.
                    world.playerAttack(lookDirection(player.yawDeg, player.pitchDeg));
                }
                if (IsKeyPressed(KEY_ONE)) world.playerSwitchWeapon(WeaponKind::Fist);
                if (IsKeyPressed(KEY_TWO)) world.playerSwitchWeapon(WeaponKind::Pistol);
            }

            const bool menuKey =
                isActionJustPressed(bindings, Action::OpenMenu) || IsKeyPressed(KEY_ESCAPE);
            if (menuKey) {
                menu.open();
                mode = AppMode::Menu;
                EnableCursor();
            } else if (isActionJustPressed(bindings, Action::Talk) && nearbyNpc >= 0 &&
                       nearbyNpc < static_cast<int>(world.npcs().size()) &&
                       world.npcs()[static_cast<std::size_t>(nearbyNpc)].combatState() ==
                           NpcState::Idle) {
                // Followers join the conversation (issue #122): player + up
                // to 3 NPCs, the brief's latency cap.
                std::vector<int> members{nearbyNpc};
                std::vector<std::string> memberNames{
                    world.npcs()[static_cast<std::size_t>(nearbyNpc)].persona().name};
                if (!joined) {
                    for (int i = 0; i < static_cast<int>(world.npcs().size()) &&
                                    members.size() < 3;
                         ++i) {
                        if (i == nearbyNpc) continue;
                        Npc& candidate = world.npcs()[static_cast<std::size_t>(i)];
                        if (candidate.behavior() == NpcAction::Follow &&
                            candidate.combatState() == NpcState::Idle) {
                            members.push_back(i);
                            memberNames.push_back(candidate.persona().name);
                        }
                    }
                }
                if (members.size() >= 2) {
                    group.open(members, memberNames);
                    groupPendingId = 0;
                    for (const int i : members) {
                        world.npcs()[static_cast<std::size_t>(i)].lookAt(player.position);
                    }
                    std::string party = "Talking with";
                    for (const std::string& n : memberNames) party += " " + n + ",";
                    party.back() = '.';
                    dialog.reset();
                    dialog.appendLine({TranscriptLine::Kind::System, "", party});
                    dialog.setInputEnabled(true);
                    mode = AppMode::Dialogue;
                    EnableCursor();
                    continue;  // group path complete; skip the solo open
                }
                session.open(nearbyNpc);
                Npc& npc = world.npcs()[static_cast<std::size_t>(nearbyNpc)];
                if (joined) {
                    netClient->sendChatOpen(nearbyNpc);
                } else {
                    npc.lookAt(player.position);
                }
                dialog.reset();
                dialog.setInputEnabled(true);
                dialog.swallowPendingText();
                dialog.appendLine({TranscriptLine::Kind::System, "",
                                   "You are talking to " + npc.persona().name + " (" +
                                       npc.persona().role + "). Enter sends, Esc leaves."});
                mode = AppMode::Dialogue;
                EnableCursor();
            }
        } else if (mode == AppMode::Dialogue) {
            if (group.active()) {
                // Combat doesn't pause for talk: drop dead/arrested
                // participants with a note; close when nobody is left.
                for (const int i : std::vector<int>(group.participants())) {
                    if (world.npcs()[static_cast<std::size_t>(i)].combatState() ==
                        NpcState::Dead) {
                        dialog.appendLine(
                            {TranscriptLine::Kind::System, "",
                             group.nameOf(i) + " is no longer with us."});
                        group.removeParticipant(i);
                    }
                }
                if (group.participants().empty()) {
                    group.close();
                    groupPendingId = 0;
                    mode = AppMode::Playing;
                    DisableCursor();
                }
            }
            // Rating capture (issue #118): F1 keeps the last completed reply
            // as a trait-example candidate, F2 logs it for review. F-keys on
            // purpose — +/- would collide with the text input. One rating
            // per reply; nothing changes in live prompts.
            if (!joined && session.npcIndex() >= 0 &&
                session.npcIndex() < static_cast<int>(world.npcs().size())) {
                Npc& ratedTarget =
                    world.npcs()[static_cast<std::size_t>(session.npcIndex())];
                const auto& history = ratedTarget.history();
                const bool ratable =
                    !client.busy() && history.size() >= 2 &&
                    history.back().role == "assistant" &&
                    !(ratedNpc == session.npcIndex() &&
                      ratedHistorySize == history.size());
                if (ratable &&
                    (IsKeyPressed(KEY_F1) || IsKeyPressed(KEY_F2))) {
                    const std::string playerLine =
                        history[history.size() - 2].content;
                    const std::string npcLine = history.back().content;
                    const bool good = IsKeyPressed(KEY_F1);
                    const bool wrote =
                        good ? ratingLog.appendCandidate(
                                   ratedTarget.persona().name,
                                   ratedTarget.persona().traitIds, playerLine,
                                   npcLine)
                             : ratingLog.appendRejected(
                                   ratedTarget.persona().name,
                                   ratedTarget.persona().traitIds, playerLine,
                                   npcLine);
                    if (wrote) {
                        ratedNpc = session.npcIndex();
                        ratedHistorySize = history.size();
                    }
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                // Leaving a conversation kicks off memory updates and fact
                // extraction (solo/host only — a guest's conversations live
                // host-side). Groups: EVERY participant summarizes from
                // their own history (which holds the labeled transcript),
                // and facts extract per participant, attributed to them
                // (issue #123).
                if (group.active()) {
                    for (const int i : group.participants()) {
                        requestSummary(i);
                        requestFacts(i);
                    }
                    group.close();
                    groupPendingId = 0;
                } else if (!joined) {
                    requestSummary(session.npcIndex());
                    requestFacts(session.npcIndex());
                }
                session.close();
                dialog.endStreaming();
                mode = AppMode::Playing;
                DisableCursor();
            } else {
                const std::string submitted = dialog.pollInput();
                if (!submitted.empty() && group.active()) {
                    if (groupPendingId == 0) {  // one streamed turn at a time
                        dialog.appendLine(
                            {TranscriptLine::Kind::Player, "You", submitted});
                        group.notePlayerTurn();
                        group.addLine("Player", submitted);
                        submitGroupTurn(group.resolveNextSpeaker(submitted));
                    }
                } else if (!submitted.empty() && session.isOpen()) {
                    Npc& npc = world.npcs()[static_cast<std::size_t>(session.npcIndex())];
                    if (joined) {
                        // Joined: the host owns the NPC — send the line up.
                        if (!netChatWaiting) {
                            dialog.appendLine({TranscriptLine::Kind::Player, "You", submitted});
                            netClient->sendChatLine(session.npcIndex(), submitted);
                            netChatWaiting = true;
                            dialog.beginStreaming(npc.persona().name);
                            dialog.setInputEnabled(false);
                        }
                    } else if (!npc.waiting()) {
                        dialog.appendLine({TranscriptLine::Kind::Player, "You", submitted});
                        const std::uint64_t id = npc.ask(submitted);
                        session.submitted(id);
                        pendingRoutes[id] = session.npcIndex();
                        dialog.beginStreaming(npc.persona().name);
                        dialog.setInputEnabled(false);
                    }
                }
            }
        } else if (mode == AppMode::Dead) {
            if (IsKeyPressed(KEY_ENTER)) {
                // Respawn: full health at the spawn point; surviving NPCs
                // stand down (the dead stay dead).
                world.player().hp = world.player().hpMax;
                player.position = Vec3{0.f, 0.f, 24.f};
                for (Npc& npc : world.npcs()) npc.calmDown();
                mode = AppMode::Playing;
                DisableCursor();
            }
        } else if (mode == AppMode::SandboxEdit) {
            // ---- Sandbox editor (issue #112): pan/zoom, ghost cursor,
            // place/delete, cycle pieces; P plays the map, Escape saves
            // and returns to town.
            const float pan = sandboxZoom * 0.9f * dt;
            if (isActionPressed(bindings, Action::MoveForward)) sandboxCam.z -= pan;
            if (isActionPressed(bindings, Action::MoveBackward)) sandboxCam.z += pan;
            if (isActionPressed(bindings, Action::StrafeLeft)) sandboxCam.x -= pan;
            if (isActionPressed(bindings, Action::StrafeRight)) sandboxCam.x += pan;
            sandboxZoom = clampf(sandboxZoom - GetMouseWheelMove() * 4.f, 16.f, 90.f);
            const float edge = world.city().halfSize();
            sandboxCam.x = clampf(sandboxCam.x, -edge, edge);
            sandboxCam.z = clampf(sandboxCam.z, -edge, edge);

            const auto& catalog = pieceCatalog();
            const int pieceCount = static_cast<int>(catalog.size());
            const int npcCount = static_cast<int>(sandboxNpcSources.size());
            if (IsKeyPressed(KEY_TAB) && npcCount > 0) {
                sandboxPlacingNpc = !sandboxPlacingNpc;  // pieces <-> NPCs
            }
            const int cycle = IsKeyPressed(KEY_RIGHT_BRACKET) ? 1
                              : IsKeyPressed(KEY_LEFT_BRACKET) ? -1
                                                               : 0;
            if (cycle != 0) {
                if (sandboxPlacingNpc && npcCount > 0) {
                    sandboxNpcIndex = (sandboxNpcIndex + npcCount + cycle) % npcCount;
                } else {
                    sandboxPieceIndex =
                        (sandboxPieceIndex + pieceCount + cycle) % pieceCount;
                }
            }

            // Cursor tile from the previous frame's camera (one frame of
            // lag is invisible at editor pan speeds).
            Vec3 ground;
            sandboxGhostLive = renderer.screenToGround(GetMousePosition(), ground);
            if (sandboxGhostLive && sandboxPlacingNpc) {
                // NPC placement: free position (not grid-bound), validity =
                // the candidate document still validating for this entry.
                sandboxGhostNpc = PlacedNpc{sandboxNpcSources[static_cast<std::size_t>(
                                                sandboxNpcIndex)],
                                            ground.x, ground.z, 180.f};
                SandboxMap candidate = sandboxDoc;
                candidate.npcs.push_back(sandboxGhostNpc);
                sandboxGhostValid = true;
                const std::string newWhere =
                    "npcs[" + std::to_string(sandboxDoc.npcs.size()) + "]";
                for (const MapError& e : validateMap(candidate)) {
                    if (e.where == newWhere) {
                        sandboxGhostValid = false;
                        break;
                    }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && sandboxGhostValid) {
                    sandboxDoc.npcs.push_back(sandboxGhostNpc);
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    // Delete the nearest placed NPC within arm's reach.
                    int best = -1;
                    float bestD = 2.25f;  // 1.5u squared
                    for (int i = 0; i < static_cast<int>(sandboxDoc.npcs.size()); ++i) {
                        const float dx = sandboxDoc.npcs[static_cast<std::size_t>(i)].x - ground.x;
                        const float dz = sandboxDoc.npcs[static_cast<std::size_t>(i)].z - ground.z;
                        if (dx * dx + dz * dz < bestD) {
                            bestD = dx * dx + dz * dz;
                            best = i;
                        }
                    }
                    if (best >= 0) {
                        sandboxDoc.npcs.erase(sandboxDoc.npcs.begin() + best);
                    }
                }
            } else if (sandboxGhostLive) {
                const PieceDef& piece =
                    catalog[static_cast<std::size_t>(sandboxPieceIndex)];
                sandboxGhost.pieceId = piece.id;
                sandboxGhost.tileX =
                    static_cast<int>(std::floor(ground.x / kMapTileUnits)) -
                    piece.tilesW / 2;
                sandboxGhost.tileZ =
                    static_cast<int>(std::floor(ground.z / kMapTileUnits)) -
                    piece.tilesD / 2;
                // Validity: would the candidate document still validate,
                // with every error citing OTHER entries ignored?
                SandboxMap candidate = sandboxDoc;
                candidate.pieces.push_back(sandboxGhost);
                sandboxGhostValid = true;
                const std::string newWhere =
                    "pieces[" + std::to_string(sandboxDoc.pieces.size()) + "]";
                for (const MapError& e : validateMap(candidate)) {
                    if (e.where == newWhere) {
                        sandboxGhostValid = false;
                        break;
                    }
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && sandboxGhostValid) {
                    sandboxDoc.pieces.push_back(sandboxGhost);
                    world.loadCity(buildCity(sandboxDoc));
                }
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    // Delete the topmost (most recent) piece under the cursor.
                    for (int i = static_cast<int>(sandboxDoc.pieces.size()) - 1;
                         i >= 0; --i) {
                        const PieceDef* pd = findPiece(sandboxDoc.pieces[i].pieceId);
                        if (!pd) continue;
                        const float minX = static_cast<float>(sandboxDoc.pieces[i].tileX) * kMapTileUnits;
                        const float minZ = static_cast<float>(sandboxDoc.pieces[i].tileZ) * kMapTileUnits;
                        if (ground.x >= minX &&
                            ground.x <= minX + static_cast<float>(pd->tilesW) * kMapTileUnits &&
                            ground.z >= minZ &&
                            ground.z <= minZ + static_cast<float>(pd->tilesD) * kMapTileUnits) {
                            sandboxDoc.pieces.erase(sandboxDoc.pieces.begin() + i);
                            world.loadCity(buildCity(sandboxDoc));
                            break;
                        }
                    }
                }
            }

            if (IsKeyPressed(KEY_P)) {
                // Play the map: save, spawn the placed NPCs, then stand at
                // the first clear spot spiraling out from the center.
                saveSandboxDoc();
                spawnMapNpcs(sandboxDoc);
                resetNpcSideArrays();
                Vec3 spawn{0.f, 0.f, 0.f};
                for (float r = 4.f;
                     r <= 60.f &&
                     world.city().circleIntersectsAny(spawn.x, spawn.z, kPlayerRadius);
                     r += 4.f) {
                    spawn = Vec3{r, 0.f, r * 0.5f};
                }
                player.position = spawn;
                playerVerticalSpeed = 0.f;
                mode = AppMode::Playing;
                DisableCursor();
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                exitSandboxToTown();
                mode = AppMode::Playing;
                DisableCursor();
            }
        } else {  // Menu
            switch (menu.update(dt)) {
                case MenuResult::Resume:
                    mode = AppMode::Playing;
                    DisableCursor();
                    break;
                case MenuResult::Quit:
                    goto shutdown;  // single exit point below the loop
                case MenuResult::None:
                    break;
            }
        }
            if (sandboxOpenRequested) {
                sandboxOpenRequested = false;
                SandboxMap doc;
                std::string slug = sandboxOpenStem;
                bool ok = true;
                if (slug.empty()) {
                    int n = 1;
                    while (fs::exists(mapsDir / ("map-" + std::to_string(n) + ".json"))) {
                        ++n;
                    }
                    slug = "map-" + std::to_string(n);
                    doc.name = slug;
                } else {
                    std::ifstream in(mapsDir / (slug + ".json"));
                    std::ostringstream buf;
                    buf << in.rdbuf();
                    ok = static_cast<bool>(in) && SandboxMap::fromJson(buf.str(), doc);
                    if (!ok) {
                        std::cerr << "[llm_npc] sandbox: cannot open '" << slug
                                  << "'\n";
                    }
                }
                if (ok) {
                    sandboxDoc = std::move(doc);
                    sandboxSlug = slug;
                    sandboxActive = true;
                    world.loadCity(buildCity(sandboxDoc));
                    resetNpcSideArrays();
                    sandboxCam = Vec3{};
                    sandboxGhostLive = false;
                    sandboxPlacingNpc = false;
                    sandboxNpcIndex = 0;
                    sandboxLookCache.clear();
                    sandboxNpcSources.clear();
                    for (const auto& loaded : roster) {
                        sandboxNpcSources.push_back("persona:" + loaded.id);
                    }
                    for (const StoredCharacter& stored : characterStore.loadAll()) {
                        sandboxNpcSources.push_back("character:" + stored.characterId);
                    }
                    mode = AppMode::SandboxEdit;
                    EnableCursor();
                }
            }
        if (mode != AppMode::Menu && jailSecondsLeft > 0.f) jailSecondsLeft -= dt;

        // The ONE world clock advances here; every time-aware system below
        // (schedules, day/night) reads this shared value — none keeps its
        // own. The town keeps living while the menu is open.
        world.state().advanceTime(dt);
        const float worldHour = static_cast<float>(world.state().timeOfDayHours());

        // NPC behaviors keep running during dialogue, freeze in the menu;
        // joined clients never simulate (the host's snapshots are truth).
        if (mode != AppMode::Menu && !joined) {
            for (Npc& npc : world.npcs()) {
                // Combat movement (flee/hostile/dead) owns non-Idle NPCs;
                // conversational behaviors would fight it.
                if (npc.combatState() == NpcState::Idle) {
                    npc.update(dt, player.position, world.city(), worldHour);
                }
            }
        }

        // Gossip spreads between NPCs who are actually near each other
        // (schedules make them meet): conservative proximity + age + chance
        // gates, at most one fact per pair per tick. Knowledge only ever
        // flips on the shared bus — there is no NPC-to-NPC message channel.
        if (!joined) {
            gossipTickTimer += dt;
            if (gossipTickTimer >= 15.f) {
                gossipTickTimer = 0.f;
                std::vector<AgentAt> agents;
                for (const Npc& npc : world.npcs()) {
                    if (npc.combatState() != NpcState::Dead) {
                        agents.push_back({npc.persona().name, npc.position()});
                    }
                }
                if (propagateGossip(world.state(), agents, gossipRng) > 0) {
                    for (Npc& npc : world.npcs()) {
                        refreshGossip(npc);
                        for (const KnownFact* fact :
                             world.state().factsKnownBy(npc.persona().name)) {
                            factStore.saveKnowledge(npc.persona().name, fact->factId);
                        }
                    }
                }
            }
        }

        // The moment an officer catches the player: short stay at the station.
        for (std::size_t i = 0; i < world.npcs().size(); ++i) {
            Npc& npc = world.npcs()[i];
            const bool caughtNow = npc.hasCaughtPlayer();
            if (caughtNow && !wasCaught[i] && npc.persona().police) {
                jailSecondsLeft = kJailSeconds;
                if (const Building* station = world.city().findBuilding("police")) {
                    player.position = Vec3{(station->minX + station->maxX) * 0.5f, 0.f,
                                           station->maxZ + 2.f};
                }
                npc.commandReturnHome();
                if (mode == AppMode::Dialogue) {  // hauled off mid-sentence
                    if (!joined) {
                        requestSummary(session.npcIndex());
                        requestFacts(session.npcIndex());
                    }
                    session.close();
                    dialog.endStreaming();
                    mode = AppMode::Playing;
                    DisableCursor();
                }
            }
            wasCaught[i] = npc.hasCaughtPlayer();
        }

        // ---- combat simulation (solo/host; guests replicate later) ----
        if (mode != AppMode::Menu && mode != AppMode::Dead && !joined) {
            world.player().position = player.position;
            const CombatFrameResult combat = world.updateCombat(dt);
            for (const auto& hit : combat.npcsDamaged) {
                const Npc& npc = world.npcs()[hit.npcIndex];
                callouts.push_back({static_cast<int>(hit.npcIndex),
                                    hit.killedByHit
                                        ? npc.persona().name + " collapses!"
                                        : (npc.isArmed() ? "You'll regret that!" : "Ow! Help!"),
                                    3.f});
            }
            for (const auto& shot : combat.npcShots) {
                if (shot.landed) hurtFlash = 0.35f;
            }
            if (world.player().hp <= 0 && mode != AppMode::Dead) {
                mode = AppMode::Dead;
                EnableCursor();
            }
        }
        // Facing derives from actual motion once every mover has run —
        // behaviors and combat both; see Npc::deriveFacingFromMotion.
        if (mode != AppMode::Menu && !joined) {
            for (std::size_t i = 0; i < world.npcs().size(); ++i) {
                world.npcs()[i].deriveFacingFromMotion(npcLastPos[i], dt);
            }
        }

        // ---- where everyone has been (issue #165) ----
        //
        // OBSERVATION ONLY. Nothing in the game reads this yet; it is the
        // substrate the alibi questions will be asked of, and producing the
        // data is a separate change from consuming it. The log never
        // influences movement, schedules or rendering.
        //
        // The player is logged under "player" like anyone else. The
        // retaliation rule means a player has to be as observable as any
        // resident — an alibi system that cannot place the player is half a
        // system.
        //
        // Not ticked during the menu or a sandbox edit: someone rearranging a
        // map is not an agent with an alibi, and recording them would put a
        // building-placement session into the evidence.
        if (mode != AppMode::Menu && mode != AppMode::SandboxEdit) {
            // The same worldHour the rest of the frame uses. Reading the clock
            // a second time here would let an agent's trail disagree with the
            // timestamps on the facts it is meant to corroborate.
            for (const Npc& npc : world.npcs()) {
                if (npc.combatState() == NpcState::Dead) {
                    // Close the stay rather than skipping it, or a corpse
                    // accumulates one visit that never ends and every "who was
                    // in the bakery" answer names the dead.
                    //
                    // Guarded on retired() because closeAgent walks back
                    // through visits_ to find the agent's newest entry, and
                    // that walk lengthens as everyone else keeps appending.
                    if (!locationLog.retired(npc.persona().name)) {
                        locationLog.closeAgent(npc.persona().name, worldHour);
                    }
                    continue;
                }
                locationLog.observe(npc.persona().name, npc.position().x,
                                    npc.position().z, worldHour);
            }
            locationLog.observe("player", player.position.x, player.position.z,
                                worldHour);
        }

#ifdef LLM_NPC_TRAIL_DUMP
        // Trail dump for the nearest resident, so the wiring above can be
        // checked by a human before anything depends on the data.
        //
        // COMPILE-GATED, not a config key or a runtime toggle — the same
        // treatment revealKillerForDebug gets, and for a weaker but real
        // version of the same reason: a trail is not the answer, but it is
        // every resident's exact movements, and a host who can read it at
        // will has an advantage no player can see. F9 is unbound; F1 and F2
        // are the only function keys the game uses.
        if (IsKeyPressed(KEY_F9)) {
            const Npc* nearest = nullptr;
            float best = 1e9f;
            for (const Npc& npc : world.npcs()) {
                const float d = distanceXZ(player.position, npc.position());
                if (d < best) {
                    best = d;
                    nearest = &npc;
                }
            }
            if (nearest != nullptr) {
                const auto trail =
                    locationLog.trailOf(nearest->persona().name, 0.0, 24.0);
                std::cerr << "[llm_npc] trail for " << nearest->persona().name
                          << " (" << trail.size() << " stays):\n";
                for (const ZoneVisit& visit : trail) {
                    std::cerr << "    " << zoneName(visit.zoneId) << "  "
                              << visit.startHour << " -> "
                              << (visit.ongoing ? 24.0 : visit.endHour)
                              << (visit.ongoing ? "  (still there)" : "") << "\n";
                }
            }
        }
#endif

        for (auto& callout : callouts) callout.ttl -= dt;
        callouts.erase(std::remove_if(callouts.begin(), callouts.end(),
                                      [](const CombatCallout& c) { return c.ttl <= 0.f; }),
                       callouts.end());
        if (hurtFlash > 0.f) hurtFlash -= dt;

        // ---- multiplayer per-frame traffic ----
        if (netServer) {
            netServer->setHostPose(player.position, player.yawDeg);
            std::vector<NetNpcPose> poses;
            poses.reserve(world.npcs().size());
            for (std::size_t i = 0; i < world.npcs().size(); ++i) {
                const Npc& npc = world.npcs()[i];
                NetNpcPose pose;
                pose.npcIndex = static_cast<int>(i);
                pose.position = npc.position();
                pose.facingDeg = npc.facingDeg();
                pose.mood = static_cast<int>(npc.mood());
                pose.behavior = static_cast<int>(npc.behavior());
                poses.push_back(pose);
            }
            netServer->publishNpcPoses(std::move(poses));
            chatRouter->update();
            remotePlayers = netServer->remotePlayerPoses();
        }
        if (joined) {
            netClient->sendInput(player.position, player.yawDeg);
            for (const auto& msg : netClient->poll()) {
                switch (msg.type) {
                    case MessageType::WorldSnapshot: {
                        remotePlayers.clear();
                        for (const auto& p : msg.payload["players"]) {
                            PlayerPose pose = playerPoseFromJson(p);
                            if (pose.playerId != netClient->playerId()) {
                                remotePlayers.push_back(std::move(pose));
                            }
                        }
                        netNpcs.clear();
                        for (const auto& n : msg.payload["npcs"]) {
                            netNpcs.push_back(netNpcPoseFromJson(n));
                        }
                        break;
                    }
                    case MessageType::ChatDelta:
                        if (session.isOpen() &&
                            msg.payload.value("npc", -1) == session.npcIndex()) {
                            dialog.appendStreamingDelta(msg.payload.value("text", std::string{}));
                        }
                        break;
                    case MessageType::ChatReply:
                        if (session.isOpen() &&
                            msg.payload.value("npc", -1) == session.npcIndex()) {
                            dialog.endStreaming();
                            const std::string name =
                                world.npcs()[static_cast<std::size_t>(session.npcIndex())]
                                    .persona().name;
                            if (msg.payload.value("ok", false)) {
                                const std::string text = msg.payload.value("text", std::string{});
                                if (!text.empty()) {
                                    dialog.appendLine({TranscriptLine::Kind::Npc, name, text});
                                } else {
                                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                                       "(" + name + " says nothing.)"});
                                }
                            } else {
                                dialog.appendLine(
                                    {TranscriptLine::Kind::System, "",
                                     "[They seem distracted: " +
                                         msg.payload.value("error", std::string{}) + "]"});
                            }
                            dialog.setInputEnabled(true);
                            netChatWaiting = false;
                        }
                        break;
                    case MessageType::NpcMoodUpdate: {
                        const int idx = msg.payload.value("npc", -1);
                        if (idx >= 0 && idx < static_cast<int>(netMoods.size())) {
                            netMoods[static_cast<std::size_t>(idx)] = msg.payload.value("mood", 0);
                        }
                        break;
                    }
                    default:
                        break;  // NpcSpeechBubble has no floating-text UI yet
                }
            }
        }

        // Proximity: replicated poses when joined, live world otherwise.
        if (joined) {
            nearbyNpc = -1;
            float best = kTalkRadius;
            for (const auto& npc : netNpcs) {
                const float d = distanceXZ(player.position, npc.position);
                if (d <= best) {
                    best = d;
                    nearbyNpc = npc.npcIndex;
                }
            }
        } else {
            nearbyNpc = world.nearestNpcWithin(player.position, kTalkRadius);
        }

        // ---- LLM traffic (solo/host; remote players' routes go first) ----
        for (const auto& delta : client.drainDeltas()) {
            if (chatRouter && chatRouter->routeDelta(delta)) continue;
            if (session.deltaArrived(delta.id, delta.text)) {
                dialog.appendStreamingDelta(delta.text);
            } else if (group.active() && delta.id == groupPendingId) {
                dialog.appendStreamingDelta(delta.text);
            }
        }
        for (const auto& reply : client.drainReplies()) {
            if (chatRouter && chatRouter->routeReply(reply)) continue;
            // World generation chain (issue #129): validate, retry with
            // the errors, and on success persist + open in the editor.
            if (worldgenRequestId != 0 && reply.id == worldgenRequestId) {
                worldgenRequestId = 0;
                std::string json;
                SandboxMap genMap;
                std::vector<GeneratedCharacter> genCast;
                std::vector<MapError> mapErrors;
                std::vector<CastError> castErrors;
                if (!reply.ok) {
                    worldgenSay("Generation failed: " + reply.errorMessage);
                    worldgenAttempt = 0;
                    continue;
                }
                if (!extractJsonObject(reply.content, json)) {
                    mapErrors.push_back({"output", "no JSON object found"});
                } else if (!parseGeneratedVillage(json, genMap, genCast)) {
                    mapErrors.push_back({"output",
                                         "JSON shape is not {\"map\": {...}, "
                                         "\"characters\": [...]}"});
                } else {
                    mapErrors = validateMap(genMap);
                    castErrors = validateCast(genCast, traitLibrary);
                    const auto links = validateVillageLinks(genMap, genCast);
                    mapErrors.insert(mapErrors.end(), links.begin(), links.end());
                }
                if (!mapErrors.empty() || !castErrors.empty()) {
                    if (worldgenAttempt < kWorldGenMaxAttempts) {
                        ++worldgenAttempt;
                        submitWorldgen(worldgenDescription + "\n\n" +
                                       renderRetryFeedback(mapErrors, castErrors));
                        worldgenSay("Attempt " + std::to_string(worldgenAttempt) +
                                    "/" + std::to_string(kWorldGenMaxAttempts) +
                                    "...");
                    } else {
                        std::string first = mapErrors.empty()
                                                ? castErrors.front().reason
                                                : mapErrors.front().reason;
                        worldgenSay("Generation failed after " +
                                    std::to_string(kWorldGenMaxAttempts) +
                                    " attempts: " + first);
                        worldgenAttempt = 0;
                    }
                    continue;
                }
                // Valid: persist the cast (gen_ ids never spawn in town —
                // spawnTownRoster filters them) and the map, then open it
                // in EDIT mode via the normal path.
                for (const GeneratedCharacter& character : genCast) {
                    const PersonaParseResult parsed =
                        parsePersonaText(character.personaText, "generated");
                    if (!parsed.ok) continue;  // validateCast already passed
                    const std::string genId =
                        generatedCharacterId(parsed.value.persona.name);
                    characterStore.savePersona(genId, character.personaText);
                    if (parsed.value.hasLook) {
                        characterStore.saveLook(genId, parsed.value.look);
                    }
                }
                std::string slug = "gen-1";
                for (int n = 1; fs::exists(mapsDir / (slug + ".json")); ++n) {
                    slug = "gen-" + std::to_string(n);
                }
                std::error_code ec;
                fs::create_directories(mapsDir, ec);
                {
                    std::ofstream out(mapsDir / (slug + ".json"));
                    out << genMap.toJson();
                }
                worldgenSay("Generated '" + genMap.name + "' -> " + slug +
                            " (opening editor)");
                worldgenAttempt = 0;
                sandboxOpenRequested = true;
                sandboxOpenStem = slug;
                continue;
            }
            // Summary notes: remember + persist, never shown in a dialog.
            if (const auto summary = summaryRoutes.find(reply.id);
                summary != summaryRoutes.end()) {
                const int npcIndex = summary->second;
                summaryRoutes.erase(summary);
                if (reply.ok) {
                    Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
                    npc.setMemory(reply.content);
                    if (memoryStore.save(npc.persona().name, npc.memory(), npc.history())) {
                        savedTurns[static_cast<std::size_t>(npcIndex)] = npc.history().size();
                    }
                }  // failure: keep the old note; retry at the next close
                continue;
            }
            // Fact proposals: VALIDATE then COMMIT — a reply that isn't a
            // clean JSON array of well-formed facts commits nothing.
            if (const auto facts = factRoutes.find(reply.id); facts != factRoutes.end()) {
                const int npcIndex = facts->second;
                factRoutes.erase(facts);
                if (reply.ok && npcIndex >= 0 &&
                    npcIndex < static_cast<int>(world.npcs().size())) {
                    Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
                    const auto proposals = validateProposedFacts(reply.content);
                    for (const ProposedFact& proposal : proposals) {
                        const KnownFact record =
                            commitFact(world.state(), proposal, npc.persona().name);
                        factStore.saveFact(record);
                        factStore.saveKnowledge(npc.persona().name, record.factId);
                        if (proposal.playerLearned) {
                            factStore.saveKnowledge("player", record.factId);
                        }
                    }
                    if (!proposals.empty()) refreshGossip(npc);
                }
                continue;
            }
            const auto route = pendingRoutes.find(reply.id);
            if (route == pendingRoutes.end()) continue;
            const int npcIndex = route->second;
            Npc& npc = world.npcs()[static_cast<std::size_t>(npcIndex)];
            const auto text = npc.onReplyArrived(reply);
            pendingRoutes.erase(route);

            // Hosting: our own conversations broadcast the same speech/mood
            // updates remote-initiated ones do.
            if (chatRouter && text) {
                chatRouter->announceNpcSpeech(npcIndex, *text);
                chatRouter->announceNpcMood(npcIndex);
            }

            if (group.active() && reply.id == groupPendingId) {
                groupPendingId = 0;
                // Latency log (issue #124): one model call per turn.
                std::cerr << "[llm_npc] group turn: "
                          << (GetTime() - groupTurnStart) << "s ("
                          << npc.persona().name << ")\n";
                dialog.endStreaming();
                if (text && !text->empty()) {
                    dialog.appendLine(
                        {TranscriptLine::Kind::Npc, npc.persona().name, *text});
                    group.addLine(npc.persona().name, *text);
                } else {
                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                       "(" + npc.persona().name + " says nothing.)"});
                    group.addLine(npc.persona().name, "...");
                }
                const std::string groupSd = stageDirection(npc.lastAction());
                if (!groupSd.empty()) {
                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                       npc.persona().name + " " + groupSd});
                }
                if (npc.lastAction() == NpcAction::CallPolice) {
                    for (Npc& officer : world.npcs()) {
                        if (officer.persona().police) officer.commandArrest();
                    }
                }
                // NPC-to-NPC floor (capped): the next participant answers
                // the line just spoken; otherwise back to the player.
                if (group.npcMayTakeFloor() && group.participants().size() >= 2) {
                    const int next = group.resolveNextSpeaker("");
                    if (next >= 0 && next != npcIndex) {
                        group.noteNpcTurn();
                        submitGroupTurn(next);
                    } else {
                        dialog.setInputEnabled(true);
                    }
                } else {
                    dialog.setInputEnabled(true);
                }
                continue;
            }
            if (session.replyArrived(reply.id, reply.ok)) {
                dialog.endStreaming();
                if (text) {
                    if (!text->empty()) {
                        dialog.appendLine({TranscriptLine::Kind::Npc, npc.persona().name, *text});
                    }
                    const std::string sd = stageDirection(npc.lastAction());
                    if (!sd.empty()) {
                        dialog.appendLine(
                            {TranscriptLine::Kind::System, "", npc.persona().name + " " + sd});
                    } else if (text->empty()) {
                        dialog.appendLine({TranscriptLine::Kind::System, "",
                                           "(" + npc.persona().name + " says nothing.)"});
                    }
                    // A summons mobilizes every officer in the city.
                    if (npc.lastAction() == NpcAction::CallPolice) {
                        for (Npc& officer : world.npcs()) {
                            if (officer.persona().police) officer.commandArrest();
                        }
                    }
                } else {
                    dialog.appendLine({TranscriptLine::Kind::System, "",
                                       "[" + npc.persona().name +
                                           " seems distracted: " + reply.errorMessage + "]"});
                }
                dialog.setInputEnabled(true);
            }
        }

        // ---- render ----
        BeginDrawing();
        renderer.setTimeOfDay(worldHour);  // sky, fog, light: one clock
        ClearBackground(renderer.skyColor());
        const bool sandboxEditing = (mode == AppMode::SandboxEdit);
        const bool cutscenePlaying = (mode == AppMode::Cutscene);
        if (cutscenePlaying) {
            // Authored poses are eye-space; beginFrame adds kEyeHeight, so
            // subtract it and the shot lands where the author framed it.
            const CameraPose shot = cutscene.pose();
            renderer.beginFrame(CameraPose{
                Vec3{shot.position.x, shot.position.y - kEyeHeight, shot.position.z},
                shot.yawDeg, shot.pitchDeg});
        } else if (sandboxEditing) {
            // High tilted vantage over the pan target; beginFrame adds eye
            // height, so hand it the zoom height minus that.
            renderer.beginFrame(CameraPose{
                Vec3{sandboxCam.x, sandboxZoom - kEyeHeight,
                     sandboxCam.z + sandboxZoom * 0.7f},
                180.f, -55.f});
        } else {
            renderer.beginFrame(
                CameraPose{player.position, player.yawDeg, player.pitchDeg});
        }
        renderer.drawCity(world.city());
        if (sandboxEditing) {
            // Placed NPCs render as their real composite looks so the
            // editor shows the cast, not markers.
            for (const PlacedNpc& placed : sandboxDoc.npcs) {
                renderer.drawCompositeCharacter(lookForSource(placed.source),
                                                Vec3{placed.x, 0.f, placed.z},
                                                placed.facingDeg, false, 0.f);
            }
        }
        if (sandboxEditing && sandboxGhostLive && sandboxPlacingNpc) {
            // NPC ghost: the actual look at the cursor plus a validity box.
            renderer.drawCompositeCharacter(
                lookForSource(sandboxGhostNpc.source),
                Vec3{sandboxGhostNpc.x, 0.f, sandboxGhostNpc.z},
                sandboxGhostNpc.facingDeg, false, 0.f);
            Building box;
            box.minX = sandboxGhostNpc.x - 0.6f;
            box.maxX = sandboxGhostNpc.x + 0.6f;
            box.minZ = sandboxGhostNpc.z - 0.6f;
            box.maxZ = sandboxGhostNpc.z + 0.6f;
            box.height = 1.9f;
            renderer.drawPlacementGhost(box, sandboxGhostValid);
        } else if (sandboxEditing && sandboxGhostLive) {
            if (const PieceDef* pd = findPiece(sandboxGhost.pieceId)) {
                Building ghost;
                ghost.id = pd->assetId;  // base id: preview with the real model
                ghost.minX = static_cast<float>(sandboxGhost.tileX) * kMapTileUnits;
                ghost.minZ = static_cast<float>(sandboxGhost.tileZ) * kMapTileUnits;
                ghost.maxX = ghost.minX + static_cast<float>(pd->tilesW) * kMapTileUnits;
                ghost.maxZ = ghost.minZ + static_cast<float>(pd->tilesD) * kMapTileUnits;
                ghost.height = pd->height;
                renderer.drawPlacementGhost(ghost, sandboxGhostValid);
            }
        }
        if (joined) {
            for (const auto& netNpc : netNpcs) {
                const int mood = netNpc.npcIndex < static_cast<int>(netMoods.size())
                                     ? netMoods[static_cast<std::size_t>(netNpc.npcIndex)]
                                     : netNpc.mood;
                const NpcFace face = faceForMood(static_cast<NpcMood>(mood));
                // Clients load the same persona files as the host, so the
                // shared-library look resolves identically by index. An
                // out-of-range index (host has created characters this
                // client's save lacks) falls back to the rigged mesh rather
                // than guessing a look.
                if (netNpc.npcIndex >= 0 &&
                    netNpc.npcIndex < static_cast<int>(npcLooks.size())) {
                    renderer.drawCompositeCharacter(
                        npcLooks[static_cast<std::size_t>(netNpc.npcIndex)],
                        netNpc.position, netNpc.facingDeg, false,
                        static_cast<float>(GetTime()), face);
                } else {
                    CharacterVisual visual;
                    visual.position = netNpc.position;
                    visual.facingDeg = netNpc.facingDeg;
                    visual.variantSeed = netNpc.npcIndex;
                    visual.face = face;
                    renderer.drawCharacter(visual);
                }
            }
        } else {
            for (std::size_t i = 0; i < world.npcs().size(); ++i) {
                const Npc& npc = world.npcs()[i];
                const bool walking = distanceXZ(npc.position(), npcLastPos[i]) > 0.01f;
                npcLastPos[i] = npc.position();
                NpcFace face = faceForMood(npc.mood());
                if (smokeRun) face = static_cast<NpcFace>(i % 6);
                // ONE shared library: every NPC — designer persona or
                // player-created — draws from the same composite parts pool
                // the creator picks from (plan: shared-character-library).
                // A gesture drives the bob so a talking NPC still reads as
                // animated (composites have no gesture clip).
                // Per-NPC clock offset so mesh-family idle/walk cycles
                // don't play in eerie unison across the plaza (#142).
                renderer.drawCompositeCharacter(
                    npcLooks[i], npc.position(), npc.facingDeg(),
                    walking || npc.pose() != NpcAction::None,
                    static_cast<float>(GetTime()) + static_cast<float>(i) * 1.618f,
                    face, npc.combatState() == NpcState::Dead);
            }
        }
        // Fellow players, drawn with the character mesh (id offset picks a
        // variant no NPC uses).
        for (const auto& remote : remotePlayers) {
            CharacterVisual visual;
            visual.position = remote.position;
            visual.facingDeg = remote.facingDeg;
            visual.variantSeed = 1000 + remote.playerId;
            renderer.drawCharacter(visual);
        }
        // Creator preview: the draft look stands a few steps in front of the
        // camera while the Creator page is open (the page's lighter overlay
        // keeps it readable); the player rotates it (see below).
        const bool previewOpen = (mode == AppMode::Menu && menu.creatorPreview());
        if (previewOpen) {
            // On the frame the page opens, start facing the camera rather than
            // an arbitrary angle (still no autonomous spin afterward).
            if (!previewWasOpen) {
                previewYaw = player.yawDeg + 180.f;
                previewIdleSeconds = 0.f;
                previewDragging = false;
            }
            // Two states, no autonomous spin (issue #93):
            //   (1) INPUT — a left-drag over the preview (not over a menu
            //       control) or Left/Right keys rotate the figure and reset
            //       the idle timer.
            //   (2) IDLE RETURN — with no input, only after kPreviewIdleTimeout
            //       does it ease back toward facing the camera. Before that it
            //       simply holds still.
            // (A future opt-in auto-rotate would be a third branch here.)
            //
            // Whether a left-press is a preview drag or a control click is
            // decided ONCE, at the press, and latched until release: a click
            // that starts on a control never rotates even if the cursor slides
            // off, and a drag that starts on empty space keeps rotating even if
            // the cursor crosses a control. Sampling the cursor per-frame
            // instead would let a control-click flicked off its rect spin the
            // figure, and stall a legitimate drag that grazes a hit-rect.
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                previewDragging = !menu.pointOverInteractive(GetMousePosition());
            }
            if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                previewDragging = false;
            }

            float rotateInput = 0.f;
            if (IsKeyDown(KEY_LEFT))  rotateInput -= kPreviewKeyDegPerSec * dt;
            if (IsKeyDown(KEY_RIGHT)) rotateInput += kPreviewKeyDegPerSec * dt;
            if (previewDragging) rotateInput += GetMouseDelta().x * kPreviewDragDegPerPx;

            // A held drag counts as active even while momentarily still, so the
            // figure never eases away from under a grabbing cursor.
            if (rotateInput != 0.f || previewDragging) {
                previewYaw += rotateInput;
                previewIdleSeconds = 0.f;
            } else {
                previewIdleSeconds += dt;
                if (previewIdleSeconds >= kPreviewIdleTimeout) {
                    const float defaultYaw = player.yawDeg + 180.f;  // front to camera
                    previewYaw += shortestAngleDelta(previewYaw, defaultYaw) *
                                  (1.f - std::exp(-kPreviewEaseRate * dt));
                }
            }
            const Vec3 previewAt = player.position + flatForward(player.yawDeg) * 3.4f;
            renderer.drawCompositeCharacter(*menu.creatorPreview(), previewAt,
                                            previewYaw, false, 0.f);
        }
        previewWasOpen = previewOpen;
        // No viewmodel during a cutscene: the camera is not the player's
        // eyes any more, so a floating gun in the corner of an
        // establishing shot reads as a rendering bug.
        if (!joined && mode != AppMode::Dead && !sandboxEditing &&
            !cutscenePlaying) {
            renderer.drawViewmodel(static_cast<int>(world.player().weapon),
                                   world.player().attackAnimFraction);
        }
        renderer.endFrame();

        // ---- 2D overlay ----
        if (worldgenStatusTtl > 0.f) {
            worldgenStatusTtl -= dt;
            drawCenteredHudText(worldgenStatus, 16, 40.f);
        }
        if (mode == AppMode::Dialogue && !joined && session.npcIndex() >= 0 &&
            session.npcIndex() < static_cast<int>(world.npcs().size())) {
            const auto& ratedHistory =
                world.npcs()[static_cast<std::size_t>(session.npcIndex())].history();
            if (!client.busy() && ratedHistory.size() >= 2 &&
                ratedHistory.back().role == "assistant" &&
                !(ratedNpc == session.npcIndex() &&
                  ratedHistorySize == ratedHistory.size())) {
                drawCenteredHudText("[F1] like reply    [F2] flag reply", 16, 12.f);
            }
        }
        if (sandboxEditing) {
            const std::string current =
                sandboxPlacingNpc && !sandboxNpcSources.empty()
                    ? "NPC " + sandboxNpcSources[static_cast<std::size_t>(sandboxNpcIndex)]
                    : "piece " +
                          pieceCatalog()[static_cast<std::size_t>(sandboxPieceIndex)].label;
            drawCenteredHudText(
                "Sandbox '" + sandboxDoc.name + "'   " + current +
                "   Tab pieces/NPCs | [ / ] cycle | click place | right-click delete | "
                "P play | Esc save+exit",
                18, 14.f);
        }
        // Nameplates from whichever pose source is authoritative right now.
        //
        // Suppressed during a cutscene, and not only for looks: the opening
        // scene is required never to identify a living NPC, and a nameplate
        // drifting into an establishing shot would name one outright.
        //
        // Suppressed with a menu open too. Nameplates are drawn before the
        // menu's backdrop, which dims them without hiding them, so they came
        // through the Journal's text as a second layer of words at the same
        // size. Found on the first capture the Journal has ever had (#216) —
        // no test would have shown it, and nobody had looked.
        const bool menuOpen = (mode == AppMode::Menu);
        const auto plateFor = [&](const Vec3& feet, const std::string& name, Color color) {
            if (cutscenePlaying || menuOpen) return;
            if (distanceXZ(player.position, feet) > kNameplateRange) return;
            Vector2 screen;
            if (!renderer.worldToScreen(feet + Vec3{0.f, 2.15f, 0.f}, screen)) return;
            drawNameplate(name, screen, color);
        };
        if (joined) {
            for (const auto& netNpc : netNpcs) {
                if (netNpc.npcIndex < 0 ||
                    netNpc.npcIndex >= static_cast<int>(world.npcs().size())) continue;
                plateFor(netNpc.position,
                         world.npcs()[static_cast<std::size_t>(netNpc.npcIndex)].persona().name,
                         WHITE);
            }
        } else {
            for (const Npc& npc : world.npcs()) {
                // Nameplate carries the schedule activity ("Marge - baking
                // bread") so the routine reads at a glance.
                plateFor(npc.position(),
                         npc.activity().empty()
                             ? npc.persona().name
                             : npc.persona().name + " - " + npc.activity(),
                         WHITE);
            }
        }
        for (const auto& remote : remotePlayers) {
            plateFor(remote.position, remote.name, Color{150, 220, 255, 255});
        }

        // Combat callouts float above their NPC like temporary nameplates.
        // Combat callouts are nameplates by another name and sit in the same
        // layer, so they come through a menu the same way.
        for (const auto& callout : callouts) {
            if (cutscenePlaying || menuOpen) break;
            if (callout.npcIndex < 0 ||
                callout.npcIndex >= static_cast<int>(world.npcs().size())) continue;
            const Npc& npc = world.npcs()[static_cast<std::size_t>(callout.npcIndex)];
            Vector2 screen;
            if (renderer.worldToScreen(npc.position() + Vec3{0.f, 2.6f, 0.f}, screen)) {
                drawNameplate(callout.text, screen, Color{255, 200, 120, 255});
            }
        }

        if (mode == AppMode::Playing) {
            if (nearbyNpc >= 0 && nearbyNpc < static_cast<int>(world.npcs().size())) {
                const Npc& npc = world.npcs()[static_cast<std::size_t>(nearbyNpc)];
                drawCenteredHudText(
                    "[" + bindings.key(Action::Talk) + "] Talk to " + npc.persona().name, 20,
                    static_cast<float>(GetScreenHeight()) - 84.f);
            }
            if (jailSecondsLeft > 0.f) {
                const int secs = static_cast<int>(jailSecondsLeft) + 1;
                drawCenteredHudText("Arrested: disturbing the peace. Released in " +
                                        std::to_string(secs) + "s",
                                    22, static_cast<float>(GetScreenHeight()) - 120.f);
            }
            DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2, 2.f,
                       Color{255, 255, 255, 200});
            // Weapon + health HUD (bottom-left), solo/host only.
            if (!joined) {
                const Player& p = world.player();
                const WeaponDef& def = weaponDef(p.weapon);
                std::string weaponLine = std::string("[1/2] ") + def.name;
                if (def.ammoMax > 0) {
                    const auto it = p.ammo.find(static_cast<std::uint8_t>(p.weapon));
                    weaponLine += "  ammo " +
                                  std::to_string(it != p.ammo.end() ? it->second : 0);
                }
                DrawText(weaponLine.c_str(), 24, GetScreenHeight() - 64, 18, RAYWHITE);
                DrawRectangle(24, GetScreenHeight() - 40, 180, 14, Color{40, 20, 20, 220});
                DrawRectangle(24, GetScreenHeight() - 40,
                              static_cast<int>(180.f * static_cast<float>(p.hp) /
                                               static_cast<float>(p.hpMax)),
                              14, Color{200, 60, 60, 255});
            }
        } else if (mode == AppMode::Dead) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{40, 8, 8, 200});
            const char* headline = "You died.";
            DrawText(headline, (GetScreenWidth() - MeasureText(headline, 44)) / 2,
                     GetScreenHeight() / 2 - 48, 44, RAYWHITE);
            const char* hint = "Press Enter to wake up at the plaza.";
            DrawText(hint, (GetScreenWidth() - MeasureText(hint, 20)) / 2,
                     GetScreenHeight() / 2 + 12, 20, Color{220, 200, 200, 255});
        } else if (mode == AppMode::Dialogue) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{8, 10, 16, 150});
            dialog.render();
        } else if (mode == AppMode::Cutscene) {
            // Letterbox, fade and caption — the whole visual vocabulary. There
            // is no post-processing chain, so a fade is a rectangle with alpha
            // and bars are two more. Deliberately: dissolves and colour grading
            // are out of scope and adding them means a renderer change, which
            // this feature is specifically built to avoid.
            const int w = GetScreenWidth();
            const int h = GetScreenHeight();
            const int bars = cutscene.letterboxPx();
            if (bars > 0) {
                DrawRectangle(0, 0, w, bars, BLACK);
                DrawRectangle(0, h - bars, w, bars, BLACK);
            }
            const float alpha = cutscene.fadeAlpha();
            if (alpha > 0.f) {
                // Named colours only; a cutscene file is content, not code, and
                // an unknown name falling back to black is the safe default.
                const std::string& tint = cutscene.fadeColour();
                Color fade = BLACK;
                if (tint == "white") fade = RAYWHITE;
                else if (tint == "grey" || tint == "gray") fade = Color{90, 90, 96, 255};
                fade.a = static_cast<unsigned char>(255.f * alpha);
                DrawRectangle(0, 0, w, h, fade);
            }
            const std::string& line = cutscene.caption();
            if (!line.empty()) {
                // Size 20 off the usable ladder: the built-in bitmap font
                // computes glyph spacing with integer division, so 14 through
                // 18 space identically and only ~10 / 20 / 30 visibly differ.
                const int size = 20;
                const int tw = MeasureText(line.c_str(), size);
                DrawText(line.c_str(), (w - tw) / 2, h - bars - size - 18, size,
                         RAYWHITE);
            }
            if (cutscene.canSkip()) {
                DrawText("Space to skip", 24, h - bars - 26, 10,
                         Color{200, 200, 200, 180});
            }
        } else if (mode == AppMode::SandboxEdit) {
            // Draw NOTHING here, and that is the whole fix.
            //
            // SandboxEdit had no branch in this chain, so it fell through to
            // the final else and ran menu.render() — which opens with a
            // full-screen dim rect at alpha 170 plus a page title and has no
            // early-out. The entire paused menu rendered over the editor every
            // frame. You could still pan and place pieces behind it, which is
            // why it read as "the map won't go away" rather than as a stuck
            // menu.
            //
            // The editor's own HUD is already drawn further up, with the other
            // world-space overlays (search `if (sandboxEditing)` above the
            // nameplates). It does not belong here.
        } else {
            menu.render();
        }

        if (hurtFlash > 0.f) {
            const auto alpha = static_cast<unsigned char>(180.f * (hurtFlash / 0.35f));
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          Color{200, 30, 30, alpha});
        }
        if (!assets.loaded()) {
            DrawText("Asset packs missing - run tools/fetch_assets.sh for the full look", 24,
                     GetScreenHeight() - 40, 18, Color{255, 225, 130, 255});
        }
        // AFTER EndDrawing, not before. raylib batches 2D draw calls and
        // flushes them at EndDrawing; TakeScreenshot reads the framebuffer
        // directly. Called before the flush it captures the 3D scene and NONE
        // of the pending UI — so every HUD, menu and overlay was silently
        // missing from every capture, and visual QA of anything 2D was blind.
        EndDrawing();
        if (screenshotPath && maxFrames >= 0 && frames >= maxFrames) {
            TakeScreenshot(screenshotPath);
        }
    }

shutdown:
    // Transcripts with unsaved turns persist on quit; their summary refresh
    // happens at the next conversation close (summarizing here would block
    // shutdown on the LLM).
    for (std::size_t i = 0; i < world.npcs().size(); ++i) {
        const Npc& npc = world.npcs()[i];
        if (npc.history().size() > savedTurns[i]) {
            memoryStore.save(npc.persona().name, npc.memory(), npc.history());
        }
    }

    leaveSession();
    CloseWindow();
    return 0;
}
