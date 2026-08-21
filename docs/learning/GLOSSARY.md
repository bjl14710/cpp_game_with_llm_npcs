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
