# Plan: Cutscene System
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: M

## The Idea

Scripted, non-interactive camera sequences — the shared machinery the opening
murder, wrong-accusation and win cutscenes all need. Build it once or each
reinvents it badly.

**The renderer is already shaped for this.**
`RaylibRenderer::beginFrame(const CameraPose&)` takes `{position, yawDeg,
pitchDeg}` and builds the `Camera3D` from it every frame, so a cutscene is
driving that pose from a keyframe track instead of from player input — **with no
renderer change at all.** The `--map` visual-QA harness already parks the camera
at fixed poses, so the pattern is proven.

## Goal

A designer can write a cutscene as a text file, play it in the game, and capture
a screenshot of any individual beat for review without touching C++.

## Decisions taken

| Question | Answer |
|---|---|
| Does the match clock pause? | **No — cutscenes play inside the Resolution phase that already exists.** |

## Why the Resolution phase is the right home

The day-phases plan already defines Investigation → Vote → **Resolution** → next
day, Resolution at ~15 seconds. Playing cutscenes there means:

- **The clock never pauses** — no server sync, no timeout for a client that never
  reports finished, no way for one stuck client to stall a match.
- **Skipping gains nothing.** Everyone's Resolution is the same length; a skipper
  watches the world for the remainder instead of investigating early.
- **The phase length is a hard budget**, which forces cutscenes short — genuinely
  good for something seen repeatedly across a three-day match.

The opening murder gets its own brief `Intro` phase on the same principle.

## Constraints that shape the design

- **No post-processing chain.** A fade is a full-screen `DrawRectangle` with
  alpha; letterbox bars are two more. Fine, and free — but no dissolves, no
  colour grading.
- **Fixed, non-resizable 1280×720** with no scaling 2D layout, so bar heights and
  caption positions are authored in absolute pixels — and must be, since no
  scaling system exists.
- **The built-in bitmap font computes glyph spacing with integer division**, so
  sizes 14–18 space identically. Caption sizes must come from a ladder that
  visibly differs — treat roughly 10 / 20 / 30 as usable.
- Dead NPCs already render collapsed and the day cycle already drives lighting,
  so a cutscene inherits the world's current look for free.

## Determinism is a requirement

These get iterated on visually, and `visual-qa` has to be able to say "beat 3
regressed". That only works if a beat renders identically every run, which means
**playback must advance by a fixed timestep in smoke runs, not wall-clock
delta.** A cutscene driven by real frame time produces a different image on every
machine and the screenshots are worthless as a regression signal.

The existing harness then needs no new capture code: `--frames N path` already
screenshots at frame N, so beat-by-beat capture is several runs at different
counts. One boot flag to start a named cutscene is the whole addition.

## Out of Scope

- Any actual cutscene content.
- Character animation authoring. Cutscenes move the camera and read the world as
  it is. NPCs keep doing whatever they were doing.
- Dialogue or voice. Captions only.
- Dissolves, colour grading, depth of field, motion blur.
- A cutscene editing UI. Text files, hand-edited.
- **Any renderer change.** If one becomes necessary, the design has gone wrong —
  stop and reconsider.

## Design

`cutscenes/*.cutscene`, same `key = value` style as `traits/*.trait`:

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

```cpp
enum class Ease { Linear, Smooth, Hold };

struct CutsceneBeat {
    std::string id;
    CameraPose pose{};        // REUSES the renderer's own struct
    float hold = 1.f;
    Ease ease = Ease::Smooth;
    std::string caption;
    float fadeIn = 0.f, fadeOut = 0.f;
    std::string fadeColour;   // empty = black
};

struct CutsceneDef {
    std::string id, name;
    // never / always / after_first — mandatory on first viewing, optional
    // afterwards. A repeat-viewing toll is the fastest way to make players
    // hate a scene.
    std::string skippable = "after_first";
    int letterboxPx = 90;
    std::vector<CutsceneBeat> beats;
};

class CutscenePlayer {
    void play(const CutsceneDef&);
    void skip();
    bool active() const;
    bool advance(float dtSeconds);   // fixed timestep in smoke runs
    CameraPose pose() const;         // interpolated
    float fadeAlpha() const;
    int letterboxPx() const;
    const std::string& caption() const;
    int beatIndex() const;           // for screenshot naming and tests
};
```

**`CutsceneBeat` reuses `CameraPose` rather than declaring its own position and
angles.** The pose *is* the interface between camera authoring and the renderer;
duplicating the struct is the first step toward them drifting apart.

`CutscenePlayer` is pure — owns no raylib types, draws nothing. The app layer
reads `pose()` and passes it to `beginFrame`, then draws bars, fade and caption
as 2D. Every timing rule is unit-testable with no window.

## Implementation Order

1. Parse `cutscenes/*.cutscene` + tests. Mirrors `Trait.cpp`; malformed files
   skipped with named errors.
2. `CutscenePlayer` timing + tests. Beat advance, interpolation, easing, fade
   envelope, skip, per-session seen-count. Pure, fixed timestep.
3. `AppMode::Cutscene` and input suppression, restored cleanly after — including
   mouse-look state.
4. The 2D overlay — letterbox, fade rect, caption.
5. `--cutscene <id>` boot flag, composed with `--frames N path`.
6. A fixture cutscene used only by tests, so real content can change freely.

## Acceptance Criteria

- [ ] A well-formed file parses into beats with poses, holds, easing, captions.
- [ ] A malformed file is skipped with a named error; the game still runs.
- [ ] A cutscene of known duration ends after exactly that many seconds and
      `beatIndex()` follows authored order.
- [ ] The same cutscene advanced twice with the same fixed timestep produces
      **identical poses at every step**.
- [ ] `ease = hold` snaps; `smooth` eases.
- [ ] Skip on `after_first`: mandatory the first time, immediate afterwards.
- [ ] Playback end restores input and the player's camera — no residual
      letterbox, no stuck fade, no swallowed mouse-look.
- [ ] `--cutscene <id> --frames N shot.png` captures the same beat every run.
- [ ] A cutscene longer than its phase budget is **truncated at the boundary**
      with a warning at load, not overrunning into the next phase.
- [ ] **`RaylibRenderer` is unmodified.**

## Edge Cases

| Situation | Behaviour |
|---|---|
| Cutscene file missing | Logged once; the beat does not play. Never blocks a phase transition |
| A beat has no camera fields | Inherits the previous beat's pose — how "hold and fade out" is written without repeating coordinates |
| The first beat has no camera fields | Starts from the player's current pose |
| `hold = 0` | Clamped to one timestep; a beat can never be unobservable |
| Skip during a fade | Fade completes to a clean state; never a half-dimmed screen |
| Cutscene triggered while one plays | Ignored, logged. Overlapping camera authority is worse than a dropped beat |
| Player dies during a cutscene | Playback finishes, then `AppMode::Dead`. Two camera owners must never coexist |
| Camera authored inside a building | Allowed and **not** corrected — collision-correcting an authored shot would silently ruin it |

## Open Questions

1. Should the caption ladder be 10/20/30, or should a bitmap font atlas be
   generated at a larger size? The latter fixes the spacing problem properly and
   is wanted by **three** features now (signage, captions, the aesthetics
   handoff's priority 7). It deserves its own issue.
2. Do cutscenes hide the HUD? Probably — a one-line decision at draw time.
3. Does the letterbox animate in? Sliding reads better than snapping.
