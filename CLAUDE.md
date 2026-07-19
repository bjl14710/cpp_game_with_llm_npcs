# CLAUDE.md — LLM NPC City

## What This Is
A first-person 3D city (raylib) where every inhabitant is a live LLM
character. Players walk up, press `T`, and talk — each NPC has its own
personality, knowledge boundaries, and memory of the conversation.
Runs locally against Ollama by default; optionally against cloud models
(Claude, GPT, Gemini) via a single OpenRouter key.

## Stack
- C++ game engine and rendering (raylib, fetched by CMake)
- LLM dialogue via `LlmClient`/`LlmBackend` — native Ollama or an
  OpenAI-compatible backend (OpenRouter, OpenAI, Gemini's compat layer,
  a local `/v1`)
- No structured JSON reply schema — the model replies in plain text with
  inline bracket directive tags, e.g. `[[action: follow]] [[mood: happy]]`,
  parsed by `parseDirectives()` in `src/core/NpcAction.hpp`
- SQLite persistence for per-NPC cross-session memory (`ConversationStore`)
- doctest for the C++ test suite

## Issue Tracker
GitHub — use `.claude/skills/github/SKILL.md` for issue/PR operations.

## Test Commands
```sh
make -C tests test      # full offline unit-test suite, no graphics libs required
```
For an in-engine visual check (headless, via Xvfb + a smoke-test flag),
see `docs/DEVELOPER.md`.

## The NPC Directive Protocol (never change without updating the parser)
Replies are free text with zero or more bracket tags anywhere in the
string; `parseDirectives()` strips them from what the player sees.

```
[[action: follow|stop|face|raise_hand|wave|arrest|call_police]]
[[mood: neutral|happy|angry|sad|embarrassed|surprised]]
```

- `action` values map via `actionFromKeyword()` in `src/core/NpcAction.hpp`;
  unknown keywords silently become `None` (fail safe, not fail loud).
- `mood` accepts small-model synonyms (`flattered` → `happy`,
  `annoyed` → `angry`, etc.) via `moodFromKeyword()`.
- `arrest`/`call_police` are civilian/police-only in practice — that
  restriction lives in the NPC's system prompt and character role, not in
  the parser. The parser itself trusts nothing about *who* sent a tag.
- Adding a new action keyword means updating `actionFromKeyword()` *and*
  any NPC-role gating that decides who's allowed to use it.

## Hard Rules
- LLM output is UNTRUSTED INPUT — `parseDirectives()` only recognizes a
  fixed allowlist of action/mood keywords; anything else is dropped.
- System prompts must work on small local models (Ollama/Qwen), not just
  cloud — see `docs/DEVELOPER.md`'s latency/prompt-size guidance.
- Never commit `docs/learning/`, `OVERNIGHT_REPORT.md`, or `.claude/memory/`.
- Feature branches + PRs only; don't push directly to `main`.

## Preferred Agents (use for implementation)
- `npc-dialogue-designer` — NPC characters, system prompts, directive tags
- `game-state-auditor` — trust boundary and directive-parsing validation
- `prompt-optimizer` — after any system prompt change, check token budget

## Agent Routing Rules
After implementing each issue, before committing, check what changed:

| What changed | Agent to invoke |
|---|---|
| Directive tag protocol (`NpcAction.hpp`) | `game-state-auditor` |
| New action keyword added | `game-state-auditor` |
| NPC memory / `ConversationStore` schema | `game-state-auditor` |
| New NPC character/system prompt | `npc-dialogue-designer` (review) |
| System prompt grew noticeably | `prompt-optimizer` |
| Test additions | `test-auditor` |
| Anything before commit | `code-economy` skill |

## Concepts Worth Teaching
- Directive-tag parsing as a narrow, fail-safe LLM/engine trust boundary
- Per-NPC in-session history vs. cross-session `ConversationStore` summaries
- Token efficiency patterns for small-model NPC system prompts
- Headless smoke-testing a raylib app under Xvfb
