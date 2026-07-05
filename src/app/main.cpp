// Game entry point — raylib migration step 1 of 8 (issue #35).
//
// This intentionally minimal loop proves the raylib toolchain end to end:
// FetchContent build, window creation, vsync, HiDPI, and frame pacing. The
// game itself (modes, world, dialogue, multiplayer) returns in the following
// issues as each subsystem's raylib port lands. The full SFML game loop this
// replaces lives in git history: `git show feature/raylib-scaffold:src/app/main.cpp`.
// See .claude/plans/raylib-visual-overhaul.md and the raylib-migration skill.
#include "raylib.h"

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    // --frames N: render N frames then exit 0. Lets scripts and the
    // overnight harness smoke-test the build without a human closing the
    // window (and without killing the process mid-frame).
    long maxFrames = -1;
    if (argc >= 3 && std::strcmp(argv[1], "--frames") == 0) {
        maxFrames = std::strtol(argv[2], nullptr, 10);
    }

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "LLM NPC City");
    SetTargetFPS(60);

    // The city's sky color — the first thing every later issue draws over.
    const Color sky{135, 190, 235, 255};

    long frames = 0;
    while (!WindowShouldClose() && (maxFrames < 0 || frames++ < maxFrames)) {
        BeginDrawing();
        ClearBackground(sky);
        DrawText("LLM NPC City — raylib migration step 1/8", 24, 24, 20, DARKBLUE);
        DrawText("Gameplay returns in the next issues.", 24, 52, 20, DARKBLUE);
        DrawFPS(24, 92);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
