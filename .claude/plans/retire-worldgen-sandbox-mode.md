# Plan: Remove LLM World Generation, Promote Sandbox to a Real Mode
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

Delete the LLM text-to-map generation — the "Generate…" field where you describe
a village and the model builds it — because it works about half the time and
confuses more than it delivers. Keep everything it was built on: the sandbox
editor, the piece catalog, saved maps and map validation all survive untouched.
Then promote the sandbox from a menu page into a **real mode**: place buildings,
walk around, talk to the residents, no match and no mystery. Finally, let a host
build a map and push it to everyone already connected.

## Goal

A host can lay out a map by hand, hit go, and everyone already in the server is
standing in it talking to NPCs — with no LLM anywhere in that path.

## Decisions taken

| Question | Answer |
|---|---|
| First-class mode or dev tool? | **Neither, as originally framed.** Delete the LLM generation outright. Keep and promote the hand-placement sandbox to a first-class mode: build a map, roam, talk to the 21 NPCs, no match. |
| Multiplayer? | **Host builds the map; it transfers to players already in the server.** Single-player map building also works. |

This supersedes the original idea's "moved, not deleted" — generation goes.

## A correction on the size assumption

Worldgen was expected to be heavy and to bloat the codebase. Measured, it is not:

| Deleted | Lines |
|---|---|
| `src/core/WorldGen.{hpp,cpp}` | 288 |
| `src/core/WorldGenValidate.{hpp,cpp}` | 194 |
| `tools/worldgen_cli.cpp` | 189 |
| `tests/test_worldgen_validate.cpp` | 145 |
| `tools/bench_schema_models.py` | 115 |
| `main.cpp` generation block | ~100 |
| **Total** | **~1030** |

A few percent of the project, not bloat. **The reasons to delete it stand without
the size argument, and they are better:**

- **It fails half the time.** `bench/REPORT.md`: 50% schema validity on the
  winning model, 0% first-try — and `gen-village`, the mode actually wired to the
  button, was solved by *none* of the three models tested.
- **It puts ~100 lines of stateful retry machinery inside `main.cpp`'s reply
  dispatch**, the most tangled file in the project. That is the real complexity
  cost, disproportionate to a feature nobody can rely on.
- **It is the only consumer of `LlmClient` besides dialogue and group chat.**
  Generation blocks the single shared worker thread, so every NPC in the world
  goes mute while a map renders.

## The seam is already clean

Worldgen sits **above** the sandbox in the dependency graph, never below:

- `WorldGen.hpp` includes `PieceCatalog.hpp`; `WorldGenValidate.hpp` includes
  `SandboxMap.hpp`. Nothing points the other way.
- `WorldGenValidate.hpp`'s own comment: *"Map validation REUSES validateMap from
  SandboxMap.hpp; this header adds the cast side and the retry-feedback
  envelope."* So **`validateMap` and `MapError` already live in
  `SandboxMap.hpp`** — the symbols `main.cpp` uses at lines 822, 1172 and 1214
  for sandbox load/save are not in the deleted files at all.
- The only player-facing entry point is one `std::function`,
  `Menu::SandboxHooks::onGenerate`.

**Nothing needs splitting.** Both worldgen files delete whole, and the visual-QA
harness — which uses `SandboxMap` and `PieceCatalog` — cannot be affected.

## Out of Scope

- Touching `SandboxMap`, `PieceCatalog`, `validateMap` or `MapError`.
- The `--map` flag and fixture maps — the visual-QA harness depends on them.
- The detective mode.
- A map editor redesign.
- Procedural (non-LLM) map generation. Not a replacement, not wanted.
- Reviving worldgen behind a flag. Git history is the archive, as with the
  SFML-era features in `legacy-feature-gaps.md`.
- Persisting maps server-side or a map browser.

## Map transfer design

`SandboxMap` already has `toJson()` / `fromJson()` with a `version: 1` gate that
rejects newer versions, and `buildCity(map)` compiles it. Transfer is almost free:

| Type | Direction | Payload |
|---|---|---|
| `SandboxMapSync` | server → all | The host's `SandboxMap::toJson()` |

Client on receipt: `fromJson` → `validateMap` → `buildCity` → swap the city.
**Validate on the client too** — never trust a peer's payload to be well-formed
just because the host sent it.

The hard part is not the transfer, it is what happens to people standing in it:
every connected player is at a position that may now be inside a wall. **Players
respawn at the new map's spawn point on swap**, and that is the behaviour, not an
edge case to patch later.

## Implementation Order

Deletion first — it shrinks the surface everything else works against.

1. **Delete the generation code and its wiring.** Both core files, the CLI, its
   test, the bench script, the `main.cpp` block, the menu hook. Build clean,
   suite green. The suite should lose *only* `test_worldgen_validate.cpp`'s
   cases; if anything else moves, something depended on worldgen that this audit
   missed — stop and re-check.
2. **Mark the bench report section historical.** One paragraph explaining that
   the measured 50% validity is why the feature was removed. This is the decision
   record.
3. **Promote sandbox to a mode.** A menu entry that loads a saved map (or a new
   one), spawns the residents, and plays with no match running.
4. **`SandboxMapSync` message type + round-trip tests.**
5. **Host push and client apply**, including respawn on swap.

## Acceptance Criteria

- [ ] No source references `WorldGen.hpp` or `WorldGenValidate.hpp`, and
      `cmake --build` succeeds with no new warnings.
- [ ] `make -C tests test` green, and the ONLY removed cases are
      `test_worldgen_validate.cpp`'s.
- [ ] Every case in `tests/test_sandbox_map.cpp` still passes unchanged.
- [ ] `--map <fixture>` boots the visual-QA fixture exactly as before.
- [ ] The Sandbox page has no "Generate…" field and no way to reach the model.
- [ ] Sandbox mode: place pieces, save, reload, walk around, hold a conversation,
      with no match phases running.
- [ ] A host push rebuilds the same city on every connected client and respawns
      them at the new spawn point.
- [ ] A malformed or newer-version map is rejected and the client keeps its
      current world.
- [ ] `LlmClient` has exactly two callers left: dialogue and group chat.

## Edge Cases

| Situation | Behaviour |
|---|---|
| A saved map references a removed piece id | Already handled by `validateMap`; load fails with named errors |
| A `gen_*` character in an old generated village | Loads as an ordinary placed NPC if its `.persona` exists; otherwise skipped with a named error. Old saves must not crash |
| Host pushes mid-conversation | The conversation closes; the world changed underneath it |
| Client mid-jump when the map swaps | Respawn overrides in-flight motion |
| Map JSON exceeds a sane frame size | `NetFraming` is length-prefixed and handles large frames; cap at validation and reject beyond it |
| A client joins after the push | Out of scope — join-time map sync belongs with the lobby work |
| Someone wants worldgen back | Git history. `bench/REPORT.md` records why it went |

## Open Questions

1. Does sandbox mode use all 21 residents or a chosen subset? Placing NPCs is
   already part of the editor, so possibly the map decides.
2. Should sandbox mode share the detective map, or only saved maps? Wandering
   downtown without a match is useful for testing.
3. Does `bench_schema_models.py` deserve to survive as a general schema-validity
   harness? It measures something real; only its *subject* is being retired.
   Default here is delete-with-history rather than a strong opinion.
