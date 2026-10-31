# Adding a character pack

How to bring a new character asset pack into the game without breaking the
socket/size-contract system. This is the checklist the Quaternius modular
pack went through; follow it for any future pack.

## 1. Acquire, pinned

- CC0 (or equivalently unencumbered) packs only — record anything weaker
  (e.g. Mixamo's free-but-not-CC0 terms) as its own row in
  `assets/LICENSES.md` with the actual terms.
- Add the pack to `tools/fetch_assets.sh` using the existing `fetch`
  helper: an **immutable** archive URL (GitHub commit archive, never a
  branch or "latest") plus its sha256. If the author only distributes via
  an unpinnable channel (Google Drive, itch.io button), pin a mirror
  commit instead and say so in `assets/LICENSES.md` — the checksum, not
  the host, is what guarantees byte-identity.
- Unpack into its own directory under `assets/models/` (one directory per
  pack) and pick a marker file so re-runs skip cleanly.

## 2. Inspect before integrating

glTF is JSON — inspect it with python before writing any C++:

```bash
python3 - <<'EOF'
import json
d = json.load(open("assets/models/characters_modular/Adventurer.gltf"))
print("nodes:", [(n.get("name"), n.get("mesh"), n.get("skin"))
                 for n in d["nodes"] if "mesh" in n])
print("anims:", [a.get("name") for a in d.get("animations", [])])
print("mats:", [m.get("name") for m in d.get("materials", [])])
EOF
```

Answer these before proceeding:
- **Part granularity** — separate files per part, or named mesh nodes
  inside one file (the Quaternius case: `*_Body/_Feet/_Head/_Legs`)?
- **Rigged or static** — do mesh nodes reference a `skin`? Skinned meshes
  render at bind pose when drawn without animation (that is what Tier A
  static assembly draws).
- **Materials** — flat `baseColorFactor` colors (cel-shader friendly) or
  textures (need the texture co-located, see `unpack_city`)?
- **Scale and rest pose** — read the POSITION accessor `min`/`max` per
  mesh for MEASURED bounds. Never eyeball, never hand-scale: parts declare
  measured bounds as `PartDef.localSize` and the 1.8u character contract
  does all sizing.

## 3. Integrate through the pack seam

- Catalog rows in `src/core/CharacterParts.cpp` with a new `pack` tag and
  one `styleTag` family — parts from different visual families must not
  mix (the style gate enforces this).
- Mesh-backed parts set `PartDef.meshName`; the renderer resolves it
  through the one mesh dispatch branch. No new assembly or picker logic —
  if the pack seems to need any, the integration is wrong.
- Sockets are authored per part in part-local space, measured from the
  inspected geometry.

## 4. Verify

- `make -C tests test` — the exhaustive combination test absorbs new
  catalog rows automatically; proportion-window tests flag contract
  fights (fix by adjusting size specs deliberately, with the old→new
  values logged in the commit).
- Screenshot smoke: `./build/cpp_game_with_llm_npcs --frames 90 shot.png
  --camera x z yaw --hour 12` and eyeball the assembled character beside
  an existing one at the same height.
- Fresh-clone check: delete the pack's directory under `assets/models/`
  and re-run `bash tools/fetch_assets.sh` — it must reproduce the same
  bytes (checksum enforced) and the marker must skip on a second run.
