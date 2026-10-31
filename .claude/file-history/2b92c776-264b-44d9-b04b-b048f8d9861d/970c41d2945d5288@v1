@echo off
REM Windows launcher: double-click to build and play. Mirrors run.ps1 — uses
REM the MSYS2 UCRT64 toolchain for SFML and checks Ollama is running first.
setlocal
cd /d "%~dp0"

set "UCRT_BIN=C:\msys64\ucrt64\bin"
if not exist "%UCRT_BIN%" (
    echo MSYS2 ucrt64 toolchain not found at %UCRT_BIN%.
    echo Install MSYS2 from https://www.msys2.org and the SFML/cmake packages.
    pause
    exit /b 1
)
set "PATH=%UCRT_BIN%;%PATH%"

if "%OLLAMA_HOST%"=="" set "OLLAMA_HOST=localhost"
if "%OLLAMA_PORT%"=="" set "OLLAMA_PORT=11434"

curl -fsS --max-time 2 "http://%OLLAMA_HOST%:%OLLAMA_PORT%/api/tags" >nul 2>&1
if errorlevel 1 (
    echo Ollama is not reachable at %OLLAMA_HOST%:%OLLAMA_PORT%.
    echo Start it with:  ollama serve
    echo And make sure the model is pulled:  ollama pull qwen2.5:3b-instruct
    pause
    exit /b 1
)

cmake -S . -B build -G "MinGW Makefiles"
if errorlevel 1 (
    echo CMake configure failed.
    pause
    exit /b 1
)
cmake --build build -j
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

build\cpp_game_with_llm_npcs.exe
endlocal
