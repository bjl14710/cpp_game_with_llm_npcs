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

# Overnight Build Report — Design import (Nocturne redesign)
Date: 2026-10-09
Branch: `design_updates_and_haiku_usage` (draft PR; main untouched)
Source: `Redesign.dc.html` from the claude.ai design project, read through
the `claude_design` MCP after `/design-login`; design system
`_ds/nocturne-...`, direction 1b (Inter UI + Instrument Serif display).

## Summary

The pause menu, every sub-page, the dialogue panel, the in-world HUD, the
death screen and the cutscene letterbox now follow the Nocturne design:
bg `#161826`, surface `#232532`, text `#e9e9ed`, accent `#9184d9`, the
neutral and accent ramps, 8px radius, outlined buttons, keycaps and tags.
No gameplay, input or networking code changed; `main.cpp` only swapped the
draw calls and feeds the new header/speaker strings to the menu and dialog.

| Gate | Result |
|---|---|
| `make -C tests test` | 649 passed / 0 failed / 48 skipped |
| `cmake -S . -B build && cmake --build build` | clean (pre-existing httplib + OpenSSL warnings only) |
| `make -C tests portability` | macOS 11.0 floor holds |
| Headless captures | every menu page, HUD, match HUD, cutscene, viewed and correct |

## Decision log

1. **One theme module, `src/app/Theme.{hpp,cpp}` (namespace `llm_npc::ui`).**
   The design's tokens, text helpers (`text`, `measure`, `wrap`, `ellipsize`)
   and widgets (panel, card, rule, kicker, keycap, tag, chip, pill, button,
   input, toast, vignette) live in one place so Menu, DialogUI and the HUD
   cannot drift. raylib `DrawText` ignores fractional spacing, so everything
   goes through `DrawTextEx` with a 10px floor.
2. **Fonts are an optional drop-in, not fetched.** `assets/LICENSES.md`
   permits CC0 assets only; Inter and Instrument Serif are OFL. The theme
   loads `assets/fonts/ui.ttf` and `assets/fonts/display.ttf` when present
   and falls back to raylib's default font otherwise. `tools/fetch_assets.sh`
   is untouched; adding the fonts (and their licence line) is the owner's call.
3. **ASCII-only UI strings.** raylib's default font covers code points
   32-126 only, so every arrow, dash and ellipsis in the design is spelled
   `<-`, `-`, `...`. The drop-in fonts would render the same strings.
4. **Menu: left 400px panel with groups Play / You / World / System**,
   keyboard Up/Down/Enter plus mouse, `setHeader(kicker, sub)` fed by
   main.cpp (zone + day, hour + phase). Sub-pages share one
   `renderPageHeader` (kicker, display title, sub-line, rule, `<- Back (Esc)`).
   All page logic, hooks and rebind semantics are unchanged.
5. **Dialogue: right 460px panel** (`DialogUI::kPanelWidth`), header with
   avatar initials, name, role - location and a mood tag, transcript filling
   from the bottom with YOU / NAME labels, streaming caret, outlined input,
   keycap hint row. New `setSpeaker` / `setMood`; the old "You are talking
   to ... Enter sends, Esc leaves." system line is gone because the header
   now carries it, and the F1/F2 HUD hint block moved into the hint row.
   The rest of the frame dims with a left-to-right gradient so the NPC stays
   visible beside the panel.
6. **HUD:** nameplates are pills (accent when talkable, neutral otherwise),
   signs are display text with an accent underline, a compass strip at the
   top (heading = 180 - yaw, markers for NPCs within nameplate range), a
   match clock card top-right (zone + day kicker, hour, phase + time until
   dusk from `daynight::kDuskStart`), a weapon card bottom-left (keycap,
   name, rounds, health bar; red below 30%), and the talk prompt as a
   keycap + "Talk to NAME". Crosshair is a 1.5px dot.
7. **Mood label mapping** Happy/Angry/Sad/Embarrassed/Surprised ->
   Warm/Irritated/Low/Flustered/Startled; Neutral shows no tag. The design
   showed a single example tag; the mapping is the smallest readable set.
8. **Death screen:** vignette, day/time kicker, display headline "You wake
   at the fountain.", accent rule, wrapped body, "Enter / Stand up" keycap.
9. **Cutscene:** letterbox bars in `kBg` with neutral-700 rules, slug as
   kicker + display line, speaker in accent-200 above the caption, headline
   wash + accent rule, "Space / skip" keycap top-right.
10. **Screenshots are local only** under `docs/qa/screenshots/nocturne/`
    (`menu_main`, `menu_controls`, `menu_multiplayer`, `menu_creator`,
    `menu_journal`, `menu_sandbox`, `menu_model`, `menu_match`, `hud`,
    `hud_match`, `cutscene`). Captured with `--frames N shot.png --menu <page>`
    / `--cutscene` / `--match 30 --hour 10`; raylib writes the basename to
    the working directory so each shot was moved afterwards. The dialogue
    panel cannot be captured headless (it needs a key press to open), so it
    was verified by compiling and by the shared Theme helpers the captured
    pages exercise.
11. **Sign overlap** (Gus's Hot Dogs over City Police Station in the HUD
    captures) is the pre-existing label-collision pass with 20px clearance
    and is unchanged by this work.
12. **Commit split.** `CMakeLists.txt`, `src/app/main.cpp` and
    `config/secrets.cfg.example` carry both design and TTS hunks. The design
    commit stages those files with the TTS lines removed; the TTS commit
    restores them. Each commit builds on its own.

## Needs your attention

- **Fonts.** Drop `Inter` as `assets/fonts/ui.ttf` and `Instrument Serif`
  as `assets/fonts/display.ttf` (both OFL) and add them to
  `assets/LICENSES.md` if you want the design's typography; without them the
  layout is identical in raylib's default font.
- **Dialogue panel** was not screenshot-verified headless. Open a
  conversation once and look at the header, transcript and hint row.

# Overnight Build Report — ElevenLabs TTS backend
Date: 2026-10-09
Branch: `design_updates_and_haiku_usage` (draft PR; main untouched)
Plan: `.claude/plans/elevenlabs-tts-backend.md`

## Summary

`provider = elevenlabs` in `config/tts.cfg` plus a per-NPC `voice_id` in
`config/voices.cfg` makes the host speak that NPC's replies through
ElevenLabs. No key, no provider or no voice entry means the line is silent,
exactly as the game is today. The key comes from `NPC_GAME_ELEVENLABS_API_KEY`
or the gitignored `config/secrets.cfg`, is only ever sent to `*.elevenlabs.io`
(or loopback for tests), and never reaches a log, an error string or the cache.

Spec note: the spec says "alongside the existing local Kokoro-82M backend" and
"Rhubarb lip sync ... unchanged". Neither exists in this repository (decision
1), so the fallback for every failure path is silence, logged once per cause.

| Gate | Result |
|---|---|
| `make -C tests test` | 649 passed / 0 failed / 48 skipped (61 new) |
| `cmake --build build` and `cmake --build build-tts` | clean |
| `make -C tests portability` | macOS 11.0 floor holds |
| `git grep -i "xi-api-key\|elevenlabs_api_key"` | header name, config key name, commented example, `*-never-real` fixtures only |

## Decision log


1. **Fallback is silence, because that is what exists.** `grep -ri
   "kokoro|rhubarb|tts|InitAudioDevice|speech" src tests config` finds nothing
   before this change. The spec's "fall back to the local pipeline" has no
   target in this repo, so every failure path ends in "this line is silent",
   logged once per cause. Stated plainly in the plan's Out of Scope.

2. **`provider = none` is the default and opens nothing.** `Speech`'s
   constructor returns before `InitAudioDevice()` when `makeTtsBackend` returns
   null, so a clean clone never initialises raylib audio, never starts the
   worker, never touches the network. Verified by a 30-frame smoke run of
   `build-tts/cpp_game_with_llm_npcs --frames 30`: zero `[llm_npc] tts:` lines
   (the only "audio" line is raylib's own `raudio: loaded (optional)` which
   predates this work).

3. **Request raw PCM and wrap it in a WAV header in code.** Output format
   default `pcm_22050`; `WavEncode.hpp` writes the canonical 44-byte RIFF
   header (format 1, 16-bit, 1 channel). No decoder dependency, lossless, and
   the cache holds one format. `wav_*` is also accepted and passed through
   after `parseWavHeader` validates it; mp3/opus/ulaw are refused at
   construction and replaced with `pcm_22050` with a log line. 22050 because
   "PCM and WAV formats with 44.1kHz sample rate requires you to be subscribed
   to Pro tier or above" — https://elevenlabs.io/docs/api-reference/text-to-speech/convert

   Caveat: no fetched page states that `pcm_*` is signed 16-bit little-endian
   mono. The code assumes it. Manual check 1 below confirms it by ear.

4. **Default model `eleven_flash_v2_5`.** The models page lists it at ~75 ms
   with a 40,000-character limit; the spec asked for the low-latency Flash
   model. Per-NPC `model_id` can override it in `voices.cfg`.
   https://elevenlabs.io/docs/models

5. **Character limits are enforced locally per model** before any request:
   flash_v2_5 40,000; flash_v2 / turbo 30,000; multilingual_v2 / v4 10,000;
   everything else (incl. `eleven_v3`) 5,000. Unknown models get the most
   conservative value; a server-side `text_too_long` (400) is still handled.

6. **Outcome policy is a pure function** `classifyTtsExchange(reached,
   status, body)` so the whole error table is one unit test. Mapping: not
   reached -> Retry; 200 -> Deliver; 401/402 -> Disable; 404 -> UnknownVoice;
   429 -> Retry unless `detail.code` (or legacy `detail.status`) contains
   `quota`, then Disable; >=500 -> Retry; anything else -> Fallback (silent, no
   retry). The errors page lists only rate-limit codes under 429
   (`rate_limit_exceeded`, `concurrent_limit_exceeded`, `system_busy`) and
   puts credit exhaustion under 402 `insufficient_credits`; the `quota` branch
   covers the legacy `quota_exceeded` status the spec named.
   https://elevenlabs.io/docs/eleven-api/resources/errors.md

7. **Unknown voice is tracked per NPC inside the backend.** `TtsVoice` carries
   `npcId`; a 404 inserts it into `unknownVoiceNpcs_`, logs voice + NPC once,
   and later calls for that NPC return without a request. Other NPCs keep
   their voices; the session is not disabled.

8. **Key resolution mirrors the LLM path but uses a new slot.** Env var
   `NPC_GAME_ELEVENLABS_API_KEY` (name configurable via `api_key_env`), else
   `config/secrets.cfg` key `elevenlabs_api_key`. The LLM `api_key` slot is
   never read for TTS (tested). No key -> provider downgraded to `none`, one
   log line. The key is never printed: `clipForLog` masks it in any logged
   body, and the "key never in error strings" test echoes the key from the
   fake server in four failure modes.

9. **Plain http to a non-loopback host is refused; https needs OpenSSL.**
   Copied from `AnthropicBackend`. The tests build has no OpenSSL so the
   `#ifndef CPPHTTPLIB_OPENSSL_SUPPORT` branch is what runs there and is
   tested; the CMake build found OpenSSL 3.6.3 and takes the https path.

10. **Cache key is FNV-1a 64 over text, voice_id, model_id, output_format**
    (0xFF separator between fields), 16 hex chars, files `<key>.wav` in
    `saves/tts_cache/` (default; `saves/` is already gitignored at
    `.gitignore:14`). Writes go to `<key>.part` then rename, so a crash never
    leaves a half WAV that later plays as noise. Eviction sorts by mtime and
    removes oldest until under `cache_max_mb`; a hit refreshes mtime (true LRU
    without a sidecar index). FNV over SHA because nothing in the repo has a
    hash and the key space (a few thousand lines) does not need collision
    resistance beyond 64 bits.

11. **Cache hits are not billed and not counted.** `TtsUsage.charactersSent`
    is incremented only on a 200. `TtsClient` checks the cache before the
    backend, so a hit never reaches `synthesize`. The shutdown line
    `[llm_npc] tts: session usage: requests=N characters=N cache_hits=N` is
    printed by `TtsClient`'s destructor when anything happened.

12. **Budget is checked before sending** (`sent + chars > budget` -> disable,
    not sent) and after recording (`sent >= budget` -> disable). So the budget
    is never exceeded and the line that reaches it exactly still plays.

13. **Host-only, host's own lines only.** `speech.speak` is called at the two
    sites where the local player's dialog renders an NPC reply (solo
    `session.replyArrived` and local `group`), after `announceNpcSpeech` has
    already routed text to remote players. Remote players' NPC replies are not
    spoken on the host and no audio is sent over the LAN (new `NetMessage`
    type; out of scope, filed as a suggested issue).

14. **`Speech` owns the audio device only when a provider is live**, and is a
    stack object declared after `InitWindow` and destroyed before
    `CloseWindow()`. Its destructor resets `TtsClient` first (worker joined, no
    result lands on a closed device), unloads sounds, then `CloseAudioDevice`.
    Finished sounds are unloaded in place and the vector compacted; an earlier
    draft used `remove_if` and would have unloaded the wrong handles from the
    unspecified tail — caught in review before any run.

15. **Voice map keyed by `persona().name`**, the identity every other store
    uses. `voices.cfg` lines are `npc.<name>.voice_id|model_id|backend`,
    split on the first `npc.` and the LAST `.` so names may contain dots.
    Names containing `#` cannot be voiced because `readKv` treats `#` as a
    comment; documented in the file header rather than forking the parser.

16. **Doc fetches that did not help**, so nobody repeats them:
    `https://elevenlabs.io/docs/developer-guides/error-messages` (404) and
    `https://elevenlabs.io/docs/api-reference/introduction` (no error shapes).
    The errors `.md` URL above is the one that worked.

## Review fixes

The reviewer subagent found nothing blocking and twelve items worth fixing;
all twelve are in, each with a test:

1. Invalid UTF-8 in a line made `json::dump` throw inside the worker.
   Now dumped with `error_handler_t::replace`, and the worker wraps each job
   in try/catch so a throwing backend yields a silent line (logged once)
   instead of a dead thread.
2. The key is only sent to `api.elevenlabs.io`, `*.elevenlabs.io` or a
   loopback host; any other `base_url` is declined before a request is built.
3. `TtsBackend::requestStop()` lets the client's destructor cut a pending
   retry backoff short, so quitting mid-retry no longer blocks for the
   backoff interval.
4. A cache hit is validated with `parseWavHeader`; a corrupt slot is removed
   (`TtsCache::remove`) and re-synthesised instead of played as noise.
5. The unwritable-cache-dir test now targets a path under a regular file so
   it fails for root too.
6. `lastDeclineReason_` is cleared on a delivered 200 so a repeated error is
   logged again.
7. `voice_id` must match `[A-Za-z0-9_-]+`; anything else is treated as an
   unknown voice before any URL is built.
8. The env var value is trimmed; whitespace-only falls back to secrets.cfg.
9. Numeric config values are parsed digits-only with bounds; garbage keeps
   the default with one log line instead of throwing.
10. Corrected the `Speech.hpp` lifetime comment (audio device is
    window-independent).
11. Doc comments on `TtsClient::config()` / `backend()`.
12. Tests pinned: 401 logged exactly once across two requests (stderr
    captured), the key is absent from every cache file name and body, and
    the shutdown usage line is asserted.

Left as designed: `parseWavHeader` rejects `wav_*` replies with extra chunks
before `data` (default `pcm_22050` is unaffected; noted in `WavEncode.hpp`),
fixed `.part` temp naming, FNV-1a cache key, local character limits, and
"one over-budget line disables the session".

## Acceptance criteria


| # | Criterion | Status | How |
|---|-----------|--------|-----|
| 1 | Default config is byte-for-byte today's game | PASS | 30-frame smoke run; no `tts:` line; `Speech` returns before `InitAudioDevice` |
| 2 | Request shape (path, `xi-api-key`, `output_format`, `text`, `model_id`) | PASS | `test_tts_backend.cpp` "the outbound request is the convert endpoint..." |
| 3 | Buffer is WAV, configured rate, mono, 16-bit | PASS | "raw PCM is returned as a 16-bit mono WAV..." (24000 Hz, 2205 samples) |
| 4 | 401 disables | PASS | unit |
| 5 | 402 disables | PASS | unit |
| 6 | 429 retried once then declines; 429-quota disables | PASS | unit (2 requests observed / 1 request + disabled) |
| 7 | 500 retried once then declines | PASS | unit; plus 500-then-200 delivers |
| 8 | Unknown voice declines, logs NPC id once, no second request | PASS | unit |
| 9 | Over-limit text declines without request; at-limit sends | PASS | unit |
| 10 | No network declines, not disabled | PASS | unit against a closed port |
| 11 | Budget disables | PASS | unit (two cases) |
| 12 | Character totals exclude cache hits | PASS | `TtsClient` + cache test: 2 submits, 1 request, 13 chars, 1 hit |
| 13 | Key never in error strings | PASS | fake echoes the key in 4 modes |
| 14 | WAV header bytes, parse round-trip | PASS | `test_wav_encode.cpp` |
| 15 | Cache key sensitivity, hit/miss, LRU eviction | PASS | `test_tts_cache.cpp` |
| 16 | Config keys, key precedence, downgrade, voice map | PASS | `test_tts_config.cpp` |
| 17 | `make -C tests test -j8` | PASS | 649 cases / 366,109 assertions / 0 failed (after review fixes) |
| 18 | `make -C tests portability` | PASS | floor holds at macOS 11.0 |
| 19 | `cmake --build build-tts` and `cmake --build build` | PASS | both link; no warnings in new files (pre-existing ld dylib warnings for OpenSSL only) |
| 20 | No key material in tree | PASS | `git grep -iE "xi-api-key|elevenlabs_api_key"` -> code + commented example only |
| 21 | A voiced NPC is heard at correct pitch (pcm_* is S16LE mono) | MANUAL | needs a real key and speakers; cannot be asserted without a live account |
| 22 | Second identical line is a cache hit in a live run | MANUAL | same; the unit test proves the logic, the live run proves the file-system path under the real binary |
| 23 | Wrong `voice_id` logs once, game continues | MANUAL | needs a live 404 from the real API |

## Manual verification commands


```sh
cd /Users/brandonlee/development/repos/products/cpp_games/cpp_game_with_llm_npcs

# 1. Builds and tests (all already green tonight)
make -C tests test -j8
make -C tests portability
cmake -S . -B build-tts && cmake --build build-tts -j8

# 2. Key hygiene — expect only test-key-never-real and the commented example
git grep -nI -iE "xi-api-key|elevenlabs_api_key|sk_[0-9a-f]{20}" -- . ':!external'
git log -p --all | grep -ciE "xi-api-key: [a-z0-9]"     # expect 0
git check-ignore -v saves/tts_cache config/secrets.cfg  # both ignored

# 3. Live run (host machine, speakers on)
export NPC_GAME_ELEVENLABS_API_KEY='<your key>'         # never commit this
sed -i '' 's/^provider = none/provider = elevenlabs/' config/tts.cfg
cat >> config/voices.cfg <<'V'
npc.Marge Holloway.voice_id = <a voice id from your ElevenLabs library>
V
./build-tts/cpp_game_with_llm_npcs 2>&1 | grep "tts:"
#   talk to Marge; expect to hear her, then on a repeated line:
#   [llm_npc] tts: cache hit for Marge Holloway (<16 hex>)
#   on exit:
#   [llm_npc] tts: session usage: requests=N characters=N cache_hits=M
ls -la saves/tts_cache/

# 4. Wrong voice id: change the voice_id to "nonexistent", relaunch, expect one
#   [llm_npc] tts: voice "nonexistent" for NPC "Marge Holloway" is unknown to the API (HTTP 404); ...
#   and the game keeps running.

# 5. Restore the defaults before committing
git checkout -- config/tts.cfg config/voices.cfg   # (once they are tracked)
unset NPC_GAME_ELEVENLABS_API_KEY
```

## Needs your attention

- **`pcm_*` is assumed to be signed 16-bit little-endian mono.** No fetched
  ElevenLabs page states it. Manual check 3 above confirms it by ear; if a
  voice sounds like static, that assumption is the first thing to look at.
- **Only the host hears voices.** Remote players' NPC replies are not spoken
  on the host and no audio crosses the LAN (out of scope per the spec;
  suggested follow-up issue: an `NpcAudio` NetMessage carrying the cache key).
