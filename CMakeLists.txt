cmake_minimum_required(VERSION 3.20)
project(cpp_game_with_llm_npcs LANGUAGES CXX)

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
    src/core/City.cpp
    src/core/Config.cpp
    src/core/DialogueSession.cpp
    src/core/HostChatRouter.cpp
    src/core/KeyBindings.cpp
    src/core/LlmClient.cpp
    src/core/NetClient.cpp
    src/core/NetFraming.cpp
    src/core/NetMessage.cpp
    src/core/NetServer.cpp
    src/core/Npc.cpp
    src/core/PersonaLoader.cpp
    src/core/World.cpp
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

# Game executable. Migration in progress (issues #35-#42): DialogUI, Menu,
# and InputMap rejoin the target as their SFML draw layers are ported.
add_executable(cpp_game_with_llm_npcs
    src/app/main.cpp
    src/app/Assets.cpp
    src/app/FaceTexture.cpp
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
