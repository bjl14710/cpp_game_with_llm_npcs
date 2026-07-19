---
description: Discovers the GUI structure of the current project — framework, components, windows, layouts, and testability — then writes a local .claude/skills/gui-map/SKILL.md that all future sessions can read. Run once on a new project, then /update-gui-map after adding components.
argument-hint: Optional focus area e.g. "src/gui" (blank = scan entire project)
---

You are mapping the GUI structure of this project. Read everything
first, then write a local skill file that captures what you find.
This is a discovery and documentation task — no code changes.

The output is .claude/skills/gui-map/SKILL.md — a project-specific
reference that future overnight sessions read so they already know
the GUI layout without re-scanning everything from scratch.

---

## STEP 1 — DETECT THE GUI FRAMEWORK

```bash
# Check for framework indicators
echo "=== Python GUI ==="
grep -rn "PyQt6\|PySide6\|tkinter\|wxPython\|kivy\|pygame" \
  . --include="*.py" 2>/dev/null | head -5

echo "=== Web/JS GUI ==="
cat package.json 2>/dev/null | python3 -m json.tool | \
  grep -i "react\|vue\|svelte\|angular\|electron" || true
ls src/ | grep -i "component\|view\|page\|widget" 2>/dev/null || true

echo "=== C++ GUI ==="
grep -rn "QWidget\|QMainWindow\|SDL\|SFML\|ImGui\|wxWidgets" \
  . --include="*.cpp" --include="*.h" 2>/dev/null | head -5

echo "=== Game Engine ==="
ls -la | grep -i "unity\|godot\|unreal\|project.godot\|*.unity" 2>/dev/null || true
find . -name "*.unity" -o -name "project.godot" 2>/dev/null | head -3

echo "=== Config files ==="
ls *.json *.toml *.cfg *.ini 2>/dev/null | head -10
cat CLAUDE.md 2>/dev/null | grep -i "stack\|gui\|framework" || true
```

Determine the framework from the output above. If multiple frameworks
are present (e.g. Electron = web inside a native shell), note all of them.

---

## STEP 2 — SCAN ALL GUI COMPONENTS

Based on the detected framework, scan for components:

### PyQt6 / PySide6
```bash
# Find all widget classes
grep -rn "class.*QWidget\|class.*QMainWindow\|class.*QDialog\|class.*QFrame" \
  . --include="*.py" 2>/dev/null

# Find all .ui files (Qt Designer)
find . -name "*.ui" 2>/dev/null

# Find all signal/slot connections
grep -rn "\.connect\|pyqtSignal\|Signal(" \
  . --include="*.py" 2>/dev/null | head -20
```

### React / Vue / Web
```bash
# Find all components
find src/ -name "*.jsx" -o -name "*.tsx" -o -name "*.vue" 2>/dev/null
find src/ -name "*.jsx" -exec grep -l "export default\|export function" {} \;

# Find routing
grep -rn "Route\|router\|<Router" src/ 2>/dev/null | head -10

# Find state management
grep -rn "useState\|useReducer\|vuex\|pinia\|redux" src/ 2>/dev/null | head -10
```

### C++ / Game GUI
```bash
# Find window/scene classes
grep -rn "class.*Window\|class.*Scene\|class.*Screen\|class.*Panel" \
  . --include="*.h" --include="*.cpp" 2>/dev/null

# Find render/draw functions
grep -rn "void.*draw\|void.*render\|void.*paint" \
  . --include="*.cpp" 2>/dev/null | head -10

# Find ImGui calls if used
grep -rn "ImGui::" . --include="*.cpp" 2>/dev/null | head -10
```

### Electron
```bash
# Find main and renderer processes
find . -name "main.js" -o -name "main.ts" -o -name "preload.js" 2>/dev/null
ls src/renderer/ src/main/ 2>/dev/null || true
```

---

## STEP 3 — MAP THE COMPONENT HIERARCHY

For each component found, record:
- Class/component name
- File path
- Parent class or base component
- What it renders or displays
- Key signals/events it emits or handles
- Whether it has existing tests
- Whether it has a golden reference image

Build a hierarchy showing how components relate:

```
MainWindow (src/gui/main_window.py)
├── MenuBar (src/gui/menu_bar.py)
├── Canvas (src/gui/canvas.py)
│   ├── PSUBlock (src/gui/blocks/psu_block.py)
│   ├── OscilloscopeBlock (src/gui/blocks/oscilloscope_block.py)
│   └── [more blocks...]
├── DevicePalette (src/gui/palette.py)
└── HUD (src/gui/hud/)
    ├── WeaponHUD (src/gui/hud/weapon_hud.py)
    └── NPCIndicator (src/gui/hud/npc_indicator.py)
```

---

## STEP 4 — DETECT TESTING APPROACH

Based on the framework, determine how GUI testing should work:

### PyQt6 / PySide6
```bash
# Check if visual testing infrastructure exists
ls tests/visual/ 2>/dev/null
ls tests/visual/golden/ 2>/dev/null
pip show pytest-qt pytest-xvfb 2>/dev/null | grep "Name:"
```

Testing approach: `QWidget.grab()` + PIL pixel diff + pytest-qt + pytest-xvfb
Golden images: `tests/visual/golden/*.png`
Run headless: `pytest tests/visual/ -v` (pytest-xvfb handles display)

### React / Vue / Web
```bash
# Check for Playwright or Cypress
ls playwright.config.* cypress.config.* 2>/dev/null || true
pip show playwright 2>/dev/null || npm list playwright 2>/dev/null || true
```

Testing approach: Playwright screenshots + pixel comparison
Golden images: `tests/visual/golden/*.png`

### C++ game with custom renderer
Testing approach: custom screenshot API + file comparison
Check if the engine has a built-in screenshot function.

### Game Engine (Unity/Godot)
Unity: `ScreenCapture.CaptureScreenshot()` in PlayMode tests
Godot: `get_viewport().get_texture().get_data()` + save as PNG

---

## STEP 5 — ASSESS CURRENT TEST COVERAGE

```bash
# Find existing GUI tests
find tests/ -name "test_gui*" -o -name "test_*window*" \
  -o -name "test_*widget*" -o -name "test_*component*" 2>/dev/null

# Find existing golden images
find . -name "*.png" -path "*/golden/*" 2>/dev/null

# Find components with NO test
echo "Components found vs tests found:"
```

For each component found in Step 2, check whether a test exists.
Mark each as:
- ✅ tested — golden image exists and test file references it
- ⚠️ partial — test file exists but no golden image
- ❌ untested — no test at all

---

## STEP 6 — WRITE THE GUI MAP SKILL

Create `.claude/skills/gui-map/SKILL.md` with everything discovered.
This is the persistent local knowledge base for this project's GUI.

```markdown
---
description: Project-specific GUI map for [PROJECT NAME]. Read this before
writing any GUI code or GUI tests. Updated by /update-gui-map when new
components are added. Last updated: [date].
---

# GUI Map — [Project Name]

## Framework
[e.g. PyQt6 6.x / React 18 / C++ with SDL2 + ImGui / Godot 4.x]

## Testing Approach
[exact testing method for this project's framework]

Test command: [e.g. pytest tests/visual/ -v]
Golden images: [path e.g. tests/visual/golden/]
Headless: [yes/no and how e.g. pytest-xvfb handles it automatically]

## Component Hierarchy
[the tree from Step 3 — real class names, real file paths]

## Component Status

| Component | File | Has Test | Has Golden | Notes |
|-----------|------|----------|------------|-------|
| MainWindow | src/gui/main_window.py | ✅ | ✅ | main_window.png |
| Canvas | src/gui/canvas.py | ⚠️ | ❌ | test stub exists |
| PSUBlock | src/gui/blocks/psu_block.py | ❌ | ❌ | not tested yet |

## Key Signals and Events
[important signal/slot or event connections between components]

## State Management
[how application state flows through the GUI — where state lives,
how components communicate changes]

## Known Issues or Fragile Areas
[components that are hard to test, timing issues, platform differences]

## What Needs Tests Next
[ordered list of untested components by priority]
1. [component] — [why it's high priority]
2. ...

## How to Add a New Component Test
[step-by-step specific to THIS project's framework and test setup]

## Golden Image Naming Convention
[e.g. component_name_state.png → psu_block_output_enabled.png]

## Platform Notes
[any differences between Mac development and EC2 CI testing]
[e.g. "generate goldens on EC2 — font rendering differs from Mac"]
```

---

## STEP 7 — SUMMARY

After writing the skill file, print:

```
GUI map written to .claude/skills/gui-map/SKILL.md

Framework detected: [framework]
Components found: [N]
  ✅ Tested: [N]
  ⚠️  Partial: [N]
  ❌ Untested: [N]

Highest priority for testing:
  1. [component] — [reason]
  2. [component] — [reason]
  3. [component] — [reason]

Next steps:
  /scaffold visual-testing      set up visual test infrastructure
  /plan-github "gui tests"      create issues for untested components
  /update-gui-map               run after adding new components

The GUI map is now available to all overnight sessions.
They will read it before writing any GUI code or tests.
```

---

## KEEPING IT CURRENT

The GUI map is only useful if it stays current. Two ways to update it:

**Manual:** Run `/update-gui-map` after adding a new component. This is
a lighter version of this command — it reads the existing map, finds
what's new, and appends rather than rebuilding from scratch.

**Automatic:** The overnight session can call `/update-gui-map` after
completing any GUI-related issue. Add this to CLAUDE.md:

```markdown
## GUI Map
After any issue that adds or modifies GUI components, run:
  /update-gui-map
This keeps .claude/skills/gui-map/SKILL.md current for future sessions.
```

The map file should be committed to git — it's project knowledge,
not a generated artifact. Other developers (and future Claude sessions)
benefit from it being in version control.
