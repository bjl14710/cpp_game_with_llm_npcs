# Plan: Opening Murder Cutscene
Date: 2026-08-07 (rewritten 2026-08-08 after the original was lost)
Status: READY FOR IMPLEMENTATION — not yet issued
Estimated complexity: S

## The Idea

A short cutscene at match start establishing where the crime happened and roughly
when, without ever hinting who did it. **Re-watchable from the Journal**, because
a player who goes back and notices the clock read 9:40 has found a real clue that
constrains every alibi they hear afterwards.

That makes it the first **generated** cutscene: location and time change every
match, so it is built from `MysterySetup` rather than hand-authored.

## Goal

A player can watch the opening, replay it later from the Journal, and use what it
shows — the place and the hour — to catch a resident whose story does not fit.

## Decisions taken

| Question | Answer |
|---|---|
| Once, or re-watchable? | **Re-watchable, carrying non-identifying clues.** Establishes when and where, never who. Requires per-match generation and a leak test. |

## The constraint that shapes the whole thing

The cutscene system drives the camera over the **live world** and explicitly
excludes character animation — "cutscenes move the camera and read the world as it
is". There is no re-enactment machinery, no way to animate a figure crossing a
window, and adding one is much larger work.

**So this cutscene cannot depict the murder. It establishes the aftermath.**

Not a compromise so much as the better version:

- Inherently leak-safe. You cannot accidentally reveal a killer you never render.
- Fits the family-friendly tone the win and loss cutscenes also aim for —
  implication rather than depiction.
- **The scene already exists.** The victim starts `NpcState::Dead` and dead NPCs
  already render collapsed.

## What is safe to show, and what is not

| Safe — narrows the field | Unsafe — identifies |
|---|---|
| The scene location (a named zone) | Any living NPC's face, body or palette |
| The hour, as a caption | A gait or walk cycle |
| Weather / time-of-day lighting | Clothing colour on a living figure |
| The victim's body | The number of figures, if matchable to a person |
| A prop or piece of evidence in frame | Anything derived from `MysterySetup.killer` |

The generator reads exactly three fields: `sceneZoneId`, `murderHour`,
`bodyPosition`. **It must not read `killer` at all.**

## Generation, and why a template rather than pure code

Shot composition — pullback distance, hold times, easing — is design work that
will want tuning without a recompile. That is the whole reason the cutscene
system is data-driven. So `cutscenes/opening.cutscene` authors the shots with
poses **relative to the scene centre**, plus caption tokens, and the generator
translates and substitutes.

```cpp
// Takes only what it needs — never the whole MysterySetup — so the killer is
// not even in scope at the call site. THE SIGNATURE IS THE SAFETY MECHANISM:
// a reviewer can see it from the declaration alone.
CutsceneDef buildOpeningCutscene(const CutsceneDef& templateDef,
                                 const std::string& sceneZoneId,
                                 double murderHour,
                                 Vec3 sceneCentre);
```

`${time_label}` uses `clockLabel` from `Journal.hpp`, which already renders
"HH:MM" — so the clue and the journal entries a player compares it against read
identically. That matters, because cross-referencing them is the entire point.

## Out of Scope

- Depicting the murder. Needs character animation.
- New geometry, props or lighting. The camera shows the world as it is.
- Sound.
- Per-zone bespoke camera work. One template, translated.
- The Journal replay UI beyond one entry.

## Implementation Order

1. Token substitution and relative-pose translation + tests. Pure. The leak
   assertion lands here and is nearly trivial given the signature.
2. Author `cutscenes/opening.cutscene`; capture beats with
   `--cutscene opening --frames N shot.png` for `visual-qa`.
3. Play it at match start during the `Intro` phase.
4. Journal replay entry.
5. The leak review pass.

## The leak check — two layers, because one is not enough

**Automated:** a test asserting the output — every caption and field — contains no
roster persona name. Cheap, and it catches a token substitution that interpolates
the wrong thing.

**Human, via `visual-qa`:** capture every beat across several generated matches
with different killers, and confirm no living resident is identifiable in any
frame. **This is the layer that matters**, because the failure mode is visual — a
camera angle that happens to frame the killer walking past is not something a
string assertion can see.

Re-run the human pass whenever the template's camera work changes, not once.

## Acceptance Criteria

- [ ] `relative_to = scene` translates every beat's pose by the scene centre and
      preserves the composition.
- [ ] `${zone_name}` and `${time_label}` substitute with the match's real values,
      and `${time_label}` matches `clockLabel`'s format.
- [ ] An unknown token is left literal and a warning logged — a silent empty
      substitution would hide an authoring mistake.
- [ ] No caption or field contains any roster persona name.
- [ ] `buildOpeningCutscene`'s signature does not accept `MysterySetup`.
- [ ] Beats captured across three matches with three different killers identify
      no living resident. Judged by `visual-qa`.
- [ ] Replaying from the Journal produces beats identical to the first viewing.
- [ ] Skipping the opening still leaves the intro phase ending on schedule.

## Edge Cases

| Situation | Behaviour |
|---|---|
| `cutscenes/opening.cutscene` missing | Logged; the intro phase passes with no cutscene. Never blocks a match starting |
| The scene is an outdoor zone with nothing to frame | Still valid — an empty street at 9:40 is a legitimate establishing shot |
| The translated camera ends up inside a building | Allowed per the cutscene system's rule. Caught by the visual pass, not code |
| **A living NPC wanders into frame** | **The real risk.** Mitigated by shot choice (tight, low angle) and caught by the visual pass. Not solvable in code without hiding NPCs, which would itself be a tell |
| `murderHour` is midnight | `clockLabel` handles wrap |
| The body is not placed when the cutscene runs | Ordering bug — the opening plays after `seedMysteryFacts` and body placement. Assert the sequence |
| Replayed after the killer is caught | Fine. Nothing in it was ever secret |

## Open Questions

1. Should the body be in frame, or implied? In frame is stronger and needs
   nothing new; implied is gentler. A tone call for the visual pass.
2. Does the `Intro` phase need its own duration setting, or reuse Resolution's?
   Probably its own — an opening can afford to be longer than a nightly beat.
3. Would a second, tighter clue beat be worth it — a prop, a dropped item?
   Depends on whether evidence has renderable representation, which it does not
   yet.
