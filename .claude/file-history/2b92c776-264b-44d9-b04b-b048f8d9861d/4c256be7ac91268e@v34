// Game entry point on raylib — full parity with the SFML loop it replaced
// (that version remains at: git show feature/raylib-scaffold:src/app/main.cpp).
// Modes: Playing / Dialogue / Menu. Solo simulates locally; hosting shares
// this world through NetServer + HostChatRouter; joining renders the host's
// snapshots and routes chat through NetClient.
#include "raylib.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
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
#include "DialogUI.hpp"
#include "FactStore.hpp"
#include "Gossip.hpp"
#include "Journal.hpp"
#include "DialogueSession.hpp"
#include "HostChatRouter.hpp"
#include "InputMap.hpp"
#include "KeyBindings.hpp"
#include "LlmClient.hpp"
#include "Math.hpp"
#include "Menu.hpp"
#include "NetClient.hpp"
#include "NetServer.hpp"
#include "Npc.hpp"
#include "PersonaLoader.hpp"
#include "RaylibRenderer.hpp"
#include "Weapon.hpp"
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
enum class AppMode { Playing, Dialogue, Menu, Dead };

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
    // --frames N [shot.png] [--camera x z yaw] [--hour H]: render N frames
    // then exit 0, optionally saving a screenshot of the last frame
    // (scripted smoke runs + visual checks). --camera overrides the default
    // plaza vantage; --hour pins the world clock so day/night-dependent
    // screenshots stay deterministic.
    long maxFrames = -1;
    const char* screenshotPath = nullptr;
    bool cameraOverride = false;
    float cameraX = 0.f, cameraZ = 0.f, cameraYaw = 180.f;
    float hourOverride = -1.f;
    if (argc >= 3 && std::strcmp(argv[1], "--frames") == 0) {
        maxFrames = std::strtol(argv[2], nullptr, 10);
        int arg = 3;
        if (arg < argc && argv[arg][0] != '-') {
            screenshotPath = argv[arg++];
        }
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
    LlmClient client(loadLlmConfig(configDir));
    client.warmUp();  // preload the model so the first reply starts fast
    World world(City::makeDowntown());
    // Smoke runs pin the clock so day/night screenshots are deterministic.
    if (hourOverride >= 0.f) world.state().setTimeOfDayHours(hourOverride);
    std::vector<std::string> personaErrors;
    const auto roster = loadAllPersonas(projectRoot / "personas", &personaErrors);
    for (const auto& err : personaErrors) std::cerr << "[llm_npc] persona error: " << err << "\n";
    // ONE look per NPC, index-aligned with world.npcs(). Every NPC —
    // designer persona or player-created — draws from the same shared
    // composite parts pool the creator picks from (plan:
    // shared-character-library); the rigged pack models are no longer an
    // NPC source. Looks come from the persona's authored `look =` line,
    // or the deterministic per-name fallback when absent/stale.
    std::vector<CharacterLook> npcLooks;
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
    // record goes through the SAME parser as designer .persona files; the
    // stored look joins npcLooks like everyone else's. Bad rows were
    // already skipped by loadAll.
    CharacterStore characterStore(projectRoot / "saves" / "characters.sqlite3");
    for (const StoredCharacter& stored : characterStore.loadAll()) {
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
    std::cerr << "[llm_npc] loaded " << world.npcs().size() << " NPCs ("
              << world.npcs().size() - roster.size() << " player-created)\n";

    // Cross-session NPC memory: summaries persist in saves/ and are injected
    // into each NPC's system prompt (plan: npc-memory-and-model).
    ConversationStore memoryStore(projectRoot / "saves" / "conversations.sqlite3");
    for (Npc& npc : world.npcs()) {
        const NpcMemory memory = memoryStore.load(npc.persona().name);
        if (!memory.summary.empty()) npc.setMemory(memory.summary);
    }
    // History length at the last save, per NPC — new turns above this mark
    // trigger a summarization request when the conversation closes.
    std::vector<std::size_t> savedTurns(world.npcs().size(), 0);
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
    for (Npc& npc : world.npcs()) refreshGossip(npc);

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
    std::vector<bool> wasCaught(world.npcs().size(), false);
    // Previous frame's NPC positions, for walk-animation detection.
    std::vector<Vec3> npcLastPos(world.npcs().size());
    for (std::size_t i = 0; i < world.npcs().size(); ++i) {
        npcLastPos[i] = world.npcs()[i].position();
    }

    // Character creator: persists BOTH records (independently) and spawns
    // the new citizen immediately. Declared after the per-NPC bookkeeping
    // vectors because a runtime spawn must grow them all.
    Menu::CreatorHooks creatorHooks;
    creatorHooks.onCreate = [&](const std::string& name, const std::string& backstory,
                                const std::string& traits,
                                const CharacterLook& look) -> std::string {
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
        if (mode == AppMode::Playing) {
            if (IsWindowFocused() && !smokeRun) applyMouseLook(player);

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
            if (IsKeyPressed(KEY_ESCAPE)) {
                // Leaving a conversation kicks off the NPC's memory update
                // and fact extraction (solo/host only — a guest's
                // conversations live host-side).
                if (!joined) {
                    requestSummary(session.npcIndex());
                    requestFacts(session.npcIndex());
                }
                session.close();
                dialog.endStreaming();
                mode = AppMode::Playing;
                DisableCursor();
            } else {
                const std::string submitted = dialog.pollInput();
                if (!submitted.empty() && session.isOpen()) {
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
            }
        }
        for (const auto& reply : client.drainReplies()) {
            if (chatRouter && chatRouter->routeReply(reply)) continue;
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
        renderer.beginFrame(CameraPose{player.position, player.yawDeg, player.pitchDeg});
        renderer.drawCity(world.city());
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
                renderer.drawCompositeCharacter(
                    npcLooks[i], npc.position(), npc.facingDeg(),
                    walking || npc.pose() != NpcAction::None,
                    static_cast<float>(GetTime()), face,
                    npc.combatState() == NpcState::Dead);
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
        if (!joined && mode != AppMode::Dead) {
            renderer.drawViewmodel(static_cast<int>(world.player().weapon),
                                   world.player().attackAnimFraction);
        }
        renderer.endFrame();

        // ---- 2D overlay ----
        // Nameplates from whichever pose source is authoritative right now.
        const auto plateFor = [&](const Vec3& feet, const std::string& name, Color color) {
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
        for (const auto& callout : callouts) {
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
        if (screenshotPath && maxFrames >= 0 && frames >= maxFrames) {
            TakeScreenshot(screenshotPath);
        }
        EndDrawing();
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
