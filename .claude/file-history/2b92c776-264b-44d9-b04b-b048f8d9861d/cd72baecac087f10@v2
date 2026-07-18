#!/usr/bin/env bash
# Downloads the CC0 asset packs the raylib renderer uses into assets/.
# Idempotent: skips anything already present. Re-run after a clean checkout
# (assets/models and assets/fonts are gitignored — see assets/LICENSES.md
# for what each pack is and its license statement).
#
# Plan: .claude/plans/raylib-visual-overhaul.md (step 3).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODELS="$ROOT/assets/models"
FONTS="$ROOT/assets/fonts"
mkdir -p "$MODELS" "$FONTS"

# TODO(implement, step 3): pin exact pack versions + URLs when choosing the
# final packs in-scene, and record a sha256 per archive. Candidate sources
# (all CC0 — verify the license file inside each download):
#   Kenney City Kit (Commercial / Suburban / Roads)  https://kenney.nl/assets
#   KayKit City Builder Bits + Adventurers characters https://kaylousberg.itch.io
#   Quaternius Ultimate Modular Buildings / Characters https://quaternius.com
#   Kenney fonts (UI font)                            https://kenney.nl/assets/kenney-fonts
#
# Shape of each entry once pinned:
#   fetch "kenney-city-kit-commercial" \
#         "https://kenney.nl/media/pages/assets/city-kit-commercial/<hash>/kenney_city-kit-commercial.zip" \
#         "<sha256>" "$MODELS/city-commercial"
fetch() {
    local name="$1" url="$2" sha="$3" dest="$4"
    if [ -d "$dest" ] && [ -n "$(ls -A "$dest" 2>/dev/null)" ]; then
        echo "[fetch_assets] $name already present — skipping"
        return
    fi
    echo "[fetch_assets] downloading $name"
    local tmp
    tmp="$(mktemp -d)"
    curl -fL --retry 3 -o "$tmp/pack.zip" "$url" || {
        echo "[fetch_assets] FAILED: $name — download manually from $url into $dest" >&2
        exit 1
    }
    echo "$sha  $tmp/pack.zip" | shasum -a 256 -c - || {
        echo "[fetch_assets] FAILED: $name checksum mismatch — the upstream file changed; re-pin" >&2
        exit 1
    }
    mkdir -p "$dest"
    unzip -q "$tmp/pack.zip" -d "$dest"
    rm -rf "$tmp"
}

echo "[fetch_assets] no packs pinned yet — pin them during plan step 3." >&2
echo "[fetch_assets] the game falls back to primitive shapes until then." >&2
