# Asset Licenses

Every asset under `assets/` is fetched by `tools/fetch_assets.sh` (the binary
files are gitignored; this record is not). All packs are CC0 / public domain
so the game can be sold later without attribution obligations. Each row was
verified against the license file inside the pinned archive.

| Pack | Source | Pinned | License | Used for |
|------|--------|--------|---------|----------|
| KayKit City Builder Bits 1.0 | github.com/KayKit-Game-Assets/KayKit-City-Builder-Bits-1.0 | commit `6397691` (sha256-verified) | CC0 1.0 (license.txt in archive: "Free to use for personal and commercial projects, no attribution needed") | buildings, roads, cars, street props |
| KayKit Character Pack: Adventures 1.0 | github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0 | commit `672074b` (sha256-verified) | CC0 1.0 (license.txt in archive) | NPC + player character models and animations |
| Quaternius Ultimate Modular Characters | quaternius.com (author) via mirror github.com/hukasu/bevy-modular-characters | commit `b84c338` (sha256-verified) | CC0 1.0 (stated on quaternius.com pack page, which links creativecommons.org/publicdomain/zero/1.0) | modular character part meshes (Body/Feet/Head/Legs per archetype) + rigged animation clips |

Provenance note for the modular pack: Quaternius' official distribution is a
Google Drive folder (no immutable URL, not automatable), so the fetch pins a
GitHub mirror commit instead — CC0 permits redistribution, and the sha256 in
`tools/fetch_assets.sh` guarantees the fetched bytes never drift. The four
.gltf files carry no license text internally; the CC0 statement lives on the
pack's quaternius.com page.

UI text currently uses raylib's built-in default font (zlib-licensed as part
of raylib itself, compiled in — nothing fetched). A bundled TTF upgrade is
tracked in the UI parity issue.
