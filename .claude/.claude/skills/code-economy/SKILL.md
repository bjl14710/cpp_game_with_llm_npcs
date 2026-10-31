---
description: Use this skill when writing or reviewing any code. Enforces code economy (write only what is necessary), test requirements (every change is verified), and readability (PRs a future developer can follow). Read before implementing any feature or reviewing any PR.
---

# Code Economy Skill

This skill enforces three principles on every change:
1. **Economy** — write only what is necessary (YAGNI)
2. **Verified** — every change is tested before committing
3. **Readable** — a future developer can follow the PR without asking questions

These are not optional. They apply to every feature, bug fix, and
refactor regardless of size. A change that passes tests but fails
the readability check is not ready to merge.

---

## Principle 1 — Economy (YAGNI / Ponytail)

Before writing any new code, run the Decision Ladder:

```
1. Does this need to exist at all?
   → Could the feature be dropped, deferred, or scoped smaller?
   → If yes to any: drop or defer it.

2. Can the standard library handle it?
   → Python: os, pathlib, collections, itertools, dataclasses, functools
   → C++: std::, <algorithm>, <ranges>, <filesystem>
   → If yes: use it. Never write what stdlib gives you.

3. Is there a native language/framework feature?
   → Python dataclass instead of manual __init__?
   → C++ structured binding instead of .first/.second?
   → If yes: use it.

4. Is there already a dependency in this project that does it?
   → Check imports and CMakeLists before adding anything new.
   → If yes: use it. No new dependency for something already available.

5. Can it be a one-liner or two-liner?
   → If yes: write it that way. Resist the urge to abstract prematurely.
```

Only if every answer is "no" do you write new code.

### Leave a comment when you chose minimal on purpose:

```python
# economy: stdlib re is enough here — upgrade to regex lib only
# if we need atomic groups or Unicode case-folding later
pattern = re.compile(r"\b(MEAS|MEASure)\b", re.IGNORECASE)
```

```cpp
// economy: std::vector<bool> bitmap is sufficient for N < 1000 —
// switch to bitset only if profiling shows a bottleneck
std::vector<bool> seen(max_cmd_id, false);
```

These comments make the intent clear in review and prevent a future
developer from "upgrading" something that was deliberately kept simple.

### Things that are explicitly EXEMPT from YAGNI:

- **SCPI mandatory commands (IEEE 488.2)** — all 10 mandatory commands
  must be implemented even if not explicitly requested. Completeness
  matters more than brevity for protocol compliance.
- **Test coverage** — do not apply YAGNI to tests. More tests are
  almost always better. The only exception is tests that test the same
  thing twice in the same way.
- **Error handling** — do not skip error handling because "it won't
  happen." Production code must handle errors. Silence is not handling.
- **Documentation strings** — doc strings on public functions are not
  bloat. They are the first thing a reviewer reads.

---

## Principle 2 — Verified (test requirements)

Every change must be verified before committing. Unverified code does
not exist as far as the PR is concerned.

### Minimum test bar per change type:

**New function or method:**
- At least one test for normal/expected input
- At least one test for boundary or edge input
- At least one test for invalid/error input (if the function can fail)

**Bug fix:**
- A test that FAILS before the fix and PASSES after
- This is non-negotiable — a bug fix without a regression test will
  recur

**New device class (Silmulator):**
- All mandatory IEEE 488.2 commands pass
- Short-form equivalence tested
- `*RST` behavior tested
- At least one out-of-range input tested (should produce ErrorQueue entry)

**New NPC character (NPC game):**
- 5 representative conversations in the test suite
- JSON schema validation passes on all outputs
- Off-topic and hostile player inputs handled in-character
- Proposed action trust threshold tested (below threshold = rejected)

**Backtesting change (Trading):**
- Backtesting-verifier passes (no lookahead bias detected)
- Results consistent across multiple runs (no randomness without seed)

### Test naming convention:

```python
# Format: test_[what]_[condition]_[expected]
def test_voltage_query_above_range_returns_error():
    ...

def test_rst_resets_all_channels_to_default():
    ...

def test_npc_dialogue_off_topic_question_deflects_in_character():
    ...
```

### Before every commit, run and confirm:

```bash
# Python
pytest tests/ -v && echo "ALL TESTS PASS"

# C++ (Silmulator)
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && make -j4 && ctest --output-on-failure

# Both
cd build && cmake .. && make -j4 && ctest --output-on-failure && cd .. && pytest tests/ -v
```

If tests fail — fix them. Do not commit with failing tests.
Do not comment out tests to make them pass.
Do not `skip` a test without a documented reason in the test file.

---

## Principle 3 — Readable (PR readability requirements)

A PR is readable if a developer who wasn't in the room can understand:
- What changed and why, in 2 minutes
- How to verify it themselves
- What would break if they reverted it

### Code-level readability:

**Naming must be self-explanatory:**
```python
# Bad
def p(c, v):
    return c.s.get(v, 0)

# Good
def get_channel_voltage(channel, channel_id):
    return channel.state.get(channel_id, 0.0)
```

**Functions do one thing:**
- If you need "and" to describe what a function does, split it.
- Maximum meaningful length: if you can't see the whole function on one
  screen, consider splitting it.

**Magic numbers get constants:**
```python
# Bad
if retries > 3:

# Good
MAX_RETRY_ATTEMPTS = 3
if retries > MAX_RETRY_ATTEMPTS:
```

**Complex logic gets a comment explaining WHY, not WHAT:**
```python
# Bad comment (restates the code):
# add 1 to the index
index += 1

# Good comment (explains the non-obvious reason):
# SCPI channel numbers are 1-indexed per IEEE 488.2 §C.3.1,
# but our internal buffer is 0-indexed
index += 1
```

### PR description readability (the /mr-description output):

The PR description must answer these questions for a reviewer:
- What does this change do? (Summary — 2 sentences max)
- Why does it exist? (Why section — reasoning, not just restating what)
- How do I know it works? (Testing section — specific, not "tests pass")
- What could break? (Risk section — only for significant changes)

The Changes by File section must explain why each file changed, not
just that it changed:

```
# Bad
src/devices/psu_e3631a.py — modified
Updated power supply implementation.

# Good
src/devices/psu_e3631a.py — added
New Keysight E3631A triple-output PSU device class. Implements all
mandatory IEEE 488.2 commands plus INST:SEL for channel switching.
Three independent output channels: 6V/5A, 25V/1A, -25V/1A.
```

### Commit message readability:

Use conventional commits format:
```
fix(#42): add Keysight E3631A triple-output PSU SCPI support

- Implements INST:SEL for P6V, P25V, N25V channel selection
- All mandatory IEEE 488.2 commands (*IDN?, *RST, *CLS, etc.)
- Short-form and long-form commands both work
- *RST restores all channels to 0V / output OFF

Closes #42
```

Format: `type(scope): description`
Types: `fix`, `feat`, `docs`, `test`, `refactor`, `chore`, `perf`
Scope: the issue number or component name

---

## Self-Check Before Committing

Run through this mentally before every commit:

```
Economy:
[ ] Passed the Decision Ladder
[ ] No new dependencies that aren't necessary
[ ] No code that stdlib already provides
[ ] Left a comment where a "minimal" choice was made on purpose

Verified:
[ ] New functions have tests
[ ] Bug fixes have a regression test
[ ] All tests pass locally (pytest / ctest)
[ ] No tests commented out or skipped without documented reason

Readable:
[ ] Function names explain what they do without reading the body
[ ] Complex logic has a WHY comment
[ ] Magic numbers have named constants
[ ] Commit message follows conventional commits format
[ ] PR description answers: what, why, how to verify
```

If any box is unchecked — fix it before committing.

---

## Reviewing Others' Code (or Claude's own output)

When reviewing a PR or your own generated code, check:

1. **Bloat** — does anything here fail the Decision Ladder?
   If so, flag it and suggest the simpler alternative.

2. **Coverage gaps** — is there a code path with no test?
   If so, name the specific path and the input that exercises it.

3. **Readability gaps** — where would a reviewer get confused?
   Flag the specific function or section, not "code is unclear."

4. **Naming** — any function or variable where you have to read
   the body to understand the name? Flag it with a suggested rename.

Flag findings as:
- **Must** — blocks merging (failing test, security issue, data loss risk)
- **Should** — strong recommendation (naming, missing error handling)
- **Could** — optional improvement (minor readability, style)

---

## What This Skill Does NOT Enforce

- Line length limits (follow the project's existing style)
- Specific formatting (use the project's formatter — black, clang-format)
- 100% test coverage (coverage is a target, not a requirement)
- Short functions for their own sake (clarity > line count)

The goal is code that is minimal, verified, and followable —
not code that is short, formatted to a spec, or aggressively DRY.
