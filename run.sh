#!/usr/bin/env bash
set -euo pipefail

# Linux launcher: verifies Ollama is reachable, builds, and runs the game.
# Run from anywhere; this script cds to its own directory.
cd "$(dirname "$0")"

HOST="${OLLAMA_HOST:-localhost}"
PORT="${OLLAMA_PORT:-11434}"

if ! curl -fsS --max-time 2 "http://${HOST}:${PORT}/api/tags" >/dev/null; then
    echo "Ollama not reachable at ${HOST}:${PORT}." >&2
    echo "Start it with:  ollama serve   (and pull a model: ollama pull qwen2.5:3b-instruct)" >&2
    exit 1
fi

# On macOS, sfml@2 is keg-only and not in the default prefix.
CMAKE_PREFIX=""
if [[ -d /opt/homebrew/opt/sfml@2 ]]; then
    CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/sfml@2"
elif [[ -d /usr/local/opt/sfml@2 ]]; then
    CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=/usr/local/opt/sfml@2"
fi

cmake -S . -B build $CMAKE_PREFIX
cmake --build build -j

exec ./build/cpp_game_with_llm_npcs
