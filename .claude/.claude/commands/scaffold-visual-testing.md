---
description: Scaffolds visual regression testing for a PyQt6 project. Creates the skill file, EC2 setup script, conftest.py, directory structure, and a plan document for the first visual tests. Run once per repo.
argument-hint: Optional app/window name to test first e.g. "MainWindow" (blank = MainWindow)
---

You are scaffolding visual regression testing for this PyQt6 project.
Run from the repo root. Creates everything needed to start writing
screenshot tests, then saves a plan for the first batch of tests.

The target widget for the first test is: "$1" (default: MainWindow)

---

## STEP 1 — READ THE PROJECT

```bash
# Understand the GUI structure before generating anything
find src/ -name "*.py" | xargs grep -l "QWidget\|QMainWindow\|QDialog" 2>/dev/null | head -10
find src/ -name "*.py" | xargs grep -l "class.*Window\|class.*Widget\|class.*Block" 2>/dev/null | head -10
cat CLAUDE.md 2>/dev/null | head -40 || true
```

Use what you find to customize the generated files — real import paths,
real class names, real window titles. Do not use placeholder names if
you can find the actual ones.

---

## STEP 2 — CREATE DIRECTORY STRUCTURE

```bash
mkdir -p tests/visual/golden
mkdir -p tests/visual/diffs
mkdir -p tests/visual/actual
mkdir -p .claude/skills/visual-testing

# Add generated dirs to gitignore — golden images ARE committed
grep -qF "tests/visual/diffs/" .gitignore  || echo "tests/visual/diffs/"  >> .gitignore
grep -qF "tests/visual/actual/" .gitignore || echo "tests/visual/actual/" >> .gitignore

# Keep the golden dir tracked even when empty
touch tests/visual/golden/.gitkeep

echo "Directory structure created"
git status --short | grep "tests/\|.gitignore" || true
```

---

## STEP 3 — WRITE THE SKILL FILE

Write `.claude/skills/visual-testing/SKILL-visual-testing.md` with this
exact content:

```markdown
---
description: Use this skill when writing or reviewing visual regression tests for PyQt6 GUI. Covers screenshot capture, golden image comparison, headless EC2 execution, and test structure. Read before writing any GUI test.
---

# Visual Regression Testing Skill (PyQt6)

## Install Prerequisites

pip install pytest-qt pytest-xvfb Pillow numpy

Linux / EC2 system packages (run setup-visual-testing.sh once):
  sudo apt-get install -y xvfb libxkbcommon-x11-0 libxcb-icccm4
  libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0
  libxcb-xinerama0 libxcb-xinput0 libxcb-xfixes0 libxcb-shape0
  libgl1-mesa-dev libglib2.0-0

Add to pytest.ini:
  [pytest]
  qt_api = pyqt6

## Directory Structure

tests/
└── visual/
    ├── conftest.py       shared fixtures and compare_screenshot helper
    ├── golden/           reference images — COMMITTED to git
    ├── diffs/            diff images on failure — gitignored
    └── test_gui_*.py     test files

## The Golden Image Rule

Golden images ARE committed — they are the ground truth.
Diff images are NOT committed — they are failure diagnostics.
Never update a golden without a human reviewing the diff first.
Never commit tests/visual/diffs/ or tests/visual/actual/.

## The compare_screenshot Pattern

def test_something(qapp, qtbot):
    widget = YourWidget()
    qtbot.addWidget(widget)
    widget.show()
    qtbot.waitExposed(widget, timeout=5000)  # ALWAYS wait
    qapp.processEvents()                      # ALWAYS flush
    compare_screenshot(widget, "your_name")   # THEN screenshot

waitExposed + processEvents before EVERY screenshot — skipping either
causes flaky tests from timing issues.

## Thresholds

threshold=0   pixel perfect (icons, logos, fixed bitmaps)
threshold=5   default (handles font antialiasing differences)
threshold=15  loose (components with dynamic content)

Generate golden images ON EC2, not Mac — font rendering differs
between platforms and goldens must match where tests run.

## Running Tests

Mac (development):
  pytest tests/visual/ -v

EC2 / headless (pytest-xvfb handles display automatically):
  pytest tests/ -v

Updating a golden after intentional UI change:
  rm tests/visual/golden/name.png
  pytest tests/visual/test_file.py::test_name -v
  git add tests/visual/golden/name.png
  git commit -m "chore(visual): update golden name after intentional change"

## Error Reference

could not connect to display  → run setup-visual-testing.sh on EC2
xcb platform not found        → install the apt packages above
Golden not found              → first run auto-creates it, then commit it
Size mismatch                 → window size changed, delete golden and regen
RMS too high                  → check tests/visual/diffs/ for the diff image
Flaky timing                  → add waitExposed + processEvents
```

---

## STEP 4 — WRITE conftest.py

Write `tests/visual/conftest.py` with real import paths from Step 1:

```python
# tests/visual/conftest.py
import os
import numpy as np
from pathlib import Path
from PIL import Image, ImageChops
import pytest
from PyQt6.QtWidgets import QApplication

GOLDEN_DIR = Path(__file__).parent / "golden"
DIFF_DIR   = Path(__file__).parent / "diffs"
ACTUAL_DIR = Path(__file__).parent / "actual"

for d in [GOLDEN_DIR, DIFF_DIR, ACTUAL_DIR]:
    d.mkdir(exist_ok=True)

DEFAULT_THRESHOLD = 5


@pytest.fixture(scope="session")
def qapp():
    """Single QApplication for the whole test session."""
    app = QApplication.instance() or QApplication([])
    yield app


def compare_screenshot(widget, name: str,
                       threshold: int = DEFAULT_THRESHOLD):
    """
    Capture a screenshot of widget and compare to the golden reference.
    On first run (no golden), saves the screenshot as the new golden.
    On subsequent runs, compares and fails if RMS diff > threshold.

    Always call qtbot.waitExposed(widget) and qapp.processEvents()
    before calling this.
    """
    golden_path = GOLDEN_DIR / f"{name}.png"
    diff_path   = DIFF_DIR   / f"{name}_diff.png"
    actual_path = ACTUAL_DIR / f"{name}_actual.png"

    # Capture screenshot via Qt
    pixmap = widget.grab()
    qimage = pixmap.toImage()

    # Convert QImage (BGRA) to PIL RGB
    w, h = qimage.width(), qimage.height()
    ptr  = qimage.bits()
    ptr.setsize(h * w * 4)
    arr    = np.frombuffer(ptr, dtype=np.uint8).reshape((h, w, 4))
    actual = Image.fromarray(arr[:, :, [2, 1, 0, 3]], mode="RGBA").convert("RGB")

    # First run — save as golden
    if not golden_path.exists():
        actual.save(golden_path)
        print(f"Golden created: {golden_path} — commit this file")
        return

    # Compare
    golden = Image.open(golden_path).convert("RGB")

    assert actual.size == golden.size, (
        f"Size changed: expected {golden.size}, got {actual.size}.\n"
        f"Delete {golden_path} and re-run to regenerate."
    )

    diff    = ImageChops.difference(actual, golden)
    rms     = float(np.sqrt(np.mean(np.array(diff, dtype=float) ** 2)))

    if rms > threshold:
        actual.save(actual_path)
        amplified = Image.fromarray(
            np.clip(np.array(diff) * 10, 0, 255).astype(np.uint8)
        )
        amplified.save(diff_path)
        raise AssertionError(
            f"Visual regression '{name}': RMS={rms:.2f} > threshold={threshold}\n"
            f"  Golden:  {golden_path}\n"
            f"  Actual:  {actual_path}\n"
            f"  Diff:    {diff_path}  (10x amplified)\n"
            f"If intentional: cp {actual_path} {golden_path} && git add && git commit"
        )

    print(f"Visual OK: '{name}' RMS={rms:.2f}")
```

---

## STEP 5 — WRITE THE SETUP SCRIPT

Write `scripts/setup-visual-testing.sh`:

```bash
#!/bin/bash
# Run once on EC2 to install visual testing prerequisites
set -euo pipefail

echo "Installing visual regression testing prerequisites..."

sudo apt-get update -qq
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
  libglib2.0-0 \
  libegl1

pip install pytest-qt pytest-xvfb Pillow numpy

echo ""
echo "Smoke test..."
python3 - << 'PYEOF'
import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from PyQt6.QtWidgets import QApplication, QWidget
app = QApplication.instance() or QApplication([])
w = QWidget()
w.resize(400, 300)
w.show()
px = w.grab()
print(f"OK — captured {px.width()}x{px.height()} screenshot headlessly")
PYEOF

echo ""
echo "Done. Next steps:"
echo "  1. Add [pytest] qt_api = pyqt6 to pytest.ini"
echo "  2. Run: pytest tests/visual/ -v   (generates golden images)"
echo "  3. git add tests/visual/golden/ && git commit -m 'test(visual): add golden images'"
```

```bash
chmod +x scripts/setup-visual-testing.sh
```

---

## STEP 6 — UPDATE pytest.ini

Check if pytest.ini exists and add the qt_api setting:

```bash
if [[ -f "pytest.ini" ]]; then
    grep -q "qt_api" pytest.ini || echo "qt_api = pyqt6" >> pytest.ini
else
    cat > pytest.ini << 'EOF'
[pytest]
qt_api = pyqt6
EOF
fi
echo "pytest.ini updated"
```

---

## STEP 7 — WRITE THE PLAN DOCUMENT

Using the real class names found in Step 1, write a plan to
`.claude/plans/visual-testing.md`:

```markdown
# Plan: Visual Regression Testing
Date: [date]
Status: READY FOR IMPLEMENTATION

## Goal
Every significant GUI component has a screenshot test that catches
unintended visual changes before they reach main.

## Out of Scope (this version)
- Pixel-perfect matching for text-heavy widgets (use threshold=5)
- Video or animation capture
- Cross-browser testing (Qt only)

## Affected Areas
- tests/visual/conftest.py        shared fixtures (created by scaffold)
- tests/visual/golden/            reference images (generated on first run)
- tests/visual/test_gui_*.py      test files (to be written)
- scripts/setup-visual-testing.sh EC2 one-time setup (created by scaffold)
- pytest.ini                      qt_api = pyqt6 (updated by scaffold)

## Implementation Order

1. Run scripts/setup-visual-testing.sh on EC2 (one-time)
2. Write tests/visual/test_gui_main_window.py
   - test_[main_window]_opens() → golden: [main_window].png
   - test_[main_window]_title() → asserts windowTitle()
3. Generate and commit golden images
4. Write tests per GUI component found in Step 1
5. Add visual tests to overnight session test command

## Acceptance Criteria
- [ ] pytest tests/visual/ -v passes on EC2 without xvfb-run prefix
- [ ] pytest tests/visual/ -v passes on Mac
- [ ] All golden images committed to tests/visual/golden/
- [ ] tests/visual/diffs/ is in .gitignore
- [ ] One test per major GUI component

## Suggested GitHub Issues
1. Write visual test for [MainWindow or first widget found]
2. Write visual tests for hardware block components
3. Write visual test for canvas interaction (drag and drop)
4. Wire visual tests into overnight session
```

---

## STEP 8 — SUMMARY

After writing all files, print:

```
Visual testing scaffold complete.

Files created:
  tests/visual/conftest.py                  ← shared test fixtures
  tests/visual/golden/.gitkeep              ← commit this dir
  scripts/setup-visual-testing.sh           ← run once on EC2
  .claude/skills/visual-testing/            ← skill for overnight session
    SKILL-visual-testing.md
  .claude/plans/visual-testing.md           ← implementation plan

Next steps:
  1. On EC2: bash scripts/setup-visual-testing.sh
  2. Write your first test in tests/visual/test_gui_main_window.py
     (see the plan at .claude/plans/visual-testing.md)
  3. Run: pytest tests/visual/ -v
     (first run auto-creates golden images — then commit them)
  4. Create GitHub issues from the plan:
     /plan-github "visual regression testing"

Golden images must be generated on EC2 (not Mac) so font rendering
matches where the overnight session runs tests.
```
