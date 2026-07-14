$ErrorActionPreference = 'Stop'

# One-click launcher (Windows): uses the MSYS2 UCRT64 toolchain for SFML, and
# for the local provider starts Ollama if needed and pulls the configured model
# before building and running.

$ucrtBin = 'C:\msys64\ucrt64\bin'
if (!(Test-Path $ucrtBin)) {
    throw "MSYS2 ucrt64 toolchain not found at $ucrtBin"
}
$env:Path = "$ucrtBin;$env:Path"

# Read one "key = value" setting from config/llm.cfg (comments stripped).
function Get-Cfg($key, $default) {
    $line = Select-String -Path 'config/llm.cfg' -Pattern "^\s*$key\s*=" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $line) { return $default }
    $val = ($line.Line -replace "^\s*$key\s*=\s*", '') -replace '#.*', ''
    $val = $val.Trim()
    if ($val) { return $val } else { return $default }
}

$provider   = Get-Cfg 'provider' 'ollama'
$ollamaHost = if ($env:OLLAMA_HOST) { $env:OLLAMA_HOST } else { Get-Cfg 'host' 'localhost' }
$ollamaPort = if ($env:OLLAMA_PORT) { $env:OLLAMA_PORT } else { Get-Cfg 'port' '11434' }
$model      = Get-Cfg 'model' 'qwen2.5:3b-instruct'

function Test-Ollama {
    try {
        Invoke-WebRequest -Uri "http://${ollamaHost}:${ollamaPort}/api/tags" `
            -UseBasicParsing -TimeoutSec 2 | Out-Null
        return $true
    } catch { return $false }
}

if ($provider -eq 'ollama') {
    if (-not (Test-Ollama)) {
        if (-not (Get-Command ollama -ErrorAction SilentlyContinue)) {
            throw "Ollama is not installed. Get it from https://ollama.com/download"
        }
        Write-Host 'Starting Ollama...'
        Start-Process -FilePath 'ollama' -ArgumentList 'serve' -WindowStyle Hidden
        for ($i = 0; $i -lt 20 -and -not (Test-Ollama); $i++) { Start-Sleep -Seconds 1 }
        if (-not (Test-Ollama)) { throw "Ollama did not come up at ${ollamaHost}:${ollamaPort}." }
    }

    # Pull the configured model on first run (no-op if already present).
    $installed = (& ollama list) 2>$null | Select-Object -Skip 1 | ForEach-Object { ($_ -split '\s+')[0] }
    if ($installed -notcontains $model) {
        Write-Host "Pulling model $model (first run only)..."
        & ollama pull $model
    }
}

cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j

& ".\build\cpp_game_with_llm_npcs.exe"
