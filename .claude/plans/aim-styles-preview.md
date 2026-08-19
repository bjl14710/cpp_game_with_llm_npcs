# Plan: Gun-Aim Fix, More Character Styles, and Player-Driven Creator Preview
Date: 2026-07-07 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION
Estimated complexity: M

## The Idea (one paragraph)
Three targeted fixes to the first-person town game. (1) **Gun aim** currently
fires from the player's feet, horizontally, ignoring where the camera is
actually pointed — the same class of bug as the earlier "moonwalk" fix, where a
value was reconstructed separately from its authoritative source and drifted.
Fix it at the source: derive one shared look/aim vector (from yaw **and** pitch)
that both the camera and the weapon consume, so every current and future weapon
aims where you look by construction. (2) **Character styles** — add more
style-tagged parts (heads, eyes, hair, bodies) and palettes to the existing
socket/style-tag catalog, purely additively, with matching renderer recipes so
they look good. (3) **Creator preview** — replace the continuous auto-spin
turntable with a two-state model: rotation is driven only by player drag/keys,
and only after a real idle timeout does the figure ease back to facing the
camera. No unconditional spin path remains.

## Goal
A player can shoot exactly where the crosshair points (up, down, level), build a
visibly more varied custom character, and rotate the creator preview themselves —
inspecting the eyes or the back of the hair — instead of waiting out a forced
spin.

## Out of Scope (this version)
- **Aim:** no crosshair/reticle art, no bullet-drop/gravity, no per-weapon spread
  or recoil-on-aim, no aim-assist, no changing the Fist melee's
  nearest-target logic, no vertical aim for NPC-fired shots (NPCs still use their
  existing hit-roll). No multiplayer combat replication (still host/solo only).
- **Styles:** no new part *categories* (still Body/Head/Eyes/Hair), no new
  architecture, no textures/skeletal parts — additive rows within the existing
  contract only. No per-part color customization beyond the palette system.
- **Preview:** no opt-in "auto-rotate preview mode" is built now (only left
  structurally easy to add later as an explicit third state). No zoom, no pan, no
  lighting controls, no turntable for the in-world NPCs.

## Affected Areas

### Part A — Gun aim (fix at the source)
- `src/core/Math.hpp` — **new** pure helper `lookDirection(float yawDeg, float
  pitchDeg)` returning the normalized 3D look vector. Single source of truth for
  "where the player is looking." (Also a shared `kEyeHeight = 1.7f` constant.)
- `src/app/RaylibRenderer.cpp:138-144` — `beginFrame` builds `camera_.target`
  from the new `lookDirection` (and uses `kEyeHeight`) instead of the inline
  duplicated yaw/pitch math, so the camera and the aim can never diverge again.
- `src/app/main.cpp:553` — pass `lookDirection(player.yawDeg, player.pitchDeg)`
  to `world.playerAttack(...)` instead of `flatForward(player.yawDeg)`.
  `flatForward` **stays** for WASD movement (movement is intentionally
  horizontal; only aim needed pitch).
- `src/core/World.cpp:34-41` (`playerAttack`) — spawn the projectile from **eye
  height** (`player_.position + {0, kEyeHeight, 0}`) along the **full 3D aim**
  (drop the `y = 0` flatten). Projectiles already integrate in 3D
  (`tickProjectiles`, `World.cpp:66`), and the existing XZ-distance + `[0,
  kNpcHeight]` band hit-test (`World.cpp:83-85`) still works because the shot now
  passes through the body band on its way to the target.
- Tests: `tests/test_combat_world.cpp` (aim origin/direction), new
  `lookDirection` unit test (see Acceptance Criteria).

### Part B — More character styles
- `src/core/CharacterParts.cpp:45-79` — add new `PartDef` rows to `partCatalog()`
  and new `PartPalette` rows to `paletteCatalog()`, each with a style tag
  (`round`/`blocky`/`any`), `localSize`, and (for bodies/heads) child sockets
  copied/tuned from the existing rows.
- `src/app/RaylibRenderer.cpp:338-382` (`drawCompositeCharacter`) — add a bespoke
  primitive recipe for each new part id so it reads as intended. Parts without a
  recipe already fall back to the generic declared-box cube (`RaylibRenderer.cpp`
  generic branch), so this is a look-quality step, not a correctness one.
- Tests: `tests/test_character_parts.cpp` — the exhaustive
  style-compatible-combination assembly test grows automatically; update any
  hardcoded catalog-count assertions and add reachability checks.

### Part C — Player-driven creator preview
- `src/app/main.cpp:510-511, 990-997` — replace the `previewSpinDeg += dt * 35.f`
  turntable with the two-state rotation model (locals `previewYaw`,
  `previewIdleSeconds`; drag/keys drive it; idle eases to face the camera).
- `src/app/Menu.hpp` / `src/app/Menu.cpp` — add `bool pointOverInteractive(
  Vector2) const` (reuses `layout()`) so main.cpp rotates the preview only when a
  left-press did **not** land on a menu control (prevents cycler/button clicks
  from also spinning the model). Update the stale "slowly turning" comment at
  `Menu.hpp:79-81`.
- No new files.

## Implementation Order

1. **Aim fix (Part A).** Add `lookDirection` + `kEyeHeight` to `Math.hpp`; route
   the camera through it; change the `playerAttack` call site and spawn origin/
   direction; drop the y-flatten. Add tests. *Independently committable:*
   `fix(combat): aim the gun where the camera looks, from eye height`.

2. **Style variety (Part B).** Add the new parts + palettes to the catalog and
   their renderer recipes; update/extend the parts tests. *Independently
   committable:* `feat(creator): more style-tagged parts and palettes`.

3. **Preview rotation (Part C).** Replace the auto-spin with the drag + idle-return
   state model; add `Menu::pointOverInteractive`. *Independently committable:*
   `feat(creator): player-driven preview rotation with idle return-to-front`.

## Autonomy decisions (most-defensible picks — logged for later tuning)

- **Aim root-cause approach (as instructed):** the derived-value fix, not a
  per-weapon patch. The camera's full 3D look vector (`RaylibRenderer.cpp:141-142`)
  is the authoritative aim target; the weapon was reconstructing a *separate*,
  degraded aim (`flatForward(yaw)`, horizontal, from the feet). Unifying both onto
  one `lookDirection(yaw, pitch)` means a new weapon that calls `playerAttack`
  with the shared vector aims correctly with zero new aim code — the bug can't be
  reintroduced per-weapon.
- **Eye height = `kEyeHeight = 1.7f`** — matches the camera eye already hardcoded
  at `RaylibRenderer.cpp:138`; promoted to a shared constant so spawn origin and
  camera can never disagree.
- **Movement stays horizontal:** `flatForward(yaw)` is deliberately kept for WASD
  — walking should not drift vertically when you look up. Only *aim* consumes
  pitch.
- **New parts (additive, bounded):** +2 per category and +3 palettes —
  Bodies: `body_slim` (round), `body_bulk` (blocky); Heads: `head_oval` (round),
  `head_tall` (blocky); Eyes: `eyes_happy` (any, arced), `eyes_round` (round);
  Hair: `hair_pony` (round), `hair_mohawk` (blocky); Palettes: `berry`, `slate`,
  `mint`. Each new part is style-tagged so it composes under the existing
  `styleCompatible` gate; each gets a bespoke renderer recipe. Final names/shapes
  may be tuned during implementation; the count and the additive-within-contract
  rule are fixed.
- **Preview idle timeout `kPreviewIdleTimeout = 2.5f` s** — long enough that
  reading the model or pausing mid-drag doesn't trigger a snap-back, short enough
  that a player who lets go sees it settle promptly. Tunable.
- **Preview ease `kPreviewEaseSpeed`:** exponential smoothing toward the default
  orientation, `previewYaw += shortestAngle(previewYaw → defaultYaw) * (1 -
  exp(-6.0f * dt))` — ~0.4–0.6 s to settle, no overshoot. Tunable via the `6.0f`
  rate.
- **Preview drag sensitivity `kPreviewDragSensitivity = 0.4f`** deg per pixel of
  horizontal mouse delta; Left/Right arrow keys also rotate at `90 deg/s` as a
  no-mouse input path. Tunable.
- **Default orientation = front toward the camera.** The preview sits at
  `player.position + flatForward(yaw) * 3.4`, so "facing the player" is
  `defaultYaw = player.yawDeg + 180` (shortest-path eased). This is the fixed
  fallback, never a spin.
- **Two states only, third left easy:** model rotation as an explicit
  `enum PreviewRotate { Drag, IdleReturn }` (or equivalent branch) so a future
  opt-in `Auto` mode is a clean added case — but `Auto` is **not** built now, and
  there is no code path where rotation advances without either live input or the
  idle-return ease.
- **Branch:** `feature/aim-styles-preview` stacked on `feature/player-journal`
  (the current tip carries all three systems — combat in `World.cpp`, the part
  catalog, and the creator preview). One draft PR; do not merge; do not touch
  main.

## Acceptance Criteria

- [ ] **Aim — horizontal parity preserved:** `lookDirection(yaw, 0)` equals
  `flatForward(yaw)` for representative yaws (unit test in a new
  `tests/test_aim.cpp` or `tests/test_math`).
- [ ] **Aim — convention correct:** `lookDirection(0,0) ≈ (0,0,1)`,
  `lookDirection(90,0) ≈ (1,0,0)`, `lookDirection(0,+45)` has `y > 0` and
  `lookDirection(0,-45)` has `y < 0` (unit test).
- [ ] **Aim — pitch reaches the projectile:** `World::playerAttack` with a
  downward aim spawns a projectile whose `direction.y < 0` and whose
  `position.y == kEyeHeight` (previously always `y == 0` origin and direction);
  test in `tests/test_combat_world.cpp`.
- [ ] **Aim — regression:** firing while pitched up then level produces different
  `direction.y` values (proves aim is derived per-shot from the live look, not a
  stored constant).
- [ ] **Styles — additive & valid:** every new part appears in
  `partsForCategory` for its category and every style-compatible combination
  including the new parts assembles (`lookIsValid` == `styleCompatible`), verified
  by the existing exhaustive test in `tests/test_character_parts.cpp`.
- [ ] **Styles — renderable:** each new part id has a bespoke recipe or the
  documented generic-box fallback (no crash, no empty draw); the ≥3 new palettes
  are selectable.
- [ ] **Preview — no auto-spin:** with no input on the Creator page, the figure
  holds still until `kPreviewIdleTimeout`, then eases to face the camera and
  stops. There is no frame where yaw advances without input or idle-return.
- [ ] **Preview — drag drives it:** left-dragging over the preview (not over a
  menu control) rotates the figure by the drag delta; arrow keys also rotate.
- [ ] **Preview — clicks don't spin:** clicking the ‹ › cyclers, palette, or
  buttons changes the look without rotating the preview
  (`Menu::pointOverInteractive` gate).
- [ ] Tests pass: `cmake --build build -j && make -C tests test` (suite green;
  new aim + parts assertions included).

## Edge Cases and Error Handling

| Situation | Expected behaviour |
|-----------|-------------------|
| Player fires while looking straight up/down (pitch ±75°) | Projectile travels along the full 3D aim from eye height; may miss all NPCs — that's correct, not a bug. |
| Pitch clamp at ±75° (`kMaxPitchDeg`) | `lookDirection` stays finite and normalized; no gimbal/NaN. |
| Fist melee | Unchanged — still nearest-target in range; aim vector ignored for melee. |
| New part has no bespoke renderer recipe | Falls back to the generic declared-box cube (already implemented) — appears plainly, never crashes. |
| Style-incompatible new combo (e.g. blocky eyes on a round head) | Rejected by `styleCompatible`/`lookIsValid` exactly as today; picker won't offer it. |
| Hardcoded catalog counts in tests | Updated to the new totals as part of Part B. |
| Preview: drag starts on a menu control | Treated as a click (Menu handles it); no rotation — `pointOverInteractive` gate. |
| Preview: player drags, then stops mid-rotation | Holds the current yaw until `kPreviewIdleTimeout`, then eases to face camera (does not resume any spin). |
| Preview: rapid alternating drag/idle | `previewIdleSeconds` resets on every input frame, so the figure only returns after a genuine continuous idle. |
| Left+Right arrows held together | Net zero rotation; treated as input (idle timer resets), so no snap-back while held. |

## Open Questions
None — plan is complete. Named defaults (idle timeout, ease rate, drag
sensitivity, eye height, new-part list) are logged above and tunable without
re-deriving any logic.

## Suggested GitHub Issues

1. **Fix gun aim: derive it from the shared look vector, fire from eye height** —
   add `lookDirection(yaw,pitch)` + `kEyeHeight` in core, route camera + weapon
   through it, spawn from eye along the full 3D aim; tests. (Concept: *deriving a
   value from its authoritative source instead of reconstructing it — the
   moonwalk/aim class of bug.*)
2. **Add more style-tagged character parts and palettes** — additive rows in the
   part/palette catalog with matching renderer recipes; extend the parts tests.
   (Concept: *extending a socket/style-tag content catalog additively without new
   architecture.*)
3. **Replace creator preview auto-spin with player-driven rotation + idle
   return** — two-state drag/idle-return model, `Menu::pointOverInteractive`
   gate, no unconditional spin. (Concept: *input-driven state with an
   idle-timeout fallback, structured for a later opt-in third state.*)
