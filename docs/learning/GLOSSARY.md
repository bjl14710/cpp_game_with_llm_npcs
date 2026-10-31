# Silmulator — Glossary

Terms the learner has demonstrably grasped are added here.

| Term | Definition |
|------|------------|
| ConnectionPort | A `QGraphicsEllipseItem` child of a block that holds `port_id`, `parent_block`, and `connected_to` (the peer port, or None). Coloured blue when free, green when connected. |
| WireItem | A `QGraphicsLineItem` in the scene (not a child of any block) that draws a gold line between two ConnectionPorts. ZValue = -1 so blocks render above it. |
| x_frac / y_frac | Fractional coordinates (0.0–1.0) passed to `add_port()` that are multiplied by the block's BLOCK_WIDTH / BLOCK_HEIGHT to position a port in local coordinates. |
| hardware_docs/ | The folder in the repo where vendor PDFs, SCPI command tables, and notes are stored. Not auto-parsed; feeds the custom-block authoring workflow. |
| custom block spec | A JSON file in `silmulator/custom_blocks/` that declares a block's name, colour, port layout, and SCPI command map. Loaded at startup by `discover_specs()`. |
| SIL | Software-in-the-Loop: both the controller logic and the instruments are simulated in software. No real hardware. |
| HIL | Hardware-in-the-Loop: the real embedded controller runs in real time; only the instruments/plant are simulated. |
| CIL | Code-in-the-Loop: actual source files are the artifact under test; SCPI calls extracted from them are replayed against simulated instruments. The Code Input block enables this. |
| SCPI | Standard Commands for Programmable Instruments (IEEE 488.2). A tree-structured text command language used by lab instruments and simulated here. |

## Clock ownership and match pacing (vocab 0027, lesson 0028, issue #155)

- **clock ownership** — which subsystem may WRITE the shared world hour this frame:
  free roam (`WorldState::advanceTime`) or a match (`MatchClock::worldHour`), never both.
- **driven clock** — a clock DERIVED from another system's progress rather than
  accumulated from elapsed time. Contrast a *ticked* clock.
- **value coupling** — consumers depend on reading a shared variable, not on calling
  or being called by its producer. What makes a producer swappable later.
- **single write site** — expressing "exactly one owner" in the code's topology,
  by collapsing mutually exclusive branches into one function.
- **state-as-representation** — storing a fact once in a form that cannot disagree
  with itself (`std::optional<MatchClock>`), rather than a flag beside the object.
- **phase machine** — `Intro → Investigation → Vote → Resolution`, looping to the day
  limit then `Ended`; at most one transition per `advance()` call.
- **Intro phase** — the opening beat, sized to the cutscene budget but not gated by it.
  Holds the clock at `dayStartHour`, unlike every other holding phase.
- **diegetic countdown** — a deadline read from the world, not the HUD: `dayEndHour`
  sits inside the dusk band, so the orange sky IS the vote timer.
- **phase overshoot carry** — leftover time crosses into the next phase rather than
  being dropped, so a long frame delays a boundary instead of losing its event.
- **byte-identical regression check** — proving an untouched path is untouched by
  hashing the same deterministic capture before and after.
- **authored content** — data written by a human into a content file and read by the
  game (`personas/*.persona`, `traits/*.trait`, `banks/*.bank`). Read-only at runtime.
- **destination override** — a nullable field holding a temporary target that outranks
  the authored one at the single point that one is read (`Npc::gatherTarget_`).
- **out-ranked, not suspended** — the authored value still resolves and still reports
  itself; the override only wins the destination. Contrast *suspended*, which must be
  restarted.
- **restore-by-reset** — undo implemented as clearing an override, not replaying a
  saved state. Diagnostic: if restore must know what was there before, you overwrote it.
- **guard bundling** — unrelated early-return conditions sharing one `if`. Splitting
  them is a behaviour change until proven otherwise.
- **incidental behaviour change** — an observable change that falls out of a refactor
  rather than being intended by it.
- **presentation-only system** — changes what is seen, is never consulted by a mechanic;
  its total failure can break nothing but the view.
- **entry-keyed transition handling** — reacting to the state being ENTERED, so routes
  nobody has written yet are still covered. Requires an idempotent handler.
- **authored marker** — a hand-placed reference point preferred over a computed one
  because the computed one is wrong on the real map.
- **geometric centre vs. clear space** — a centre describes bounds, not emptiness;
  `zoneCentre("plaza")` is inside Gus's cart.
- **spacing vs. formation logic** — spacing stops agents stacking; formation assigns
  ordering and re-packs. The plaza ring is spacing, so a dead resident leaves a gap.
- **vacuous test** — passes for a reason unrelated to its claim. A movement test whose
  fixture cannot move always passes.
- **diegetic signal** — information carried by something inside the fiction (the dusk
  light). Free to read, imprecise by nature.
- **HUD signal** — information drawn on the glass in front of the world. Exact, and
  costs a glance away from the game.
- **shared-variable signalling** — a second signal for free because two systems
  already read the same value: `dayEndHour` inside the dusk band makes the sky a
  rendering of the phase clock, not a metaphor for it.
- **stated HUD budget** — a written limit on HUD growth with the consequence of
  exceeding it. HUDs grow by accretion; each step is individually reasonable.
- **integer glyph advance** — raylib's built-in font spaces sizes 14-18 identically,
  so any type hierarchy inside that range does not exist at runtime.
- **single read site** — a value read in exactly one place, so a rule about when it
  applies becomes topology rather than discipline.
- **weak vs strong invariant** — "do not do X" (enforced by readers) vs "there is no
  way to do X" (enforced by structure).
- **trailing default** — a final branch serving both "the last real case" and "a case
  nobody wrote", making a missing case indistinguishable from a handled one.
- **exhaustive switch (no default)** — a switch over an enum with no `default:`, so a
  new enumerator is a compiler warning.
- **derived layout** — spacing computed from the item count against the available
  band, making overflow impossible rather than invisible.
- **latent constant disagreement** — a hardcoded constant that must agree with a
  count nobody re-checks.
- **harness boundary** — the precise line past which the test suite verifies nothing:
  here, everything in `src/app/`.
- **deferred-frame handoff** — acting one frame after a transition so same-frame
  consumers still see the state that produced it.
- **invalid instrument** — apparatus producing plausible numbers about the wrong
  thing. A bug gives a wrong answer you notice; this gives a believable one forever.
- **behaviour export** — crossing a language boundary by having the owning language
  print its result, rather than re-deriving it.
- **hand-synced duplicate** — a copy kept in step by discipline alone.
- **fail closed (measurement boundary)** — refusing output on bad input where the
  product would degrade and continue.
- **zero-byte refusal** — printing no partial result, so a broken fixture cannot be
  mistaken for data.
- **identity check** — asserting a new path is byte-for-byte equal to the existing
  one where they should agree.
- **residue testing** — testing only what an existing harness demonstrably does not
  cover.
- **pointer-into-argument** — a function returning a pointer into a container it was
  passed; the inline call form dangles.
- **side channel** — information reaching an observer through a path nobody designed
  as a communication path (the `[[MOOD:]]` tag).
- **separability** — whether one group's outputs can be told apart from another's.
  Preferred over "never emits X" because the fix is to hide the signal in noise.
- **exploit-derived metric** — a statistic whose shape comes from the attack it
  detects. "Sort faces by hostility" is a rank, so the statistic is a rank.
- **rank permutation test** — p as the fraction of the population scoring at least
  as high as the subject. No distributional assumption, no dependency.
- **total variation distance (TVD)** — 0..1 distance between two distributions;
  0 identical, 1 nothing in common.
- **collapsed statistic** — a summary that reduces a distribution to one number and
  is blind to structure in the other dimensions.
- **cover (hidden-role design)** — innocents behaving enough like the guilty one
  that its behaviour stops being a unique signal. Requires the groups to look alike.
- **undocumented defence** — a protection that works for a reason other than the one
  the design describes.
- **CANNOT EVALUATE** — a third outcome beside pass and fail, for a gate that could
  not run at all.
- **scaffold exit code** — an unimplemented gate returning non-zero so it can never
  be mistaken for a passing one.
- **self-reported n** — sample size printed beside the verdict; a probe that hides
  its own n is worse than no probe.
- **ordering-stable finding** — a conclusion that survives run-to-run noise because
  it depends on rank rather than magnitude.
