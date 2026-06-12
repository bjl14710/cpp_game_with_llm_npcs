cmake_minimum_required(VERSION 3.20)
project(cpp_game_with_llm_npcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)
find_package(Threads REQUIRED)

add_executable(cpp_game_with_llm_npcs
    src/main.cpp
    src/core/LlmClient.cpp
    src/Npc.cpp
    src/DialogUI.cpp
)

target_include_directories(cpp_game_with_llm_npcs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

target_link_libraries(cpp_game_with_llm_npcs
    PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        Threads::Threads
)
