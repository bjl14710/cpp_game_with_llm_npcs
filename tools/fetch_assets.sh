#!/usr/bin/env bash
# Downloads the CC0 asset packs the raylib renderer uses into assets/.
# Idempotent: skips anything already present. Re-run after a clean checkout
# (assets/models and assets/fonts are gitignored — see assets/LICENSES.md
# for what each pack is and its license statement).
#
# Packs are pinned to immutable GitHub commit-archive URLs with sha256
# verification, so a fresh clone always gets byte-identical assets.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODELS="$ROOT/assets/models"
mkdir -p "$MODELS"

# KayKit City Builder Bits 1.0 (CC0) — buildings, roads, cars, props.
CITY_SHA_COMMIT="63976910ca04d16f0fc531b9c614244be8128713"
CITY_URL="https://github.com/KayKit-Game-Assets/KayKit-City-Builder-Bits-1.0/archive/${CITY_SHA_COMMIT}.zip"
CITY_SHA256="dc101668e104968509740fcad1ed2a12f6123e1a75185f050abeef76dbefd151"

# KayKit Character Pack: Adventures 1.0 (CC0) — 5 rigged characters,
# ~75 animation clips each (Idle, Walking_A, Cheer, Death_A, ...).
CHARS_SHA_COMMIT="672074b73ba276876a19e8816ecdc5241817ab47"
CHARS_URL="https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0/archive/${CHARS_SHA_COMMIT}.zip"
CHARS_SHA256="00777a7b9811a7c7f8dd3fda4e56604f6a93b33673d64557153f354d677b47af"

# Quaternius Ultimate Modular Characters (CC0) — 4 modular rigged
# characters (Adventurer, SciFi, Soldier, Witch), each with named
# Body/Feet/Head/Legs mesh nodes on a shared skeleton and 24 shared
# animation clips (Idle, Walk, Run, Gun_Shoot, Death, ...).
# Quaternius distributes only via an unpinnable Google Drive folder, so
# this fetches a GitHub mirror pinned by commit (CC0 permits mirroring);
# the sha256 guards byte-identity either way.
MODULAR_SHA_COMMIT="b84c33822779c827fec2074edbfe98c4ddafb0e5"
MODULAR_URL="https://github.com/hukasu/bevy-modular-characters/archive/${MODULAR_SHA_COMMIT}.zip"
MODULAR_SHA256="cc22bdd1dd7944535ce84a7822f491245c1726571ba19272d14440afa425e427"

# Downloads + verifies one archive, then runs the caller-supplied unpack
# function on the extracted tree.
fetch() {
    local name="$1" url="$2" sha="$3" marker="$4" unpack="$5"
    if [ -e "$marker" ]; then
        echo "[fetch_assets] $name already present — skipping"
        return
    fi
    echo "[fetch_assets] downloading $name"
    local tmp
    tmp="$(mktemp -d)"
    trap 'rm -rf "$tmp"' RETURN
    if ! curl -fL --retry 3 -o "$tmp/pack.zip" "$url"; then
        echo "[fetch_assets] FAILED downloading $name — get it manually:" >&2
        echo "  $url" >&2
        exit 1
    fi
    echo "$sha  $tmp/pack.zip" | shasum -a 256 -c - >/dev/null || {
        echo "[fetch_assets] FAILED: $name checksum mismatch (upstream changed?) — re-pin needed" >&2
        exit 1
    }
    unzip -q "$tmp/pack.zip" -d "$tmp/x"
    "$unpack" "$tmp/x"
    echo "[fetch_assets] $name ready"
}

# City: flatten the gltf dir and co-locate the texture the .gltf files
# reference by sibling URI (the pack keeps it one directory up).
unpack_city() {
    local x="$1"
    local src
    src="$(find "$x" -type d -name gltf -path "*city_builder*" | head -1)"
    mkdir -p "$MODELS/city"
    cp "$src"/*.gltf "$src"/*.bin "$MODELS/city/"
    cp "$src/../texture/citybits_texture.png" "$MODELS/city/"
}

# Characters: the five .glb files are self-contained (embedded buffers,
# textures, and animations).
unpack_chars() {
    local x="$1"
    local src
    src="$(find "$x" -type d -path "*Characters/gltf" | head -1)"
    mkdir -p "$MODELS/characters"
    cp "$src"/*.glb "$MODELS/characters/"
}

# Modular characters: four self-contained .gltf files (embedded buffers,
# flat-color materials, no textures) at the mirror's assets/ root; the
# mirror's own code and the two stray .glb part files are not copied.
unpack_modular() {
    local x="$1"
    local src
    src="$(find "$x" -type d -name assets -path "*bevy-modular-characters*" | head -1)"
    mkdir -p "$MODELS/characters_modular"
    cp "$src"/Adventurer.gltf "$src"/SciFi.gltf "$src"/Soldier.gltf \
       "$src"/Witch.gltf "$MODELS/characters_modular/"
}

fetch "KayKit City Builder Bits" "$CITY_URL" "$CITY_SHA256" \
      "$MODELS/city/citybits_texture.png" unpack_city
fetch "KayKit Character Pack: Adventures" "$CHARS_URL" "$CHARS_SHA256" \
      "$MODELS/characters/Knight.glb" unpack_chars
fetch "Quaternius Ultimate Modular Characters" "$MODULAR_URL" "$MODULAR_SHA256" \
      "$MODELS/characters_modular/Adventurer.gltf" unpack_modular

echo "[fetch_assets] all packs present under assets/models/"
