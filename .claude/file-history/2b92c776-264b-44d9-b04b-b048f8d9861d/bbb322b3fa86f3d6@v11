cmake_minimum_required(VERSION 3.20)
project(cpp_game_with_llm_npcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
# Game executable needs SFML+OpenGL; QUIET so the dev container can still
# configure CMake and build the core library + tests without them.
find_package(SFML 2.6 COMPONENTS graphics window system QUIET)
find_package(OpenGL QUIET)

# ---- raylib migration (plan: .claude/plans/raylib-visual-overhaul.md) ----
# Step 1 enables this block, REPLACING the SFML/OpenGL find_packages above.
# The app target then swaps Renderer3D.cpp for RaylibRenderer.cpp,
# Assets.cpp, and FaceTexture.cpp (skeletons already in src/app/), links
# `raylib` instead of the sfml-*/OpenGL::GL set, and drops the ApplicationServices
# framework (raylib owns relative mouse handling).
#
# include(FetchContent)
# FetchContent_Declare(raylib
#     GIT_REPOSITORY https://github.com/raysan5/raylib.git
#     GIT_TAG        5.5   # TODO(step 1): confirm/pin the latest stable tag
# )
# set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(raylib)
# --------------------------------------------------------------------------

# Core: SFML-free game logic shared with the unit tests.
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

if(SFML_FOUND AND OPENGL_FOUND)
    add_executable(cpp_game_with_llm_npcs
        src/app/main.cpp
        src/app/DialogUI.cpp
        src/app/InputMap.cpp
        src/app/Menu.cpp
        src/app/Renderer3D.cpp
    )
    target_include_directories(cpp_game_with_llm_npcs PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    )
    target_link_libraries(cpp_game_with_llm_npcs
        PRIVATE
            llm_npc_core
            sfml-graphics
            sfml-window
            sfml-system
            OpenGL::GL
    )
    if(APPLE)
        # Mouse-look uses CoreGraphics cursor warping directly; see main.cpp.
        target_link_libraries(cpp_game_with_llm_npcs PRIVATE
            "-framework ApplicationServices")
    endif()
else()
    message(STATUS "SFML or OpenGL not found; building core library only "
                   "(use tests/Makefile for the test suite)")
endif()
