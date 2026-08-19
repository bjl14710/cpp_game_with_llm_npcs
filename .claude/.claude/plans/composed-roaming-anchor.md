# Fix hardware block connections not working / not visible in the GUI

## Context

User report: wires between hardware blocks don't get created, the "first entry point"
(first-added block's ports) is not connecting, and ports/wires are not showing in the GUI.
Symptoms confirmed: failure during drag, on release, after save/reopen, and ports not visible —
across all block types.

Diagnosis (all root causes identified by reading the code; the existing
`tests/test_canvas_connections.py` suite bypasses the real Qt mouse path, which is why it
passes while the GUI misbehaves):

1. **Rubber-band hijacks the wire drag.** `CanvasScene.mousePressEvent` /
   `mouseReleaseEvent` (Silmulator/silmulator/gui/canvas.py:167-194) handle the port click
   but never call `event.accept()`. `QGraphicsView` is in `RubberBandDrag` mode
   (canvas.py:304); an unaccepted left-press makes the view start a rubber-band selection
   that draws over the drag, churns the scene selection on every move, and fights the
   dashed temp wire → "no wire shows while dragging".

2. **New blocks stack on top of each other, burying the first block's ports.**
   - Toolbar buttons add every block at fixed `QPointF(200, 150)` (main_window.py:246).
   - Add-menu adds every block at fixed `QPointF(100, 100)` (main_window.py:215).
   - Library panel spaces blocks `260`px apart (main_window.py:72-75) but blocks are up to
     300px wide (OscilloscopeBlock) / 280 (MSP430) → guaranteed overlap.
   Newer blocks render above older ones, so the first block's edge ports become invisible /
   unclickable, and wires (z = -1, intentionally below blocks) are hidden under the overlap
   → "first entry point not connecting / not showing".

3. **Save → reopen silently drops Function Generator blocks and all their wires.**
   `FunctionGeneratorBlock.__init__` passes device_type `"Function Gen"`
   (function_generator_block.py:35) but the registry key is `"Function Generator"`
   (canvas.py:31). `to_dict` stores `device_type`; `from_dict` → `add_block("Function Gen")`
   → `BLOCK_REGISTRY.get(...)` → `None` → block skipped, and every wire referencing it is
   dropped (canvas.py:279-294).

4. **File → New leaves dangling block/wire references.** `MainWindow._new_canvas`
   (main_window.py:277) calls `QGraphicsScene.clear()`, which deletes the C++ items but
   leaves `CanvasScene._blocks` / `_wires` populated → stale counts, and the next save /
   reset-all touches deleted items (RuntimeError). Related to the save/reopen symptom.

## Changes

### 1. `Silmulator/silmulator/gui/canvas.py`
- `mousePressEvent`: when a port is found, call `event.accept()` before returning.
- `mouseReleaseEvent`: when finishing a wire drag, call `event.accept()` before returning.
- `mouseMoveEvent`: while `self._connecting`, call `event.accept()` before the early return.
- Override `clear()` on `CanvasScene` to call `super().clear()` and reset `_blocks`,
  `_wires`, `_connecting`, `_temp_wire` (and have `from_dict` use it instead of its own
  partial reset).
- Add `CanvasScene.find_free_position(block_cls) -> QPointF`: scan grid slots
  (left-to-right, top-to-bottom, snapped to `GRID_SIZE`) and return the first position
  where the candidate rect (block width/height + a margin for edge ports, ~20px) does not
  intersect any existing block's scene rect. Allow `add_block(device_type, pos=None)` to
  use it when no position is given.

### 2. `Silmulator/silmulator/gui/main_window.py`
- `BlockLibraryPanel._add_block`, toolbar button handler, and Add-menu handler: stop
  passing fixed/overlapping positions — call `canvas.add_block(name)` and let
  `find_free_position` place the block.
- `_new_canvas`: rely on the fixed `CanvasScene.clear()` (no other change needed).

### 3. `Silmulator/silmulator/gui/blocks/function_generator_block.py`
- `super().__init__(FunctionGenerator(), "Function Generator")` — must equal the
  `BLOCK_REGISTRY` key so save/load round-trips.

## Tests (`Silmulator/tests/`)

- `test_canvas_connections.py` additions:
  - **End-to-end mouse test through the real Qt path** using `PyQt6.QtTest.QTest` on a
    `CanvasView` viewport (offscreen platform already forced in conftest): add two blocks,
    press on src port, move, release on dst port; assert a `WireItem` exists and no
    rubber-band selection replaced it. This is the regression test for the missing
    `event.accept()` — it fails the current code via the rubber-band path where the
    synthetic-call tests pass.
- `test_block_creation.py` additions:
  - Parametrized over `BLOCK_REGISTRY`: `block.device_type` must be a key of
    `BLOCK_REGISTRY` (catches the "Function Gen" mismatch for every current/future block).
  - Auto-placement: adding N blocks with no position produces pairwise non-intersecting
    scene rects.
  - `clear()` resets state: after `scene.clear()`, `scene.blocks == []` and
    `to_dict() == {"blocks": [], "wires": []}`.
- New round-trip test: build a scene with one of each block type + wires, `to_dict` →
  fresh scene `from_dict` → assert same block types and same wire count (catches #3).

## Verification

1. `cd Silmulator && QT_QPA_PLATFORM=offscreen python -m pytest tests/ -q` — full suite
   (288 existing tests) plus new tests green. (Resolve the python/uv toolchain first —
   `python` was not on PATH; use the project `.venv` or `uv run`.)
2. Confirm the new QTest end-to-end test fails on the unfixed code (run it before applying
   the canvas.py fix) — proves it covers the real bug.
3. Offscreen smoke script: construct `MainWindow`, add three blocks via the same handlers
   the toolbar uses, assert non-overlapping positions, save to dict, reload, assert all
   blocks/wires present.
4. Per CLAUDE.md: run the `reviewer` subagent before committing; commit as
   `fix(gui): ...` on the `fix_hardware_connectors` branch.

## Out of scope (noted for later, per "don't fix unrelated")
- GNU-Radio-style click-then-click connecting (current model is press-drag-release only).
- Input/output port direction enforcement (already in the project TODO list).
