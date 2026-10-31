$ErrorActionPreference = 'Stop'

# Windows launcher: matches the style of the shooter/racing games. Uses MSYS2
# UCRT64 for SFML, and verifies Ollama is reachable before building.

$ucrtBin = 'C:\msys64\ucrt64\bin'
if (!(Test-Path $ucrtBin)) {
    throw "MSYS2 ucrt64 toolchain not found at $ucrtBin"
}
$env:Path = "$ucrtBin;$env:Path"

$ollamaHost = if ($env:OLLAMA_HOST) { $env:OLLAMA_HOST } else { 'localhost' }
$ollamaPort = if ($env:OLLAMA_PORT) { $env:OLLAMA_PORT } else { '11434' }

try {
    Invoke-WebRequest -Uri "http://${ollamaHost}:${ollamaPort}/api/tags" `
        -UseBasicParsing -TimeoutSec 2 | Out-Null
} catch {
    Write-Error "Ollama not reachable at ${ollamaHost}:${ollamaPort}. Start it with 'ollama serve' (and 'ollama pull qwen2.5:3b-instruct')."
    exit 1
}

cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j

& ".\build\cpp_game_with_llm_npcs.exe"
