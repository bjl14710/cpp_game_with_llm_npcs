# Plan: Mii-Style Visual Overhaul, Expanded Customization, Player Avatar, Playtest
Date: 2026-07-10 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
Push the game's look toward a cartoony, Mii/Tomodachi-Life-inspired style —
friendly proportions (oversized heads), clean bold outlines, one cohesive
pastel palette — applied through the existing shared composite parts library
so roaming NPCs, the character creator, and (new) the player's own avatar all
change together. Expand the parts pool substantially (a new Mouth category,
more hair/eyes/bodies/palettes), give the player an avatar editable with the
same creator UI, tag every part/palette with a *pack* id so purchased or
downloaded graphics packs can slot in later, and finish with a visual-qa
playtest that both verifies consistency and records improvement ideas.

## Goal
The player opens one creator UI to restyle their own avatar or build NPCs
from a visibly Mii-like, outlined, pastel cast — and every character in town
reads as the same cartoon art style.

---

## PART A — Engine assessment (verdict: NO migration; recommendation only, as briefed)

**raylib 5.5 is not a blocker for a cohesive Mii/Tomodachi-style target.
No migration is warranted.** Reasoning, for the record:

1. **The Mii look is technically modest.** Tomodachi-style rendering is
   low-poly rounded shapes + flat colors + soft/no shadows + dark outlines.
   The game already renders flat-shaded primitives (spheres, cylinders,
   cubes) with palette colors — the *hard* part of a toon pipeline (removing
   realistic lighting/texturing) is already our baseline.
2. **Outlines are achievable with tools already in use.** The classic
   inverted-hull technique (redraw each shape slightly enlarged, dark,
   with front-face culling) needs only `rlSetCullFace` + a scaled second
   pass inside `drawCompositeCharacter` — the same `rlgl` layer the
   composite renderer already drives (`rlPushMatrix`/`rlScalef`). The fog
   shader (issue #67) proves custom shading integrates cleanly if a
   post-process outline is ever preferred later.
3. **Proportions and palette are data, not engine.** Head-to-body ratio and
   colors live in `CharacterParts.cpp` catalog rows — no engine feature is
   involved at all.
4. **Migration cost is extreme and buys nothing needed.** The C++ core
   (World, LLM streaming, networking, SQLite persistence) plus 176 doctest
   cases and the whole draft-PR chain are raylib-adjacent but C++-native;
   Unity/Godot would strand or wrap all of it for zero rendering capability
   we actually lack. Even a raylib→bgfx/sokol swap would rebuild the
   renderer for features (deferred lighting, shadow maps) a flat toon style
   deliberately avoids.

Per the brief this verdict is stated plainly and Parts B–D proceed. No
migration plan document is produced because none is recommended.

---

## Out of Scope (this version)
- Any engine/renderer migration (Part A verdict: not needed).
- Post-process (shader-based) outlines — inverted hull is the v1; a
  full-screen outline shader is a logged alternative if hull outlines
  disappoint on boxes.
- Replicating the player avatar's look to OTHER machines in multiplayer
  (needs a net protocol change; the seam is left: remote players keep the
  rigged mesh, logged follow-up as in PR #99).
- Skeletal animation / limbs on composites (procedural bob stays; Miis
  barely articulate anyway — deliberate style fit).
- Restyling the KayKit *city* (buildings/cars stay; only sky/light palette
  constants may be gently tuned if characters clash after the pastel pass).
- Shipping more than one graphics pack — tonight builds the "core" pack and
  the seam, not a second pack.
- Acting on Part D playtest findings beyond recording them.

## Affected Areas
- `src/core/CharacterParts.hpp` — `PartDef::pack` + `PartPalette::pack`
  ("core" default) — the graphics-pack seam; `PartCategory::Mouth` + count
  bump; catalog growth.
- `src/core/CharacterParts.cpp` — Mii-proportion size/socket rework of
  existing rows; `mouth` sockets on all heads; new CategorySpec row
  (Mouth→Head via "mouth"); new parts (mouths, hair, eyes, bodies) and
  pastel palettes.
- `src/app/RaylibRenderer.cpp` — outline pass (inverted hull around the
  existing per-part recipe loop); recipes for the new parts; recipe
  registry isolated behind one documented dispatch function (pack seam).
- `src/app/Menu.{hpp,cpp}` — Mouth label in `categoryLabel`; an "Avatar"
  entry that reuses the SAME creator page in avatar mode (same picker,
  Randomize, preview; Save applies to the player instead of spawning).
- `src/app/main.cpp` — player avatar look: load/persist, wire the avatar
  hooks, tint the punch sleeve/fist and pistol hand from the avatar's
  palette (the only first-person place the avatar is visible).
- `src/core/CharacterStore.{hpp,cpp}` — persist the avatar look under a
  reserved id (e.g. `player_avatar`) in the existing look table (same
  format, same validation path; no new storage system).
- Tests: `tests/test_character_parts.cpp` (Mouth category joins the
  exhaustive combo test; pack tags present), `tests/test_character_store.cpp`
  (avatar look round-trip), `tests/test_persona_look.cpp` /
  `tests/test_persona_roster.cpp` (roster looks stay valid after the
  proportion/catalog rework).

## Implementation Order
1. **Pack seam + recipe isolation** — `pack` field on parts/palettes
   (all "core"), part recipes gathered behind one dispatch function with a
   documented contract ("a pack = catalog rows + recipes + palettes").
   Data-only change, everything still renders identically. *Committable.*
2. **Mii proportions** — resize existing catalog rows: heads ~0.95–1.05u on
   shortened bodies (~0.75–0.85u) so heads read as ~45% of height; sockets
   re-authored per part (the socket contract localizes this); palettes
   shifted to a cohesive Tomodachi pastel set. Exhaustive combo + roster
   tests confirm every look still assembles. *Committable.*
3. **Outline pass** — inverted-hull second draw (scale ≈1.05 about each
   part's center, near-black, front-face culling) wrapped around the recipe
   loop in `drawCompositeCharacter`; screenshot-verify on spheres AND boxes
   (boxes are the risk case; if corners gap, fall back to slightly larger
   scale or per-part outline opt-out flag). *Committable.*
4. **Mouth category** — enum + count bump, CategorySpec row, `mouth`
   sockets on all four heads, 4–5 mouth parts (smile, open smile, neutral,
   "cat" mouth, small o) + recipes, Menu label. Combo test absorbs it;
   authored persona looks gain a mouth entry (parse stays 5-part?
   **Decision:** `look =` grows to six items with the old five-item form
   still accepted, mouth defaulting per-persona via the deterministic
   fallback — round-trip renders six). *Committable.*
5. **Catalog expansion** — +6 hair, +4 eyes, +2 bodies, +4 pastel palettes
   (all pack "core", style-tagged, contract-conforming) with recipes; the
   ten personas' authored looks refreshed to exploit the range (distinct,
   in-character, roster-test-enforced). *Committable.*
6. **Player avatar** — reserved `player_avatar` row in CharacterStore;
   creator page reused in avatar mode from a new menu entry ("Edit My
   Avatar"); punch sleeve/fist + pistol hand tinted from the avatar
   palette; avatar look validated on load (stale → deterministic fallback,
   logged, same rule as personas). *Committable.*
7. **Part D — visual QA + playtest** — visual-qa subagent: (a) side-by-side
   screenshot set: creator preview, roaming NPCs, and the avatar's visible
   surfaces, judged for one-art-source consistency + outline quality;
   (b) free-form playtest sweep (walk the town at different hours, talk,
   fight, jump, create a character) collecting concrete improvement notes.
   Findings list → OVERNIGHT_REPORT.md, no action taken on them.

## Acceptance Criteria
- [ ] Part A verdict recorded (this document + OVERNIGHT_REPORT.md) —
      no migration performed, no migration code written.
- [ ] Every `PartDef` and `PartPalette` carries a `pack` tag; renderer
      recipes are dispatched behind one documented function; no style
      assumption lives outside catalog data + recipes (grep-verifiable:
      no part-id literals in main.cpp/Menu.cpp).
- [ ] Characters render with visible dark outlines and Mii proportions
      (head ≈ 40–45% of standing height, screenshot-verified) across NPCs,
      creator preview, and avatar surfaces.
- [ ] Mouth is a fifth category: every head has a `mouth` socket, the
      exhaustive combination test covers it, and old five-item `look =`
      lines still parse (mouth defaults deterministically).
- [ ] Hair ≥ 18, eyes ≥ 10, bodies ≥ 6, mouths ≥ 4, palettes ≥ 12 — all
      style-tagged, all assembling in the combo test.
- [ ] The player can open "Edit My Avatar", restyle with the same picker
      used for NPC creation, and the choice persists across restarts
      (CharacterStore round-trip test) and tints the first-person arm/hand.
- [ ] Ten persona looks remain authored, valid, pairwise-distinct
      (roster test) after the proportion rework.
- [ ] `make -C tests test` green; game builds; smoke screenshots captured.
- [ ] OVERNIGHT_REPORT.md contains the visual-qa consistency verdict AND
      a playtest findings list (observations only, unactioned).

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Old five-item `look =` line (no mouth) | Parses; mouth filled by the deterministic per-name fallback — files never break. |
| Stored NPC/avatar look predates Mouth | `lookIsValid` fails on the empty slot → same demote-to-fallback path personas use, with a logged reason. |
| Box parts make ragged inverted-hull outlines | Tune hull scale; if still poor, per-part `outline=false` opt-out in the recipe table (logged), not a global abandon. |
| Avatar look row missing/corrupt in saves | Deterministic fallback from a fixed seed ("player"), logged; creator can overwrite it. |
| A pack tag other than "core" appears (future) | Picker filters to active packs; unknown-pack parts in stored looks demote to fallback like any unknown id. |
| Proportion rework breaks an authored persona look | Roster test fails in step 2 — fix the look line in the same commit; looks never silently fall back for the shipped ten. |
| Multiplayer guest sees the host's avatar | Unchanged this version: remote players keep the rigged mesh (logged out-of-scope + follow-up). |

## Open Questions
None blocking — decisions above are logged with reasoning per the autonomy
brief (six-item look format, inverted-hull v1, avatar stored in
CharacterStore under a reserved id, remote replication deferred).

## Suggested GitHub Issues
1. **feat(parts): graphics-pack seam — pack tags + isolated recipe dispatch** — pack field on parts/palettes, recipes behind one documented function. (Concept: *content-pack seams via tagged data.*)
2. **feat(style): Mii proportions + cohesive pastel palette rework** — catalog resize/re-socket, palette overhaul, roster looks kept valid. (Concept: *art direction as pure data under a socket contract.*)
3. **feat(render): inverted-hull cartoon outlines for composites** — two-pass outline in drawCompositeCharacter, box-corner risk handled. (Concept: *inverted-hull outlining.*)
4. **feat(parts): Mouth category** — fifth category end-to-end; look format grows to six items, five-item back-compat. (Concept: *extending a category-spec system without touching assembly code.*)
5. **feat(parts): catalog expansion + refreshed persona looks** — hair/eyes/bodies/mouths/palettes growth, ten looks re-authored. (Concept: *scaling a constrained content catalog.*)
6. **feat(avatar): player avatar via the shared creator** — reserved store row, avatar mode on the creator page, first-person tinting. (Concept: *one UI, two write targets.*)
7. **chore(qa): visual consistency gate + free playtest findings** — visual-qa side-by-sides + playtest notes to OVERNIGHT_REPORT.md. (Concept: *self-directed playtesting.*)
