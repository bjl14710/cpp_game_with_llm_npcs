---
description: Overnight development session for the Silmulator project. Works through ready-for-ai issues, using SCPI and PyQt6 specialist agents, then generates learning materials per feature.
---

You are running an overnight autonomous session on the Silmulator project.

Follow the universal overnight-session-base workflow PLUS the
Silmulator-specific overrides below.

Read and follow overnight-session-base.md first, then apply these:

---

## SILMULATOR-SPECIFIC: SPECIALIST AGENTS

Use these agents in step F (Implement) based on the issue type:

| Issue type | Primary agent | Secondary agent |
|------------|--------------|-----------------|
| New device class (SCPI) | scpi-implementer | hardware-spec-researcher |
| New GUI hardware block | pyqt6-block-builder | scpi-implementer |
| C++ simulation core change | cpp-cmake-specialist | — |
| Build system / CMake change | cpp-cmake-specialist | — |
| SCPI parser change | cpp-cmake-specialist | scpi-implementer |
| Real device spec research | hardware-spec-researcher | scpi-implementer |
| General Python layer | (main agent) | scpi-implementer |

Before implementing a new device class, ALWAYS:
1. Use hardware-spec-researcher to find the real device's SCPI command set
2. Let it write docs/devices/[manufacturer]-[model]-spec.md
3. THEN have scpi-implementer build the class from that spec

---

## SILMULATOR-SPECIFIC: TEST COMMANDS

```bash
# C++ build and test
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
ctest --output-on-failure
cd ..

# Python tests
pytest tests/ -v

# Both (run this for any issue touching either layer)
mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j4 && ctest --output-on-failure && cd .. && pytest tests/ -v
```

Tests must pass before committing. If cmake fails, that's a build failure
not a test failure — report it clearly in the issue comment.

---

## SILMULATOR-SPECIFIC: LEARNING CONCEPT SELECTION

When selecting the concept for /teach, /drill-me, and /terms:

For new device classes:
- Concept = the device's distinctive technical behavior, NOT the model name
- "Multi-channel power supply SCPI channel selection" ✅
- "Rigol DP832 support" ❌

For SCPI changes:
- Concept = the SCPI feature or protocol aspect being implemented
- "SCPI mandatory commands IEEE 488.2" ✅
- "Fixed a handler" ❌

For PyQt6 GUI blocks:
- Concept = the Qt pattern being applied
- "Qt drag-and-drop hardware block pattern" ✅
- "Added a new block" ❌

For C++ core:
- Concept = the C++ technique, data structure, or algorithm
- "Ring buffer thread-safe access with std::mutex" ✅
- "Buffer fix" ❌

For CMake:
- Concept = the build system concept
- "CMake FetchContent for C++ dependencies" ✅
- "Updated CMakeLists" ❌

---

## SILMULATOR-SPECIFIC: HARD RULES

1. Every new device MUST implement all mandatory IEEE 488.2 commands
   (*IDN?, *RST, *CLS, *ESE, *ESR?, *OPC, *OPC?, *STB?, *SRE, *SRE?)
2. Both long-form and short-form SCPI commands MUST work
   (MEASure:VOLTage? AND MEAS:VOLT? must both respond identically)
3. Handlers return strings for queries, None for commands, never Python booleans
4. *RST must restore ALL stateful values to documented power-on defaults
5. Out-of-range values must add to ErrorQueue, not silently clip
6. No new device class is complete without pytest unit tests covering:
   - All mandatory commands
   - Normal range, boundary, and out-of-range inputs
   - Short-form equivalence
   - *RST behavior
7. No PyInstaller .dmg or .app in this commit — code only
8. No changes to main branch — feature branches only

---

## SILMULATOR-SPECIFIC: OVERNIGHT LAUNCHER

Save this as ~/scripts/nightly-silmulator.sh:

```bash
#!/bin/bash
set -euo pipefail

REPO="$HOME/repos/Claude_Builds/Silmulator"
LOG="$REPO/nightly-$(date +%Y%m%d).log"

echo "$(date): Starting Silmulator overnight session" | tee "$LOG"

cd "$REPO"
git checkout main
git pull

claude --dangerously-skip-permissions \
  "Run /overnight-session for the Silmulator project. Tonight's issues are the ones labeled ready-for-ai. Use the Silmulator overnight session command which specifies the scpi-implementer, pyqt6-block-builder, and cpp-cmake-specialist agents. Generate learning materials for each completed feature and rebuild the index at the end." \
  2>&1 | tee -a "$LOG"

echo "$(date): Session complete. Check OVERNIGHT_REPORT.md and draft PRs." | tee -a "$LOG"
```

chmod +x ~/scripts/nightly-silmulator.sh
