#!/usr/bin/env bash
set -euo pipefail

# One-click launcher (macOS/Linux): brings the whole stack up. For the local
# provider it starts Ollama if it isn't already running and pulls the configured
# model if it's missing, then builds and runs the game. Cloud providers skip
# Ollama entirely. Run from anywhere; this script cds to its own directory.
# raylib is fetched and built by CMake automatically — no graphics packages
# to install.
cd "$(dirname "$0")"

# Read one "key = value" setting from config/llm.cfg (comments stripped),
# falling back to the given default. Mirrors readKv() in src/core/Config.cpp.
cfg_get() {
    local key="$1" default="$2" value
    value="$(sed -n "s/^[[:space:]]*${key}[[:space:]]*=[[:space:]]*//p" config/llm.cfg 2>/dev/null \
        | sed 's/#.*//; s/[[:space:]]*$//' | head -n1)"
    [[ -n "$value" ]] && echo "$value" || echo "$default"
}

PROVIDER="$(cfg_get provider ollama)"
HOST="${OLLAMA_HOST:-$(cfg_get host localhost)}"
PORT="${OLLAMA_PORT:-$(cfg_get port 11434)}"
MODEL="$(cfg_get model qwen2.5:3b-instruct)"

# Local provider only: make sure Ollama is serving and the model is present.
if [[ "$PROVIDER" == "ollama" ]]; then
    if ! curl -fsS --max-time 2 "http://${HOST}:${PORT}/api/tags" >/dev/null 2>&1; then
        if ! command -v ollama >/dev/null 2>&1; then
            echo "Ollama is not installed. Get it from https://ollama.com/download" >&2
            exit 1
        fi
        echo "Starting Ollama..."
        ollama serve >/tmp/llm_npc_ollama.log 2>&1 &
        for _ in $(seq 1 20); do
            if curl -fsS --max-time 1 "http://${HOST}:${PORT}/api/tags" >/dev/null 2>&1; then
                break
            fi
            sleep 1
        done
        if ! curl -fsS --max-time 2 "http://${HOST}:${PORT}/api/tags" >/dev/null 2>&1; then
            echo "Ollama did not come up; see /tmp/llm_npc_ollama.log" >&2
            exit 1
        fi
    fi

    # Pull the configured model on first run (no-op if already present).
    if command -v ollama >/dev/null 2>&1 && ! ollama list 2>/dev/null | awk 'NR>1{print $1}' | grep -qx "$MODEL"; then
        echo "Pulling model ${MODEL} (first run only)..."
        ollama pull "$MODEL"
    fi
fi

cmake -S . -B build
cmake --build build -j

exec ./build/cpp_game_with_llm_npcs
