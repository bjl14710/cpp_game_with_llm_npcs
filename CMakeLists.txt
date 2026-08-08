cmake_minimum_required(VERSION 3.20)
# C is for the vendored SQLite amalgamation (external/sqlite3.c).
project(cpp_game_with_llm_npcs LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# raylib provides windowing, input, 3D rendering, and the 2D UI overlay
# (plan: .claude/plans/raylib-visual-overhaul.md). Fetched and pinned so a
# clean clone builds with no system packages; 5.5 is pinned deliberately —
# 6.0 redesigned the skeletal animation API the character rendering uses.
include(FetchContent)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Core: rendering-free game logic shared with the unit tests.
add_library(llm_npc_core STATIC
    external/sqlite3.c
    src/core/CharacterParts.cpp
    src/core/CharacterStore.cpp
    src/core/City.cpp
    src/core/Config.cpp
    src/core/ConversationStore.cpp
    src/core/DialogueSession.cpp
    src/core/FactStore.cpp
    src/core/Gossip.cpp
    src/core/GroupSession.cpp
    src/core/HostChatRouter.cpp
    src/core/KeyBindings.cpp
    src/core/LineBank.cpp
    src/core/LocationLog.cpp
    src/core/MatchClock.cpp
    src/core/Mystery.cpp
    src/core/PieceCatalog.cpp
    src/core/RatingLog.cpp
    src/core/SandboxMap.cpp
    src/core/TopicMatcher.cpp
    src/core/Trait.cpp
    src/core/WorldGen.cpp
    src/core/WorldGenValidate.cpp
    src/core/Zones.cpp
    src/core/LlmBackend.cpp
    src/core/LlmClient.cpp
    src/core/OllamaBackend.cpp
    src/core/OpenAiBackend.cpp
    src/core/NetClient.cpp
    src/core/NetFraming.cpp
    src/core/NetMessage.cpp
    src/core/NetServer.cpp
    src/core/Npc.cpp
    src/core/PersonaLoader.cpp
    src/core/World.cpp
    src/core/WorldState.cpp
)
target_include_directories(llm_npc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)
target_link_libraries(llm_npc_core PUBLIC Threads::Threads)
if(WIN32)
    # httplib needs winsock when built with MinGW.
    target_link_libraries(llm_npc_core PUBLIC ws2_32)
endif()

# Game executable (raylib renderer — replaced the original SFML+legacy-GL
# app across issues #35-#42).
add_executable(cpp_game_with_llm_npcs
    src/app/main.cpp
    src/app/Assets.cpp
    src/app/DialogUI.cpp
    src/app/FaceTexture.cpp
    src/app/InputMap.cpp
    src/app/Menu.cpp
    src/app/RaylibRenderer.cpp
)
target_include_directories(cpp_game_with_llm_npcs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
)
target_link_libraries(cpp_game_with_llm_npcs
    PRIVATE
        llm_npc_core
        raylib
)

# Prints a persona's exact system prompt — feeds the model benchmark
# (tools/bench_npc_models.py) from the same code path the game uses.
add_executable(persona_prompt tools/persona_prompt.cpp)
target_link_libraries(persona_prompt PRIVATE llm_npc_core)

add_executable(worldgen_cli tools/worldgen_cli.cpp)
target_link_libraries(worldgen_cli PRIVATE llm_npc_core)
