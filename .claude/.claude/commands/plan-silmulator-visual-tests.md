---
description: Creates the GitHub issues for adding visual regression testing to the Silmulator GUI. Run this once to queue the work for overnight sessions.
---

Read .claude/skills/github/SKILL.md and .claude/skills/visual-testing/SKILL-visual-testing.md
before doing anything.

---

You are creating GitHub issues for the Silmulator visual regression
testing milestone. Each issue is scoped for one overnight session.
All issues are labelled ready-for-ai and assigned to the repo owner.

```python
import os
from github import Github
from datetime import datetime

gh       = get_github_client()
repo     = get_repo(gh)
username = os.environ["GITHUB_USERNAME"]

# Ensure overnight labels exist
create_overnight_labels(repo)

# Create the milestone
milestone = repo.create_milestone(
    title="Visual Regression Testing — Silmulator GUI",
    description=(
        "Add screenshot-based visual regression tests to the Silmulator. "
        "Each GUI component gets a golden reference image. Tests run "
        "headlessly on EC2 via pytest-xvfb. Failures show pixel diff images."
    )
)
print(f"Created milestone: {milestone.title}")

# ─── Issue specifications ─────────────────────────────────────────────────────
issues = [
    {
        "title": "Add visual testing infrastructure (conftest, golden dir, pytest config)",
        "body": """## What to change
Add the visual regression testing foundation to the Silmulator repo.
No GUI tests yet — just the infrastructure everything else builds on.

## Specifically
1. Create `tests/visual/conftest.py` with the `compare_screenshot()` helper
   (pixel capture via QWidget.grab(), PIL diff against golden, threshold check,
   diff image saved to tests/visual/diffs/ on failure)
2. Create `tests/visual/golden/` directory with a `.gitkeep`
3. Add to `.gitignore`:
   ```
   tests/visual/diffs/
   tests/visual/actual/
   ```
4. Add to `pytest.ini` (or create it):
   ```ini
   [pytest]
   qt_api = pyqt6
   ```
5. Add to `requirements-dev.txt`:
   ```
   pytest-qt
   pytest-xvfb
   Pillow
   numpy
   ```

## How to verify it's done
`pytest tests/visual/ -v` runs without import errors (no tests yet = fine).
`tests/visual/golden/` directory exists and is tracked by git.
`tests/visual/diffs/` is in `.gitignore`.

## Constraints
Do not write any actual GUI tests yet — just the infrastructure.
`compare_screenshot()` must match the interface in the visual-testing skill.
Threshold default is 5 (not 0 — allows minor antialiasing differences).

## Code economy note
Use stdlib only: PIL, numpy, pathlib. No additional testing frameworks.

## Branch name
`feature/issue-N-visual-test-infrastructure`

## Concept for learning materials
"Visual regression testing with golden image comparison"
""",
    },
    {
        "title": "Add visual test: main window opens and matches golden image",
        "body": """## What to change
Add the first real visual test: verify the Silmulator main window opens
and renders correctly, both on Mac (development) and EC2 (overnight runs).

## Specifically
1. Create `tests/visual/test_gui_main_window.py` with:
   - `test_main_window_opens()` — launches MainWindow, waits for it to be
     exposed, calls compare_screenshot(window, "main_window")
   - `test_main_window_title()` — asserts windowTitle() == "Silmulator"
   - `test_main_window_minimum_size()` — asserts window is at least 800x600
2. Generate the golden image on first run (auto-saved when golden absent)
3. Add `tests/visual/golden/main_window.png` to git

## How to verify it's done
`pytest tests/visual/test_gui_main_window.py -v` passes.
`tests/visual/golden/main_window.png` exists and is committed.
Run twice — second run compares against golden and passes.

## Constraints
Use `qtbot.waitExposed(window, timeout=5000)` before screenshotting.
Use `qapp.processEvents()` after waitExposed.
Test must pass headlessly with pytest-xvfb (no DISPLAY env var needed).

## Code economy note
Three tests maximum in this file. Use the compare_screenshot helper from
conftest — do not duplicate screenshot logic.

## Branch name
`feature/issue-N-visual-test-main-window`

## Concept for learning materials
"PyQt6 headless testing with pytest-qt and xvfb virtual display"
""",
    },
    {
        "title": "Add visual tests: hardware block components render correctly",
        "body": """## What to change
Add visual tests for each hardware block widget that can be placed on
the Silmulator canvas (PSU block, oscilloscope block, etc.).

## Specifically
1. Create `tests/visual/test_gui_blocks.py` with one test per block type:
   - `test_psu_block_renders()` → golden: psu_block.png
   - `test_oscilloscope_block_renders()` → golden: oscilloscope_block.png
   - (add more if other block types exist in src/gui/blocks/)
2. Each test: instantiate the block, show it, waitExposed, processEvents,
   compare_screenshot
3. Generate and commit all golden images

## How to verify it's done
`pytest tests/visual/test_gui_blocks.py -v` passes.
One golden image per block type committed to tests/visual/golden/.

## Constraints
Each block test is independent — no shared state between tests.
Use the block's default/initial state for the golden (no configuration).
If a block type doesn't exist yet, skip it with pytest.mark.skip and a note.

## Code economy note
Follow the exact pattern from test_gui_main_window.py — no new helpers.

## Branch name
`feature/issue-N-visual-test-block-components`

## Concept for learning materials
"QWidget.grab() screenshot capture and PIL pixel-level image comparison"
""",
    },
    {
        "title": "Add visual test: drag a block onto the canvas and verify layout",
        "body": """## What to change
Add an interaction test: simulate dragging a PSU block from the device
palette onto the canvas, then verify the resulting canvas state visually.

## Specifically
1. Add to `tests/visual/test_gui_canvas.py`:
   - `test_canvas_empty_state()` → golden: canvas_empty.png
   - `test_canvas_with_psu_block()`:
     - Launch MainWindow
     - Simulate clicking the PSU button in the device palette (qtbot.mouseClick)
     - Wait for block to appear (qtbot.waitUntil)
     - processEvents
     - compare_screenshot(canvas, "canvas_with_psu_block")
2. Generate and commit golden images

## How to verify it's done
`pytest tests/visual/test_gui_canvas.py -v` passes.
Two golden images committed.
The canvas_with_psu_block.png shows a PSU block on the canvas.

## Constraints
Use qtbot.waitUntil with a timeout of 3000ms — not time.sleep().
The test must pass headlessly. If palette/drag API doesn't exist yet,
test the canvas in isolation and leave a comment about what's missing.

## Code economy note
QTest and qtbot are the right tools — no pyautogui or screen coordinates.

## Branch name
`feature/issue-N-visual-test-canvas-interaction`

## Concept for learning materials
"Simulating user interactions in Qt tests with QTest and qtbot"
""",
    },
    {
        "title": "Add overnight session support for visual tests (headless EC2 setup)",
        "body": """## What to change
Ensure the overnight session can run visual tests headlessly on EC2
without any manual xvfb setup. Update the test command and document
the one-time EC2 setup.

## Specifically
1. Verify `pytest-xvfb` is in `requirements-dev.txt`
   (when installed, pytest starts Xvfb automatically — no xvfb-run prefix needed)
2. Update the test command in CLAUDE.md or README to just:
   `pytest tests/ -v`
   (visual tests included automatically via pytest-xvfb)
3. Add `docs/visual-testing.md` explaining:
   - How to update golden images after intentional UI changes
   - How to read diff images when tests fail
   - The EC2 one-time setup command (setup-visual-testing.sh)
4. Add a CI-ready `.env.test` or document the DISPLAY variable is not needed
   when pytest-xvfb is installed

## How to verify it's done
`pytest tests/ -v` runs all tests including visual tests in one command.
No `xvfb-run` prefix needed.
docs/visual-testing.md exists and explains the update-golden workflow.

## Constraints
Do not add a Makefile or separate test runner script — one pytest command only.
Do not require DISPLAY to be manually set.

## Code economy note
pytest-xvfb handles all the xvfb lifecycle. No custom session management.

## Branch name
`feature/issue-N-visual-test-overnight-integration`

## Concept for learning materials
"Headless display servers and virtual framebuffers for CI GUI testing"
""",
    },
]

# ─── Create all issues ────────────────────────────────────────────────────────
created = []
for spec in issues:
    result = create_issue(
        repo,
        title=spec["title"],
        body=spec["body"],
        labels=["ready-for-ai"],
        assignees=[username],
        milestone=milestone
    )
    # Update the branch name with the real issue number
    issue_obj = repo.get_issue(result["number"])
    updated_body = spec["body"].replace(
        "feature/issue-N-",
        f"feature/issue-{result['number']}-"
    )
    issue_obj.edit(body=updated_body)
    created.append(result)
    print(f"  ✅ #{result['number']}: {spec['title']}")

print(f"\nCreated {len(created)} issues in milestone: {milestone.title}")
print("\nTonight's queue:")
queue = list_issues(repo, label="ready-for-ai")
for item in queue:
    print(f"  #{item['number']:3} {item['title']}")
print(f"\nRun: bash ~/scripts/nightly-github.sh silmulator")
```
