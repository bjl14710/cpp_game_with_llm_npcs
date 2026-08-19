---
description: Use this skill when writing or reviewing visual regression tests for the Silmulator PyQt6 GUI. Covers screenshot capture, golden image comparison, headless EC2 execution, and test structure. Read before writing any GUI test.
---

# Visual Regression Testing Skill (PyQt6 / Silmulator)

This skill covers screenshot-based visual regression testing for the
Silmulator's PyQt6 GUI. Read it fully before writing any GUI test.

The goal: every significant GUI component has a test that:
1. Launches the widget headlessly
2. Captures a screenshot
3. Compares it to a golden reference image
4. Fails if the diff exceeds a threshold

---

## Install Prerequisites

```bash
# Python packages
pip install pytest-qt pytest-xvfb Pillow

# Linux / EC2 system packages (run once)
sudo apt-get update
sudo apt-get install -y \
  xvfb \
  libxkbcommon-x11-0 \
  libxcb-icccm4 \
  libxcb-image0 \
  libxcb-keysyms1 \
  libxcb-randr0 \
  libxcb-render-util0 \
  libxcb-xinerama0 \
  libxcb-xinput0 \
  libxcb-xfixes0 \
  libxcb-shape0 \
  libgl1-mesa-dev \
  libglib2.0-0
```

Add to `pytest.ini` or `pyproject.toml`:
```ini
[pytest]
qt_api = pyqt6
```

---

## Directory Structure

```
tests/
└── visual/
    ├── conftest.py            ← shared fixtures and helpers
    ├── golden/                ← reference images (COMMITTED to git)
    │   ├── main_window.png
    │   ├── psu_block.png
    │   └── oscilloscope_block.png
    ├── diffs/                 ← diff images on failure (gitignored)
    └── test_gui_*.py          ← test files
```

Add to `.gitignore`:
```
tests/visual/diffs/
tests/visual/actual/
```

Golden images ARE committed — they are the ground truth.
Diff images are NOT committed — they are failure diagnostics.

---

## conftest.py — Shared Fixtures

```python
# tests/visual/conftest.py
import os
import pytest
from pathlib import Path
from PIL import Image, ImageChops
from PyQt6.QtWidgets import QApplication

GOLDEN_DIR = Path(__file__).parent / "golden"
DIFF_DIR   = Path(__file__).parent / "diffs"
DIFF_DIR.mkdir(exist_ok=True)

# Default pixel difference threshold (0-255 per channel, RMS across all pixels)
# 0 = pixel perfect, 5 = allows minor antialiasing differences
DEFAULT_THRESHOLD = 5


@pytest.fixture(scope="session")
def qapp():
    """Single QApplication instance for the whole test session."""
    app = QApplication.instance() or QApplication([])
    yield app


def compare_screenshot(widget, name: str,
                       threshold: int = DEFAULT_THRESHOLD,
                       update_golden: bool = False):
    """
    Capture a screenshot of widget and compare to the golden reference.

    Args:
        widget:        any QWidget — the thing to screenshot
        name:          filename stem, e.g. "main_window" → golden/main_window.png
        threshold:     max allowed RMS pixel difference (0=pixel perfect, 5=default)
        update_golden: if True, save this screenshot as the new golden image
                       Use only to intentionally update references.

    Raises:
        AssertionError: if pixel difference exceeds threshold

    Example:
        def test_main_window_opens(qapp, qtbot):
            window = MainWindow()
            qtbot.addWidget(window)
            window.show()
            qtbot.waitExposed(window)
            compare_screenshot(window, "main_window")
    """
    golden_path = GOLDEN_DIR / f"{name}.png"
    diff_path   = DIFF_DIR   / f"{name}_diff.png"
    actual_path = DIFF_DIR   / f"{name}_actual.png"

    # Capture the screenshot
    pixmap  = widget.grab()
    qimage  = pixmap.toImage()

    # Convert QImage → PIL Image
    width, height = qimage.width(), qimage.height()
    ptr    = qimage.bits()
    ptr.setsize(height * width * 4)
    import numpy as np
    arr    = np.frombuffer(ptr, dtype=np.uint8).reshape((height, width, 4))
    # Qt stores BGRA, PIL wants RGBA
    actual = Image.fromarray(arr[:, :, [2, 1, 0, 3]], mode="RGBA").convert("RGB")

    # Update golden mode — save and exit
    if update_golden or not golden_path.exists():
        GOLDEN_DIR.mkdir(exist_ok=True)
        actual.save(golden_path)
        print(f"Golden image saved: {golden_path}")
        return

    # Load golden reference
    golden = Image.open(golden_path).convert("RGB")

    # Size must match exactly
    assert actual.size == golden.size, (
        f"Screenshot size changed: expected {golden.size}, got {actual.size}.\n"
        f"If intentional, delete {golden_path} and re-run to regenerate."
    )

    # Pixel diff
    diff   = ImageChops.difference(actual, golden)
    import numpy as np
    diff_arr = np.array(diff, dtype=float)
    rms    = float(np.sqrt(np.mean(diff_arr ** 2)))

    if rms > threshold:
        # Save diagnostics
        actual.save(actual_path)
        # Amplify diff for visibility (multiply by 10)
        amplified = Image.fromarray(
            np.clip(np.array(diff) * 10, 0, 255).astype(np.uint8)
        )
        amplified.save(diff_path)

        raise AssertionError(
            f"Visual regression: '{name}' pixel RMS={rms:.2f} > threshold={threshold}\n"
            f"  Golden:  {golden_path}\n"
            f"  Actual:  {actual_path}\n"
            f"  Diff:    {diff_path}  (amplified 10×)\n"
            f"If this change is intentional:\n"
            f"  1. Review {actual_path}\n"
            f"  2. cp {actual_path} {golden_path}\n"
            f"  3. git add {golden_path} && git commit -m 'chore(visual): update golden {name}'"
        )

    print(f"Visual OK: '{name}' RMS={rms:.2f} (threshold={threshold})")
```

---

## Writing a Visual Test

```python
# tests/visual/test_gui_main_window.py
import pytest
from PyQt6.QtCore import Qt
from your_app.main_window import MainWindow
from .conftest import compare_screenshot


def test_main_window_opens(qapp, qtbot):
    """Main window renders without crashing and matches golden image."""
    window = MainWindow()
    qtbot.addWidget(window)
    window.show()

    # Wait until the window is actually visible
    qtbot.waitExposed(window, timeout=5000)

    # Process pending events so layout is complete
    qapp.processEvents()

    compare_screenshot(window, "main_window")


def test_main_window_title(qapp, qtbot):
    """Window title is correct."""
    window = MainWindow()
    qtbot.addWidget(window)
    assert window.windowTitle() == "Silmulator"


def test_psu_block_renders(qapp, qtbot):
    """PSU hardware block renders correctly on canvas."""
    from your_app.gui.blocks.psu_block import PSUBlock
    block = PSUBlock()
    qtbot.addWidget(block)
    block.show()
    qtbot.waitExposed(block, timeout=3000)
    qapp.processEvents()
    compare_screenshot(block, "psu_block")


def test_canvas_empty_state(qapp, qtbot):
    """Empty canvas shows the expected placeholder."""
    from your_app.gui.canvas import Canvas
    canvas = Canvas()
    qtbot.addWidget(canvas)
    canvas.show()
    qtbot.waitExposed(canvas, timeout=3000)
    qapp.processEvents()
    compare_screenshot(canvas, "canvas_empty")
```

---

## Simulating User Interactions

```python
from PyQt6.QtCore import Qt, QPoint
from PyQt6.QtTest import QTest

def test_block_drag_to_canvas(qapp, qtbot):
    """Dragging a PSU block onto the canvas adds it correctly."""
    from your_app.main_window import MainWindow
    window = MainWindow()
    qtbot.addWidget(window)
    window.show()
    qtbot.waitExposed(window)

    # Simulate clicking the PSU button in the device palette
    palette = window.device_palette
    psu_button = palette.find_button("PSU")
    qtbot.mouseClick(psu_button, Qt.MouseButton.LeftButton)

    # Wait for the block to appear on canvas
    qtbot.waitUntil(
        lambda: window.canvas.block_count() == 1,
        timeout=3000
    )

    qapp.processEvents()
    compare_screenshot(window, "canvas_with_psu_block")
```

---

## Running Tests

### Mac (development — real display):
```bash
pytest tests/visual/ -v
```

### EC2 / headless (no display):
```bash
# Option 1: xvfb-run wraps everything (simplest)
xvfb-run --auto-servernum --server-args="-screen 0 1920x1080x24" \
  pytest tests/visual/ -v

# Option 2: offscreen platform (no xvfb needed, but no real rendering)
QT_QPA_PLATFORM=offscreen pytest tests/visual/ -v

# Option 3: pytest-xvfb handles it automatically (install once, works everywhere)
pip install pytest-xvfb
pytest tests/visual/ -v   # xvfb started automatically
```

Recommended for overnight sessions: **pytest-xvfb** — install once, no
`xvfb-run` prefix needed, works automatically in the pytest session.

### Updating golden images after an intentional UI change:
```bash
# Delete the specific golden and re-run to regenerate
rm tests/visual/golden/main_window.png
pytest tests/visual/test_gui_main_window.py::test_main_window_opens -v

# Or update all goldens at once (be careful — review before committing)
UPDATE_GOLDEN=1 pytest tests/visual/ -v
```

In `conftest.py`, check this env var:
```python
update_golden = os.environ.get("UPDATE_GOLDEN", "0") == "1"
```

---

## Overnight Session Integration

The overnight session test command for Silmulator becomes:

```bash
# Run all tests including visual
pip install pytest-xvfb  # ensures headless display is available
pytest tests/ -v          # visual tests run automatically via pytest-xvfb
```

No `xvfb-run` prefix needed when pytest-xvfb is installed.

For the overnight session's CLAUDE.md addition:
```
## Visual Regression Tests
- Golden reference images live in tests/visual/golden/ — COMMIT these
- Diff images live in tests/visual/diffs/ — NEVER commit these
- If a visual test fails: check tests/visual/diffs/ for the diff image
- If a UI change is intentional: delete the golden, re-run, commit the new one
- Never commit a golden update without a human reviewing the diff first
```

---

## What Makes a Good Visual Test

**Test whole components, not implementation details:**
```python
# Good — tests the full rendered widget
compare_screenshot(window, "main_window")

# Fragile — tests a pixel coordinate that may shift with font changes
assert qimage.pixelColor(100, 50).red() == 255
```

**Use `waitExposed` and `processEvents` before screenshotting:**
```python
window.show()
qtbot.waitExposed(window, timeout=5000)  # wait for Qt to actually display it
qapp.processEvents()                      # flush pending layout/paint events
compare_screenshot(window, "main_window") # NOW screenshot
```

**Keep threshold appropriate to the component:**
- `threshold=0` — pixel perfect (use for icons, logos, fixed bitmaps)
- `threshold=5` — default (handles minor font rendering differences across OS)
- `threshold=15` — loose (use for components with dynamic content like timestamps)

**Name goldens clearly:**
```
main_window.png              ← what it is
canvas_empty.png             ← what state
canvas_with_psu_block.png    ← what state
psu_block_output_enabled.png ← component + state
```

---

## Threshold and Flakiness

Visual tests can be flaky due to:
- Font rendering differences between Mac and EC2 Linux
- Antialiasing differences between platforms
- Timing issues (widget not fully rendered when screenshot taken)

Solutions:
- Use `threshold=5` not `threshold=0` for text-heavy widgets
- Always `waitExposed` + `processEvents` before screenshotting
- Generate goldens ON the platform tests run on (EC2, not Mac)
  so font rendering matches exactly
- For components with dynamic content (timestamps, counters):
  mock or freeze the dynamic value before screenshotting

---

## Error Reference

| Error | Cause | Fix |
|-------|-------|-----|
| `could not connect to display` | No X server on EC2 | Install pytest-xvfb or use `xvfb-run` |
| `xcb platform not found` | Missing Qt XCB libraries | Run the apt-get install block above |
| Golden not found | First run on new test | Run once — golden is auto-created |
| Size mismatch | Window size changed | Intentional? Delete golden and regenerate |
| RMS too high | Visual diff detected | Check tests/visual/diffs/ for the diff image |
| Flaky timing | Screenshot too early | Add `qtbot.waitExposed` + `processEvents` |
