cmake_minimum_required(VERSION 3.20)
project(cpp_game_with_llm_npcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)
find_package(Threads REQUIRED)

# Core: SFML-free game logic shared with the unit tests.
add_library(llm_npc_core STATIC
    src/core/City.cpp
    src/core/Config.cpp
    src/core/LlmClient.cpp
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

add_executable(cpp_game_with_llm_npcs
    src/main.cpp
    src/DialogUI.cpp
)

target_include_directories(cpp_game_with_llm_npcs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(cpp_game_with_llm_npcs
    PRIVATE
        llm_npc_core
        sfml-graphics
        sfml-window
        sfml-system
)
