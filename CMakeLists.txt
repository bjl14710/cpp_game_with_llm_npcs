cmake_minimum_required(VERSION 3.20)
# C is for the vendored SQLite amalgamation (external/sqlite3.c).
project(cpp_game_with_llm_npcs LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Deployment floor, and the reason it is written down rather than assumed.
#
# Plan: .claude/plans/cpp20-upgrade.md.
#
# Two C++20 library features are gated behind Apple availability annotations
# and are a COMPILE ERROR below macOS 13.3, not a graceful fallback:
#
#     std::format / std::format_to
#     std::to_chars / std::from_chars for float and double
#
# Neither is adopted here. std::format measured 1.7-2.3x SLOWER than the
# "a" + std::to_string(x) concatenation it would replace (Apple's libc++ ships
# an unoptimised <format>), and the whole string-formatting budget is ~0.014 ms
# of a frame, so there is nothing to buy with the portability.
#
# Without an explicit target CMake defaults to the host SDK, which means a
# std::format call added on a machine running 14.x compiles silently and fails
# only for somebody on 12.x — with no signal until they try. Setting the floor
# turns that into a build error naming the version.
#
# 11.0 is the first Apple-silicon release, so it costs nothing on any arm64
# machine while keeping a wide Intel range. Everything else in C++20 that this
# project uses — starts_with/ends_with, map::contains, designated initialisers,
# <span>, <ranges>, std::ssize, heterogeneous lookup, and INTEGER to_chars —
# is verified clean all the way down to 10.15.
#
# scripts/check-portability-floor.sh asserts this is still true, and
# `make -C tests portability` runs it.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
        "Minimum macOS. Keeps std::format and float to_chars unavailable on purpose." FORCE)
endif()

# Default to an OPTIMISED build when the caller does not choose one.
#
# There was no default, and CMake's own default is the empty string, which
# means no -O flag at all. So `cmake -S . -B build && cmake --build build` —
# exactly what tools/bench_npc_render.py's error message tells you to run —
# produced an unoptimised binary, and every render performance number this
# project has recorded was measured on one. The twenty-one-residents milestone
# is scoped against 26.4 ms/frame taken that way; the same commit at -O2
# measures ~17 ms.
#
# RelWithDebInfo rather than Release: symbols are worth keeping for a game that
# is still being debugged daily, and the optimisation level is the part that
# was missing. A caller who wants something else still passes
# -DCMAKE_BUILD_TYPE and this does nothing.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
    message(STATUS "No CMAKE_BUILD_TYPE set — defaulting to RelWithDebInfo")
endif()

# Frame-budget instrumentation (issue #257). OFF by default and compiles to
# nothing when off — src/profiling/scope_timer.h expands every macro to
# ((void)0), so a release build carries no timer, no atomic and no allocation.
#
# Deliberately not Tracy. Tracy is the better profiler, and it is also a new
# network-fetched dependency; CLAUDE.md says to ask first. The chrono fallback
# answers #257 with nothing new in the build.
#
#   cmake -S . -B build-prof -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_PROFILING=ON
option(ENABLE_PROFILING "Scoped frame-budget timers (issue #257)" OFF)

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
    src/core/Cutscene.cpp
    src/core/DialogueSession.cpp
    src/core/FactStore.cpp
    src/core/Gossip.cpp
    src/core/GroupSession.cpp
    src/core/HostChatRouter.cpp
    src/core/KeyBindings.cpp
    src/core/LineBank.cpp
    src/core/LocationLog.cpp
    src/core/MatchClock.cpp
    src/core/Montage.cpp
    src/core/Mystery.cpp
    src/core/PieceCatalog.cpp
    src/core/RatingLog.cpp
    src/core/Role.cpp
    src/core/SandboxMap.cpp
    src/core/Storyline.cpp
    src/core/TopicMatcher.cpp
    src/core/Trait.cpp
    src/core/Vote.cpp
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
if(ENABLE_PROFILING)
    target_compile_definitions(cpp_game_with_llm_npcs PRIVATE ENABLE_PROFILING)
endif()

# Prints a persona's exact system prompt — feeds the model benchmark
# (tools/bench_npc_models.py) from the same code path the game uses.
add_executable(persona_prompt tools/persona_prompt.cpp)
target_link_libraries(persona_prompt PRIVATE llm_npc_core)

add_executable(worldgen_cli tools/worldgen_cli.cpp)
target_link_libraries(worldgen_cli PRIVATE llm_npc_core)

add_executable(bank_probe tools/bank_probe.cpp)
target_link_libraries(bank_probe PRIVATE llm_npc_core)
