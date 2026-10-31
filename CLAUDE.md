# cpp_games — Claude Code Rules

## Project Overview
This is the root of the cpp video games that I am working on. For this, we just need to get the dependencies installed.

## Tech Stack
- **C++20**, no compiler extensions. Two builds must both stay green, and they
  are not the same build: `tests/Makefile` GLOBS `src/core/*.cpp`, while the
  root `CMakeLists.txt` lists sources EXPLICITLY. A green test suite does not
  prove a green cmake build — a new `src/core` file can be missing from CMake
  and nothing will notice. Run both.
  - `make -C tests test` — core logic, no graphics; the only build available in
    the Linux dev container
  - `cmake -S . -B build && cmake --build build` — full app, needs a display
  - `make -C tests portability` — asserts the macOS floor still holds
- **raylib 5.5**, pinned via CMake FetchContent. 6.0 redesigned the skeletal
  animation API the character rendering depends on; do not bump it casually.
- **doctest** (`external/doctest.h`), **nlohmann/json 3.11.3**
  (`external/json.hpp`), **SQLite amalgamation** (`external/sqlite3.c`).
  All vendored — a clean clone builds with no system packages.
- **macOS floor: 11.0**, enforced by `CMAKE_OSX_DEPLOYMENT_TARGET`. This is
  deliberate and load-bearing: `std::format`, `std::format_to` and
  floating-point `std::to_chars` are Apple-gated at macOS 13.3 and are a
  compile error below it. None of them is adopted — `std::format` measured
  1.7–2.3× **slower** than the `std::to_string` concatenation it would replace.
  Integer `to_chars`, `starts_with`/`ends_with`, `map::contains`, designated
  initialisers, `<span>`, `<ranges>` and `std::ssize` are all fine.
  See `.claude/plans/cpp20-upgrade.md`.
  - **Trap:** `__cpp_lib_format` is `0` on Apple's libc++ even where
    `std::format` works, because availability annotations suppress the
    feature-test macro. Never guard on it; guard on the deployment target.
- **LLM backends:** Ollama (local) and OpenAI-compatible HTTP.

## Skill Usage Policy

Standards and analysis skills are OPT-IN. Do NOT apply the following
unless I explicitly invoke them by name or via a command that calls them:

- python-standards
- c-cpp-standards
- complexity-reduction

During normal work, write reasonable code without enforcing these.
Enforce them only when asked (e.g. "apply python-standards to this file"
or via /refine). When in doubt, ask before applying a standards skill.

## Hard Rules
- No placeholders or TODOs in committed code
- Every public function needs a docstring or comment
- Match existing naming conventions in the codebase
- Use type hints / type annotations where the language supports them
- Tests required for new business logic

## Workflow
- For new features: start with /grill-me to align before coding
- Use plan mode for any change touching 3+ files
- For unfamiliar code: use /zoom-out for context
- For bugs: use /diagnose for structured debugging (no guessing fixes)
- Commit format: type(scope): description
- Push to feature branches, never directly to main
- Use the reviewer subagent before committing significant changes
- Use the strict-reviewer subagent before milestones or risky integrations
- At session end: /recap for learning, /handoff if continuing later

## Communication Style
- Be direct and concise
- Surface trade-offs explicitly
- Ask questions when requirements are unclear

## What NOT to Do
- Don't modify .env files or anything in /secrets/
- Don't install new dependencies without asking
- Don't refactor code unrelated to the current task
- Don't add features I didn't ask for

## Skill Creation — Automatic
When you notice you're applying a pattern that could be reused, propose
it as a skill.

A pattern qualifies as a skill when:
- It's been used 2+ times in this codebase, OR
- It's a non-trivial technique that took thought to get right, OR
- It encodes domain knowledge specific to this project, OR
- It's a workflow that could break if done wrong

When you spot one, after completing the current task:
1. Pause and propose: "I notice [pattern] is appearing repeatedly.
   Should I save this as a skill?"
2. If approved, create .claude/skills/[skill-name]/SKILL.md following
   the format in .claude/skills/README.md
3. Keep skill files focused and under 200 lines each
4. Reference existing skills before reinventing patterns

## Transparency Requirement
When using a library, framework feature, language construct, or pattern
not used elsewhere in this codebase, briefly mention it:
"Using [X] for this because [Y]. New to this codebase."
One sentence. I can ask follow-ups if interested.
