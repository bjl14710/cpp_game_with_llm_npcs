# Overnight Build Report
Date: 2026-08-21
Issues attempted: 5 (4 completed, 1 skipped)

## Summary

Four issues completed with draft PRs, all stacked on the unmerged #155 work.
Two of them turned up **pre-existing bugs that nothing was going to catch** —
the Sandbox and Model menu pages have been titled "Multiplayer" since they were
added, and the main menu's Quit button has been rendering 14px off-screen. Both
were found by looking at screenshots, not by tests, and both are fixed.

The night's most substantive result is #200: the role leak probe now runs
against a real model for the first time and says the mystery's headline exploit
is closed — but that the secret-keepers are **not providing the cover the design
says they provide**. The killer is defended by an entirely different, undocumented
mechanism. That is a design finding, not a bug, and it needs a human decision.

A milestone of 11 issues (#299-#309) was also planned and filed for the two-loop
NPC dialogue measurement system, with scaffolds on disk.

## Completed ✅

| Issue | Title | Branch | PR | Concept Taught |
|-------|-------|--------|----|----------------|
| #156 | Gather every resident at the plaza for the vote phase | `feature/issue-156-plaza-gather` | #310 | Layering a temporary override over authored content |
| #157 | Day/phase HUD and match settings | `feature/issue-157-match-hud-settings` | #311 | Diegetic versus HUD signalling for time pressure |
| #304 | `persona_prompt --state` renders the full prompt for a scripted state | `feature/issue-304-persona-prompt-state` | #312 | One implementation across a language boundary |
| #200 | Implement the leak probe and record a baseline | `feature/issue-200-role-leak-probe` | #313 | Side-channel leakage: measuring what a system reveals through its metadata |

## Skipped (needs human) ⏭

| Issue | Title | Reason |
|-------|-------|--------|
| #173 | Author eleven new residents, flip roster to 21 | Its own precondition is unmet: `bench_npc_render.py --gate 16.67` still fails (p99 18.42 ms, 228/900 frames over, at the far camera). Confirmed the premise rather than re-running the bench — `git log 044de83..origin/dev -- src/app/RaylibRenderer.cpp src/core/CharacterParts.cpp` is empty, so nothing could have moved the number. Removed `ready-for-ai`, which was making a BLOCKED issue look actionable. |

## The two findings worth your attention

### 1. The secret-keepers are not doing their job (#200)

The probe ran 130 model calls against `qwen3:8b` across two independent runs.

- **Action leak: fixed.** Zero forbidden directives. The killer emitting
  `[[ACTION: call_police]]` does not reproduce at the shipped role-block placement.
- **Mood leak, as a player would exploit it: PASS.** Killer ranks 4th of 10 by
  hostility, behind three bystanders (p = 0.50, rank permutation).
- **The issue's literal pass condition FAILS**, reproducibly:

| Comparison | Run A | Run B |
|---|---|---|
| killer vs secret-keepers | 0.85 | **0.59** — separable |
| killer vs bystanders | 0.20 | **0.12** — indistinguishable |
| secret-keepers vs bystanders | 0.80 | **0.69** — separable |

It is not a leak, because the direction is inverted from the harmful one: the
killer reads *ordinary*, and it is the **secret-keepers** who stand out —
`secret_keeper.role`'s demeanour line yields `[[MOOD: embarrassed]]` on ~59% of
turns against ~0% for everyone else.

So the exploit does not open. But cover requires the killer and the keepers to
look alike, and they look nothing alike. **The killer hides among the bystanders
by being unremarkable** — a real, measurable defence that no one would learn from
reading the role files.

This also answers the plan's open question 1 ("how many secret-keepers is enough
cover?"): the count is the wrong lever. Eight keepers would deepen the embarrassed
cluster without moving the killer nearer to it. `bench/ROLE-LEAK.md` names two
directions and takes neither — both are content calls.

**Caveat, stated plainly:** the killer contributes 8 turns. TVD at that n is
noisy, and the 0.85 → 0.59 move between runs is what the noise looks like. What
survives both runs is the *ordering*, not the values.

### 2. Two menu bugs that had been shipping for months (#157)

- The page-title chain ended in `: "Multiplayer"`, so **every unhandled page wore
  that title** — Sandbox and Model both have, since they were added. Replaced with
  a `switch` carrying no `default:`, so the next page is a compiler warning.
- The main menu started at a fixed `h*0.5 - 326` stepping 72px. With ten rows the
  last button's bottom edge sat at y=734 in a 720px window: **Quit has been 14px
  off-screen.** This is the "menu row-count budget undocumented" low from #155's
  review, now a real defect. Layout now derives from the row count.

Neither is expressible as a test here: `tests/Makefile` globs `src/core/*.cpp` and
nothing else, so all of `src/app/` is verified by screenshot or not at all.

## Also delivered: milestone 34 planned and scaffolded

Eleven issues filed for the two-loop dialogue measurement system
(#299-#307, #309, plus #308 flagged `needs-human`). Plan at
`.claude/plans/npc-dialogue-measurement.md`; scaffolds are on disk but
**uncommitted**, because they carry TODOs and CLAUDE.md forbids committing those.

Recon corrected four things about the original spec — the dialogue path emits
directive tags rather than JSON; `RatingLog` already implements half the curation
bridge; `role_leak_probe.py` was a live queued issue rather than dead code (so the
plan's "absorb and delete it" was wrong and is now corrected); and
`docs/learning/`, `OVERNIGHT_REPORT.md` and `.claude/memory/*` are **tracked, not
gitignored** as the instructions assumed.

## Test Status

All gates green on every branch at the point of commit.

| Gate | Result |
|---|---|
| `make -C tests test` | **546 passed / 0 failed / 48 skipped** (15 new tests tonight) |
| `cmake --build build` | clean, no warnings |
| `make -C tests portability` | macOS 11.0 floor holds |
| `python3 -m unittest discover -s tools` | green |
| `scripts/check-persona-prompt-state.sh` | 8/8 |
| `role_leak_probe.py --turns 8` | PASS, exit 0 |

The 48 skips include 24 from tonight's uncommitted scaffolds (`DirectiveCheck`,
`TelemetryLog`, `GoldenSet`) — they compile and are skipped, as intended.

## Suggested First Move

**Read `bench/ROLE-LEAK.md` and decide what the secret-keepers are for.** Two
options, both coherent: widen their affect so `embarrassed` stops being a group
signature, or accept that the killer hides among ordinary residents and re-state
that in `roles/README.md`. Everything else tonight is reviewable at your leisure;
this one is a design decision blocking nothing yet but quietly shaping the mystery.

## Draft PRs Awaiting Review

Stacked — each targets the previous, so review in order and merge from the bottom:

| PR | Base | Summary |
|---|---|---|
| #298 (#155) | `dev` | Match drives the world clock |
| #310 (#156) | #298's branch | Plaza gather for the vote phase |
| #311 (#157) | #310's branch | Day/phase HUD, match settings, + two menu bugs |
| #312 (#304) | #311's branch | `persona_prompt --state` |
| #313 (#200) | #312's branch | Leak probe implemented and baselined |

## Standing items that are yours, not mine

- **Rotate `GH_TOKEN`** — I leaked it in plaintext in an earlier session.
- **Rotate the `.claude/.claude/.credentials.json` OAuth tokens** — still in `dev`
  history on a **public** repo since 2026-07-18.
- **PR #281** — open, non-draft, base `main`, empty body, 144 commits, carries the
  credentials file. Should be closed.
- **#308** — decide whether `docs/learning/` (76 tracked files), `OVERNIGHT_REPORT.md`
  and `.claude/memory/*` should be untracked. Untracking deletes them from the remote.
- **#173** — decide between optimising ~2 ms of character rendering or relaxing the
  16.67 ms gate. The gap is 1.75 ms at p99, not the ~3.5 ms the old arithmetic implied.

---

# Overnight Build Report — Anthropic Haiku 5.5 backend
Date: 2026-10-09
Branch: `design_updates_and_haiku_usage` (draft PR; main untouched)
Plan: `.claude/plans/anthropic-haiku-backend.md`

## Summary

`provider = anthropic` in `config/llm.cfg` now sends NPC dialogue to
`claude-haiku-5-5` through the Messages API, with the configured Ollama model as
the automatic fallback. Nothing above the LLM layer changed: the same system
prompt is assembled, the same directive tags are parsed, the same role gate
decides whether a secret is in the prompt. No key means the game is Ollama,
byte for byte, plus one log line.

| Gate | Result |
|---|---|
| `make -C tests test` | **588 passed / 0 failed / 48 skipped** (34 new) |
| `cmake -S . -B build && cmake --build build` | clean; `OpenSSL 3.6.3 found — https LLM providers enabled` |
| `make -C tests portability` | macOS 11.0 floor holds |
| Live TLS probe (invalid key, real endpoint) | handshake OK, HTTP 401, backend disabled, fallback requested |

## Decision log

Every fork in the spec, the option taken, and why. Sources are the files read
or the pages fetched on 2026-10-09; nothing here is from memory.

1. **Reuse the existing seam, do not build a second one.** `LlmBackend`
   (`src/core/LlmBackend.hpp`) already is "generate(request) -> text or error"
   with Ollama and OpenAI-compatible implementations behind `makeBackend`. The
   new `AnthropicBackend` is a third implementation; the only seam change is one
   `bool fallback` on `BackendResult` so a backend can decline a request without
   failing it.
2. **Fallback as a composite, not as flags inside LlmClient.** `makeBackend`
   returns a `FallbackBackend(Anthropic, Ollama)` for `provider = anthropic`.
   LlmClient, Npc and main.cpp did not change. Warm-up goes to Ollama because
   that is the model that has to be resident when the cloud drops out.
3. **Godot HTTP constraint is inapplicable; cpp-httplib is used; OpenSSL made
   optional.** The project is C++/raylib with cpp-httplib 0.15.3 vendored.
   httplib cannot speak https without `CPPHTTPLIB_OPENSSL_SUPPORT` and no build
   defined it (grep of `CMakeLists.txt`, `tests/Makefile`). CMake now does
   `find_package(OpenSSL QUIET)` and enables it when found; a machine without
   OpenSSL still builds and the backend logs once and falls back. No vendored
   dependency added. The test build stays TLS-free: the fakes speak http.
4. **Thinking disabled and effort low, both sent.** Migration guide: thinking is
   adaptive by default on Haiku 5.5, `{"type":"disabled"}` is accepted at effort
   low/medium/high, and thinking tokens count toward `max_tokens` so a small cap
   "can stop with stop_reason: max_tokens after a thinking block and before any
   text". With a 200-token cap that risk is the whole budget, so disabled wins
   over the guide's general "use effort as the lever" advice. Effort low is sent
   as well so the choice is explicit if thinking is ever re-enabled.
5. **No temperature / top_p / top_k.** Migration guide: any non-default value
   returns 400 on this model and the defaults are what we would send.
6. **No structured outputs.** The reply contract is directive-tagged prose
   (`[[MOOD: x]]`, `[[ACTION: y]]`, `src/core/NpcAction.hpp`,
   `src/core/DirectiveCheck.hpp`), not a JSON schema, so `output_config.format`
   has nothing to enforce. Replies go through the existing `checkReply` path
   unchanged. Documented as supported on Haiku 5.5 should the contract ever
   become JSON.
7. **No backend-level "1-3 sentences" instruction.** Every persona's `style`
   field already says it (`personas/baker.persona:4` etc.), and the client is
   shared with world generation, whose JSON prompt a brevity line would damage.
   The prompt layer is untouched, which is what the goal asked for.
8. **Prompt caching: one `system` text block with `cache_control: ephemeral`,
   history capped to the last 12 turns.** Caching docs: cache hierarchy is
   tools → system → messages, the breakpoint goes on the last stable block, and
   the minimum cacheable prefix on Haiku 5.5 is 512 tokens. The whole rendered
   prompt is one block because `Persona::renderSystemPrompt` already orders it
   stable-first; memory and gossip sit inside it, so a memory write invalidates
   that NPC's cache for one turn. Short personas may fall under 512 tokens and
   be silently uncached: both cache counters then read 0 and nothing is wrong.
9. **History window opens with a user turn.** After the cap, leading
   non-user turns are dropped so `messages` never starts with an orphaned
   assistant line.
10. **Key handling.** Read from `NPC_GAME_ANTHROPIC_API_KEY` or gitignored
    `config/secrets.cfg` (`api_key`), via the existing resolver in
    `src/core/Config.cpp`. The env var name follows the provider when
    `api_key_env` is not set, so switching providers cannot read an OpenRouter
    key by accident. The key travels only in the `x-api-key` header: never in a
    body, an error string, a log line, or a network message to clients
    (multiplayer already routes every remote player through the host's single
    `LlmClient`, `src/core/HostChatRouter.hpp`).
11. **Refusal line.** `stop_reason: "refusal"` yields
    `I'd rather not get into that. [[MOOD: neutral]]` as a successful reply, with
    no retry and no Ollama: the API has no server-side fallback and re-asking a
    local model a question Claude declined would defeat the refusal.
12. **max_tokens is a failed reply, not a fallback.** The existing failure path
    (`Npc::onReplyArrived` → "seems distracted") is what the player sees, and no
    truncated word is emitted as a delta. Ollama is not asked, because a prompt
    that overran the cap once would overrun it again.
13. **Retry once with backoff on 429 / 5xx / timeout / unreachable, then
    Ollama for that request.** 401 disables the backend for the session. 400
    and other 4xx go straight to Ollama for that request with the error body
    clipped into the log. Pinned by `classifyExchange` and its test.
14. **Cost visibility.** All four `usage` counters summed per session; one
    `[llm_npc] anthropic: session usage:` line from the backend destructor,
    which runs when LlmClient leaves scope at shutdown. `anthropic_token_budget`
    (0 = none) switches the session to Ollama when the sum reaches it.
15. **Shared URL parser.** `parseBaseUrl` moved from a private copy in
    `OpenAiBackend.cpp` to `src/core/BaseUrl.hpp` so both cloud backends use
    one. The only change to existing code outside the seam.
16. **Commits use explicit paths.** The branch carried unrelated staged
    scaffolds (DirectiveCheck, GoldenSet, TelemetryLog, SignLayout, tools/*.py,
    AGENTS.md). They were left exactly as found; nothing here commits them.

17. **Reviewer pass (reviewer subagent) and what it changed.** One crash path
    and a few robustness gaps, all in the new code, all fixed and tested:
    a 200 whose content blocks are not objects or carry `"text": null` used to
    throw out of `json::value()` on the worker thread (uncaught → terminate);
    blocks are now checked field by field. A text-less 200 now declines to
    Ollama instead of rendering a blank line. `decline()` now logs once per
    distinct reason, so a persistent 400 is visible. secrets.cfg's single
    `api_key` slot is now provider-checked by the `sk-ant-` prefix, so an
    Anthropic key can never go out as a Bearer token to a third-party
    endpoint and vice versa. Plain http is refused to any host but loopback.
    Error-body clips mask the key before they are kept. Empty history turns
    are dropped (the API rejects empty text blocks). New `anthropic_timeout_s`
    (default 20) bounds the try/retry/fallback chain to well under a minute
    instead of two minutes at Ollama's 60 s timeout.
18. **behavior-qa pass.** Everything runnable passed; nothing statistical
    could run because no eval baseline or golden set exists yet. It flagged
    the OpenSSL deployment-target link warning noted below.

## Acceptance criteria

| Criterion | Status |
|---|---|
| Valid key → schema-valid reply from `claude-haiku-5-5` | **Not run: no key on this machine** (env var unset, `config/secrets.cfg` absent). Steps below. |
| No key → Ollama exactly as before | Tested: Ollama bodies byte-identical, one log line |
| Gated secret absent from outbound body | Tested both ways: absent with the gate closed, present with it open (so the first case is not vacuous), and only ever inside the cached system block |
| Mocked refusal → in-character line | Tested |
| Mocked 500 → Ollama answers | Tested, including "500 then 200 delivers from Anthropic" |
| Second consecutive request shows nonzero `cache_read_input_tokens` | **Not run: needs a key.** Steps below. |
| No key material in repo or history | **Fails on a pre-existing item, not on this work** — see below |
| Existing tests pass; both builds green | Yes |

### Manual verification (needs your key)

```
export NPC_GAME_ANTHROPIC_API_KEY=sk-ant-...
sed -i '' 's/^provider = ollama/provider = anthropic/' config/llm.cfg
./run.sh
```
Talk to one resident twice in a row and quit. The shutdown line
`[llm_npc] anthropic: session usage: ... cache_write=N cache_read=M` should show
`cache_write > 0` after the first turn and `cache_read > 0` after the second;
if both stay 0 the persona's prompt is under the 512-token minimum (decision 8).
Set `provider = ollama` back afterwards.

## Needs your attention

- **Key material in the repo (pre-existing).** `git grep` for `sk-ant-` finds
  `.claude/.credentials.json` and `.claude/.claude/.credentials.json`, both
  **tracked on this branch** (commit `1e0cd87 saving claude files to be used on
  remote`) and in history. These are the OAuth tokens already listed under
  standing items above. Untracking them deletes tracked files and rewriting
  history is a force-push, so neither was done tonight. The acceptance criterion
  cannot pass until they are removed and rotated.
- **OpenSSL deployment target.** Homebrew's `openssl@3` is built for macOS 14,
  so the link emits `building for macOS-11.0, but linking with dylib ... built
  for newer version 14.0`. Harmless on this machine; a user on macOS 11-13 who
  wants the cloud path needs an OpenSSL built for their floor.
- **Design import was blocked.** The `claude_design` MCP server is not
  configured in this session, `DesignSync` requires `/design-login`, and the
  claude.ai design URL returns HTTP 403 to a plain fetch. Run `/design-login`
  (or add the MCP) and the `Redesign.dc.html` import can proceed.
