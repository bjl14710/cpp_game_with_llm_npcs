# Cutscenes

Scripted, non-interactive camera sequences. Same `key = value` text shape as
`traits/*.trait` and `storylines/*.storyline`, so authored content reviews as an
ordinary diff in a PR.

Parsed by `src/core/Cutscene.{hpp,cpp}`. Played by `CutscenePlayer`, which is
pure — it owns no raylib and draws nothing. `src/app/main.cpp` reads `pose()`,
hands it to `beginFrame`, and draws the bars, fade and caption as 2D.

**The renderer is unmodified and must stay that way.**
`RaylibRenderer::beginFrame` already takes a `CameraPose` and rebuilds its
`Camera3D` from it every frame, so a cutscene is nothing more than driving that
pose from a keyframe track instead of from player input. If a renderer change
starts to look necessary, the design has gone wrong — stop and reconsider.

## Format

```
name = Opening Discovery
skippable = after_first
letterbox = 90

beat = establish
  camera = -8.0, 2.2, 14.0
  yaw = 200
  pitch = -6
  hold = 2.5
  ease = smooth
  caption = Tuesday morning.
  fade_in = 1.0

beat = out
  hold = 1.0
  fade_out = 1.0
  fade_colour = grey
```

| Key | Scope | Meaning |
|---|---|---|
| `name` | file | Player-facing title. Defaults to the filename stem. |
| `skippable` | file | `never`, `always` or `after_first`. |
| `letterbox` | file | Bar height in pixels, top and bottom. |
| `beat` | starts a beat | The beat's id — used in test assertions and screenshot names. |
| `camera` | beat | `x, y, z`. **Eye height, literally** — see below. |
| `yaw` / `pitch` | beat | Degrees. `yaw 0` faces +Z, `yaw +90` faces +X, `pitch > 0` looks up. |
| `hold` | beat | Seconds. Clamped up to one timestep. |
| `ease` | beat | `linear`, `smooth` (default) or `hold`. |
| `caption` | beat | Text centred above the lower bar, or under the headline on a card beat. |
| `slug` | beat | Place and clock, drawn under the top bar in label grey. |
| `speaker` | beat | `Name: line`, drawn in the transcript's amber. |
| `headline` | beat | 44px card line. **Its presence makes the beat a card**: a dim wash goes over the whole frame and the caption moves under it. |
| `journal_subject` | file | Subject key this scene opens on the fact bus. Normalized to `[a-z0-9_]` at parse. |
| `journal_line` | file | The fact written under that subject, max 140 characters. |
| `fade_in` / `fade_out` | beat | Seconds of fade at the start / end of the beat. |
| `fade_colour` | beat | `black` (default), `white` or `grey`. |

The filename stem is the id. An `id =` key that disagrees with it is reported
and ignored.

`#` opens a comment **only as the first non-space character**, because captions
are prose and `Table #3, still set for two.` has to survive.

## A cutscene may open a journal subject

`journal_subject` + `journal_line` write one `KnownFact` (source `town`) when
playback ends, and grant it to the player.

This is the point of the whole feature. The journal already renders
`[conflicting accounts]` in orange when two residents disagree on a subject —
a murder-investigation mechanic that sat finished with nothing to be conflicted
about. Opening one subject key is what turns ten chatty residents into a case.

**A skip still writes.** Skipping must not quietly cost the player the premise
of the game, so the write happens on any exit from playback.

**Set both keys or neither.** The write needs a subject and a line; a scene
carrying one of them is reported at parse and has both cleared, because half a
pair silently writes nothing — and a scene using its own fact as an
"already played" test would then replay forever.

`addFact` is first-teller-wins, so replaying a scene never duplicates its row or
re-dates it — and because facts round-trip through `saves/facts.sqlite3`, the
row is also how a "play once per save" scene knows it has already played. That
is one source of truth rather than a second seen-flag that can disagree with it.

## Camera height is literal

`CameraPose` elsewhere in the codebase is feet-space and `beginFrame` adds
`kEyeHeight` (1.7m). Cutscene poses are **eye-space**: `camera = 0, 2.2, 0`
puts the lens at 2.2m, and the app subtracts the eye height on the way through.

Authors should not have to know how tall the player is to frame a shot.

## Beats inherit

A beat that sets no camera fields holds the previous beat's pose. That is how
"hold here and fade out" is written without repeating coordinates. A beat may
inherit partially — set `yaw` alone for a pan from wherever the last shot ended.

**The first beat inherits from wherever the camera was when playback started**,
which is why inheritance is resolved at `play()` rather than at parse time.

## Determinism is a requirement

These get iterated on visually and `visual-qa` has to be able to say "beat 3
regressed". That only works if a beat renders identically every run, so
playback advances by a **fixed timestep** in smoke runs, never wall-clock delta.
A cutscene driven by real frame time produces a different image on every machine
and the captures are then worthless as a regression signal.

```
./cpp_game_with_llm_npcs --frames 90 shot.png --cutscene fixture_pan
```

Capture a specific beat by picking the frame count: at 60 Hz, frame N is at
N/60 seconds into the scene.

## Cutscenes never pause the clock

They play inside the Resolution phase that already exists. That means no server
sync, no timeout for a client that never reports finished, and no way for one
stuck client to stall a match. It also means **the phase length is a hard
budget** — `truncateToBudget` trims an overlong scene at the boundary and warns,
rather than letting it run into the next phase.

## What is deliberately not here

- **No post-processing.** A fade is a rectangle with alpha; bars are two more.
  No dissolves, no colour grading, no depth of field.
- **No character animation.** Cutscenes move the camera and read the world as it
  is. NPCs keep doing whatever they were doing.
- **No dialogue or voice.** Captions only.
- **No editing UI.** Text files, hand-edited.

## Layout is absolute pixels

Fixed, non-resizable 1280×720 with no scaling 2D layout, so bar heights and
caption positions are authored in absolute pixels and must be.

The built-in bitmap font computes glyph spacing with **integer division**, so
sizes 14–18 space identically. Caption sizes come from a ladder that visibly
differs — roughly 10 / 20 / 30.

## Captions are ASCII only

The built-in font has glyphs for **ASCII 32–126 and nothing else**. An em-dash,
a curly quote or an ellipsis character reaches the screen as a literal `?`.

Write `-` not `—`, `'` not `’`, `...` not `…`.

The parser reports a non-ASCII caption at load, because prose pasted from a
document will do this constantly and the failure is otherwise invisible until
someone actually runs the scene. This was found by looking at a capture — no
test would have caught it, which is the argument for capturing every scene.

The proper fix is a generated bitmap font atlas at a larger size, which would
also solve the integer-spacing problem. **Three features want it now** (signage,
captions, and the aesthetics handoff's priority 7), so it deserves its own
issue rather than a workaround per consumer.
