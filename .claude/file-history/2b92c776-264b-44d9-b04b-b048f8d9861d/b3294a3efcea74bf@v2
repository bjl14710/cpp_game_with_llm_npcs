# Plan: One Shared Character Library — Unify NPCs and the Creator, Then Expand Hair
Date: 2026-07-08 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: L

## The Idea (one paragraph)
Roaming NPCs and player-created characters are drawn by two unrelated systems,
so a created character never looks native next to the townsfolk. NPCs render as
whole **rigged glTF KayKit models** (`drawCharacter`, 5 fixed adventurers);
creator characters render as **primitive-composite parts** from the
`CharacterParts` socket catalog (`drawCompositeCharacter`). They share no
assets, and KayKit's rigged models physically can't donate swappable
heads/hair/bodies — which is exactly why the composite system was built. This
plan makes the composite `CharacterParts` catalog the **single shared character
library**: every roaming NPC gets a composite look drawn from the same pool the
creator picks from, the rigged-model path is retired as the NPC source, and the
hair category is then expanded — so new hairstyles appear for NPCs and the
creator at once, and the two can literally use the same assets.

## Goal
A character built in the creator and a townsperson are rendered by the same
parts library and look like they belong to the same world — and the creator
offers many more hairstyles, all shared with the NPCs.

## Audit finding (step 1 — already confirmed; record in OVERNIGHT_REPORT.md first)
Established first-hand this session (while shipping #92, including side-by-side
screenshots of both):
- **NPCs are NOT part-based.** `main.cpp` NPC loop: `customLooks.find(i)` →
  `drawCompositeCharacter(...)`, **else** `drawCharacter(visual)`
  (`src/app/main.cpp:983-990`). `drawCharacter` uses a rigged glTF model chosen
  by index from five KayKit adventurers loaded in `Assets.cpp:170-172`
  (Barbarian/Knight/Mage/Rogue/Rogue_Hooded; Knight = police,
  `Assets.cpp:240,246`).
- **The creator is the only part-based system.** `CharacterParts.cpp` holds the
  socketed body/head/eyes/hair catalog + palettes; `drawCompositeCharacter`
  (`RaylibRenderer.cpp`) renders it from primitives.
- **So the mismatch is not "two pools, one smaller."** It is two different
  construction methods (whole rigged models vs primitive composites) that share
  nothing. The creator's pool can't include the KayKit models because they are
  not decomposable into parts.
- **The unification hook already exists:** any NPC with a `customLooks[i]` entry
  already renders as a composite. Unifying = give *every* NPC a look.

## Decision (most-defensible pick — logged)
**Make the composite `CharacterParts` catalog the one shared library and render
every roaming NPC from it; retire rigged models as the NPC source.** This is the
only option that satisfies all five goals at once — one pool, creator as a
picker over it, NPCs native to it, and *more hairstyles* (rigged models can't
donate hair). It needs no new art pipeline because the composite NPC render path
already exists.

**Trade-off (honest):** this retires the five rigged, textured, skeletally
animated KayKit adventurers as the NPC look. NPCs become primitive composites
with the existing procedural walk-bob and billboard mood faces — lower art
fidelity, higher cohesion + customization + hair variety. The user's stated
priority is explicitly cohesion ("literally the same assets… not two separate
art sources"), so cohesion wins.

**Preserve NPC variety as the baseline (per the brief):** the current NPC "range"
is five distinct whole looks + a police variant. The composite pool already far
exceeds that combinatorially (4 bodies × 4 heads × 6 eyes × 8 hair × 8 palettes),
so each of the ten personas gets its **own distinct, authored** composite look,
and the police/armed NPCs keep a recognizable uniform look. Variety is matched
and then exceeded, never reduced.

### Alternatives considered (logged, not chosen)
- **Match-but-keep-separate:** tune composites to resemble the KayKit style so
  they read as native, keeping rigged NPCs. Rejected: does not give "the same
  assets / one pool" the brief asks for, and still blocks shared hairstyles.
- **Source a CC0 *modular* character pack** (e.g. Quaternius modular /
  KayKit skeleton-style separable parts) and rebuild BOTH systems on it. This is
  the only path that keeps rigged fidelity AND unifies — but it is XL (new asset
  import, re-rigging, socket re-mapping) and may not drop in cleanly. Noted as
  the future upgrade if rigged fidelity later outweighs simplicity.

## Out of Scope (this version)
- Sourcing/importing any new external art pack (the modular-pack alternative).
- Re-adding skeletal animation to composites (walk stays the procedural bob).
- Remote **player** rendering: fellow players still use `drawCharacter` (the
  rigged mesh) for now — a multiplayer edge; giving remote players composite
  looks is a logged follow-up, not this ticket.
- Deleting the rigged-model *loading* code in `Assets.cpp` (leave it dormant so
  remote players keep working and the path can be revived); only the **NPC
  dispatch** to it is retired.
- New part *categories* — still Body/Head/Eyes/Hair. Hair expansion is additive
  within the existing contract (as in #92).

## Affected Areas
- `src/app/main.cpp` — the NPC render loop (`~968-991`): populate `customLooks[i]`
  for **every** NPC so all render via `drawCompositeCharacter`; the
  `else drawCharacter(visual)` branch stops being reached for NPCs. Look source:
  the persona's authored look, else a deterministic fallback.
- `src/core/PersonaLoader.{hpp,cpp}` — parse an optional `look = body, head,
  eyes, hair, palette` header key into `LoadedPersona` (a placement/appearance
  concern, like `position` — additive, an unknown-key today would error, so it
  must be registered). Extend `renderPersonaText` to round-trip it.
- `src/core/Persona*` / a small helper — a deterministic
  `lookForPersona(name)` = author-supplied look if present, else
  `randomizeLook(hash(name))`, so all ten personas (and any future procedural
  NPC) get distinct, valid, style-consistent looks with zero manual gaps.
- `personas/*.persona` — add an authored `look = …` line to each of the ten
  (baker motherly, cop uniform/cap, busker casual, etc.), so looks are
  intentional, not just hashed. Police/armed personas get a uniform-reading look.
- `src/core/CharacterParts.cpp` — expand the Hair category with N new
  style-tagged parts (and any palettes that suit them), additive within the
  socket/size contract.
- `src/app/RaylibRenderer.cpp` — bespoke `drawCompositeCharacter` recipes for the
  new hair parts (or documented generic-box fallback), as in #92.
- Tests: `tests/test_character_parts.cpp` (exhaustive combo test auto-covers new
  hair; add reachability), new `tests/test_persona_look.cpp` (persona `look`
  parse + round-trip + deterministic fallback validity), and update
  `tests/test_persona_roster.cpp` if it asserts anything about NPC models.

## Implementation Order
1. **Record the audit finding in OVERNIGHT_REPORT.md** (the brief requires this
   before code): the two-systems finding above and the chosen direction.
2. **Persona `look` support** — add the optional `look` key to PersonaLoader +
   `LoadedPersona` + `renderPersonaText`, with a `lookForPersona(name)` helper
   (authored-or-deterministic). Unit-tested. *Independently committable.*
3. **Unify NPC rendering** — in main.cpp, assign every NPC a `CharacterLook` from
   step 2 and route all NPCs through `drawCompositeCharacter`; keep police/armed
   visually distinct. Rigged `drawCharacter` no longer dispatched for NPCs.
   *Independently committable.*
4. **Author the ten persona looks** — a distinct, in-character `look =` line per
   persona; verify variety ≥ the old five-model baseline. *Committable.*
5. **Expand hairstyles** — add the new style-tagged hair parts + recipes to the
   shared pool; both creator and NPCs pick them up. *Committable.*
6. **Visual QA** — side-by-side screenshot of a creator-built character next to
   roaming NPCs; confirm they read as one art source; attach the QA report to
   OVERNIGHT_REPORT.md. (Use the visual-qa subagent per the brief; if absent,
   the smoke-render + Read-PNG method used for #92.)

## Acceptance Criteria
- [ ] OVERNIGHT_REPORT.md states the audit finding (two systems, chosen
      unification) before implementation.
- [ ] Every roaming NPC renders via `drawCompositeCharacter` from the shared
      `CharacterParts` pool — no NPC uses a rigged glTF model (verify: with the
      character models absent, NPCs still render as composites, not fallback
      markers).
- [ ] A `.persona` `look = body, head, eyes, hair, palette` line parses,
      round-trips through `renderPersonaText`, and drives that NPC's look; a
      persona with no `look` still gets a deterministic, valid, distinct look
      (`tests/test_persona_look.cpp`).
- [ ] All ten personas have distinct looks; police/armed read as uniformed;
      count of visually distinct NPC looks ≥ the previous five.
- [ ] The creator's part/palette pool is exactly the pool NPCs render from — the
      creator maintains no separate subset (same `partCatalog()`/`paletteCatalog()`).
- [ ] New hairstyles appear in both the creator picker and on NPCs; every
      style-compatible combination still assembles (`tests/test_character_parts.cpp`).
- [ ] `make -C tests test` green; game builds.
- [ ] Visual-QA side-by-side screenshot in OVERNIGHT_REPORT.md shows a
      creator-built character looking native beside roaming NPCs.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|-------------------|
| Persona `look` names an unknown part/palette | Reject that look with a logged reason; fall back to the deterministic `lookForPersona(name)` so the NPC still spawns. |
| Persona has no `look` line | Deterministic `randomizeLook(hash(name))` — valid, style-consistent, stable across runs. |
| Two personas hash to similar looks | Authored `look` lines make the ten intentionally distinct; the hash fallback only covers unauthored/procedural NPCs. |
| Character models missing on disk | NPCs now render as composites regardless (composites are primitives, not loaded assets) — a strict improvement over today's marker-cylinder fallback. |
| Police/armed NPC | Authored uniform look (e.g. cap + cool/blue palette) so authority stays readable without the Knight model. |
| Remote players | Still rigged this version (out of scope); logged follow-up to give them composite looks too. |
| New hair part without a bespoke recipe | Generic declared-box fallback (documented in #92) — renders, never crashes. |

## Open Questions
- **Fidelity vs cohesion is a genuine product call.** This plan retires the
  rigged NPC models to satisfy the "one shared library / same assets" goal. If
  keeping the rigged art matters more, the off-ramp is the modular-pack
  alternative (XL) — flagged here so it can be redirected before implementation.
  Otherwise: none — plan is complete.

## Suggested GitHub Issues
1. **Persona `look` support + deterministic fallback** — optional `look` persona
   key parsed/round-tripped; `lookForPersona(name)` = authored-or-hashed. (Concept:
   *authored data with a deterministic derived fallback.*)
2. **Unify NPC rendering onto the shared composite parts library** — every NPC
   gets a CharacterLook and renders via `drawCompositeCharacter`; rigged models
   retired as the NPC source; police stays distinct. (Concept: *collapsing two
   render paths onto one shared asset library behind an existing dispatch hook.*)
3. **Author ten in-character persona looks** — a distinct look per townsperson,
   preserving/exceeding the old five-model variety. (Concept: *art-directing a
   cast within a constrained part system.*)
4. **Expand the shared hairstyle pool** — N new style-tagged hair parts + recipes,
   shared by creator and NPCs. (Concept: *additive style-tagged catalog growth in
   a single shared pool.*)
