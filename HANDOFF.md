# Session handoff — detective mode

Written 2026-08-08 for a fresh agent (remote Claude Code, or a future local
session) picking this up cold. `dev` is at `baf6e37`, suite green at 357 cases.

Every factual claim here was checked against the repo by four independent
verifiers before this was committed; the first draft had 25 errors and they are
fixed. Where something remains an assumption it says so.

---

## READ THIS FIRST — a secret is committed, on a public repo

**Two** tracked files hold the same credential blob, both live in HEAD on `dev`:

- `.claude/.credentials.json`
- `.claude/.claude/.credentials.json`

They are byte-identical (blob `b07aaaf`). **A purge must target both paths** —
scrubbing only the nested one leaves the secret fully exposed.

Contents, by field name:

| Field | State |
|---|---|
| `accessToken` | **Expired** 2026-07-05 |
| `refreshToken` | **Present**, 108 chars. Whether it still mints access tokens cannot be determined from inside the repo — assume it can. |
| `scopes` | `user:file_upload`, `user:inference`, `user:mcp_servers`, `user:profile`, `user:sessions:claude_code` |
| `subscriptionType` | `max` |

**`main` is clean.** Commit `1e0cd87` is *not* an ancestor of `main`, so the
default branch a visitor lands on does not expose the file. The leak is confined
to `dev` and its descendants, which makes remediation much cheaper than it
first looks.

`1e0cd87` ("saving claude files to be used on remote") changed **6,841 paths**,
**6,515** of them under `.claude/` — a whole `~/.claude` home directory,
including 99 session transcripts, 429 file-history entries, shell snapshots,
telemetry, and a full Python 3.12 venv under `.claude/security/`. `.gitignore`
gained an allowlist later (PR #146) that stops *new* files landing there, but
**gitignore does not untrack what is already tracked**.

Two steps, in this order:

1. **Rotate.** Log out and back in to Claude Code. The refresh token is the part
   that matters. Until this happens, step 2 alone is useless — the value is
   already public.
2. **Purge from `dev`'s history** (`git filter-repo` or BFG), targeting both
   paths, then force-push. The repo hook requires manual approval for
   force-pushes.

**Do not repeat the pattern.** If the goal is to give a remote agent context,
that is what this file is for. Never commit `~/.claude`.

---

## Original goal

`bjl14710/cpp_game_with_llm_npcs` is a C++17 / raylib 5.5 first-person game with
LLM-driven NPCs. The current arc is **detective mode**: a Clue-like multiplayer
mystery in a nine-zone downtown, where one of 21 residents is dead, one is the
killer, and players interrogate LLM-driven NPCs across a three-day match to work
out which.

`.claude/plans/` holds 40 plan documents. **Fourteen** of them are the
detective-mode set, indexed by `.claude/plans/DETECTIVE_MODE_INDEX.md`. **Read
that index first** — it carries the build order and a list of measured findings
that are the whole reason the plans are worth keeping.

---

## Current state

### Shipped and merged to `dev`

| Milestone | What landed |
|---|---|
| #18 NPC line bank | `TopicMatcher`, `LineBank`, banked serving through `LlmClient`, `tools/eval_lines.py`, `tools/refine_lines.py` |
| #19 Game-day phases | `MatchClock` — day counter, four-phase machine, match-paced world hour |
| #20 Zones & alibi log | `Zones` (nine named regions), `LocationLog` (transition-based) |
| #21 Residents | Roster diversity gate (`tests/test_persona_roster.cpp`) |
| #22 Opening murder | `Mystery.{hpp,cpp}` — `generateMystery`, `seedMysteryFacts`, `placeBodyClearOfColliders`, `startVictimDead`, `voteIsCorrect` |
| #23 Storyline templates | `Storyline.{hpp,cpp}` — parser, structural validator, deterministic casting |

**No open PRs.** All seven from the last two sessions (#182–#185, #190–#192) are
merged.

### The gap is integration

**The detective-mode entry points have no caller outside tests.** `MatchClock`
is never ticked, `generateMystery` is never invoked, no storyline is ever
loaded, and `LocationLog::observe` is never called from the game loop.
`src/app/main.cpp` still runs free-roam exactly as before.

That is the single most important fact for whoever picks this up.

Two things are *not* in that gap, and it would be wrong to redo them:

- **The line bank from #18 is already wired in.** `main.cpp:234` constructs
  `std::make_unique<LineBank>(projectRoot / "banks", …)` and `main.cpp:239`
  calls `client.setLineBank(...)`, through `LlmClient.hpp:125,162`. It is off by
  default (`line_bank = off` in `config/llm.cfg`) and there are no `.bank` files
  yet — that is issue #153.
- **`Zones` has production callers** — `Mystery.cpp`, `Storyline.cpp` and
  `LocationLog.cpp` all use it. Just nothing in `src/app/`.

### Untouched

- Idea 7 (storyline templates) has **no plan document** — the one item of
  fourteen that never had an `/idea` run. Milestone #23 was built from
  requirements other plans impose on it. See "Open questions".
- Learning materials (`docs/learning/`) have not been generated for five
  sessions despite `/teach`, `/drill-me` and `/terms` being available.

---

## Next steps, in order

1. **#155 — drive the world clock from the match.** The keystone: it turns
   `MatchClock` from a tested unit into the thing that runs the game.
   **Widen the issue before starting.** `detective-day-phases.md:121` assigns
   "minimal match entry and the `Intro` phase" to #155, but the issue body
   mentions neither and caps the change at "under 30 lines". Plan and ticket
   currently disagree about what "done" means.
2. **#157 — day/phase HUD and match settings.** Cheaper than it looks: its
   stated dependency is #154, which is **already merged** (PR #167), so it is
   unblocked today. It does not depend on #155.
3. **#175 — four new core parts**, taking the core silhouette pool 12 → 24.
4. **#170 → #171 — render perf.** 21 NPCs cost 35.3 ms/frame today.
   **#171 is described in its own issue as "the gate for the whole milestone".**
5. **#173 — eleven new residents.** Its body says **"BLOCKED until the render
   gate passes"** and "do not start until `bench_npc_render.py --gate 26.4`
   passes". So #173 comes *after* #170/#171, not before.
6. **#156 — plaza gather for the vote.** Depends on #155.
7. **#166 — building signage.** Wants a font atlas (see Gotchas).

`#189` and `#153` need a human decision first — see Open questions.

---

## Key decisions, and why

| Decision | Why | Recorded in |
|---|---|---|
| Mystery generation is **deterministic from a seed** | Tests cannot assert on generation otherwise, and the host must be able to replay a match without shipping the answer over the wire | `Mystery.cpp` |
| `World::rng_` is **never** reused for it | It drives combat accuracy rolls — sharing would make generation depend on how many shots were fired | `Mystery.cpp` |
| Murder hour is `[20:00, 24:00)` the night before | Day one starts 09:00 and the town already knows. Stopping short of midnight keeps every "before or after?" comparison single-case | `Mystery.cpp` |
| The death fact names the place but **not the hour** | The hour is what an alibi is checked against; giving it away free removes the reason to cross-check testimony | `seedMysteryFacts` |
| `setup.killer` has a **short enumerated reader list** | Leak defence. Currently three: `voteIsCorrect`, `castStoryline`, and the gated reveal. The list is in `Mystery.hpp`'s header comment — **keep it current, it went stale once already** | `Mystery.hpp` |
| The reveal is a **preprocessor gate**, not a config key | Anything runtime-readable can be flipped by whoever is hosting | `LLM_NPC_REVEAL_KILLER` |
| A lie is an **ordinary fact**, no `isLie` flag | Same subject, different content — `Journal.hpp` already flags contradictions | `alibis-and-testimony.md` |
| Authored content is `key = value` text, not JSON | A night of generated content has to review as a normal diff in a PR | `banks/README.md`, `storylines/README.md` |
| Single-player starts like multiplayer, with 3–4 guesses | Owner's decision, 2026-08-08 | **`vote-and-retaliation.md:26,36-50`** — not in issues #155–#157, which do not mention it |

### Rejected alternatives worth not revisiting

- **`std::mt19937`** for mystery generation — works, but makes the
  cross-platform determinism contract non-obvious at the call site.
- **Random retries** for body placement — can miss a clear spot that exists and
  answers differently each run. Replaced with a 24×24 lattice scan.
- **Banning the killer's name from seeded facts** — would forbid the very clues
  the mode is made of. The leak test asserts on the *pairing* of the name with
  death language instead.

---

## Gotchas

These cost real time to discover. Several contradict what the plans say.

**A green test suite does not prove the build works.** `tests/Makefile` **globs**
`../src/core/*.cpp`; the root `CMakeLists.txt` lists sources **explicitly**. A
new `.cpp` is picked up by the tests automatically and missed by cmake. This has
broken the build once already (`e67ab67`). **Always run both.**

**doctest cannot decompose `&&`** inside an assertion — "Expression Too Complex
Please Rewrite As Binary Comparison". Assign to a `bool` first. Message
expressions containing operators must be parenthesised.

**Silhouettes: 12 in the core pack, 28 in the full catalog.** The core pack has
6 bodies (3 `round`, 3 `blocky`) × 4 heads (2 `round`, 2 `blocky`) = **12**, not
the naive 24, because families cannot mix. The catalog also ships a
`quaternius` pack (4 bodies × 4 heads = 16 more) which `lookIsValid` accepts —
but shipped residents draw from **core only**, via `coreParts()` in
`tests/test_persona_roster.cpp:135-141`. #175 exists to take core from 12 → 24.

**There are three style families, not two, and `any` bridges them.** `round`,
`blocky`, `quaternius`, plus an `any` tag. `tagsCompatible` returns true when
either side is `any`, which is why every eye, hair and mouth part pairs with
both round and blocky — and why the count is 12 rather than 6. `quaternius` is
hard-exclusive and rejects even `any`. No *body* or *head* is tagged `any`, so
"families cannot mix" holds for silhouettes specifically.

**`NpcDamagedEvent` is emitted by `World::updateCombat`, not `takeDamage`.**
`opening-murder.md` says otherwise. `Npc::markDeadAtStart()` is still the right
entry point for the victim, but for intent, not because an event fires today.

**The zone centre is not guaranteed collision-free.** `opening-murder.md` offers
it as a safe fallback; the bakery block's centre `(-64, -64)` sits exactly on
Marge's Bakery's south edge. `placeBodyClearOfColliders` still falls back there
but logs that the body may be inside a building.

**`ConversationStore` keys rows by `Persona::name`** ("Marge Holloway"), not the
file stem ("baker"). This silently made `refine_lines.py` skip the entire cast
once.

**The NPC radius exists four times and has already diverged:**

| Where | Name | Value |
|---|---|---|
| `Npc.cpp:14` | `kNpcRadius` | `0.45f` |
| `Mystery.cpp:45` | `kBodyRadius` | `0.45f` |
| `SandboxMap.cpp:18` | `kBodyRadius` | `0.45f` |
| `World.cpp:66` | `kNpcRadius` | **`0.4f`** — projectile hit radius |

All four are anonymous-namespace or function-local, so none is reachable from
the others. Changing "both" is not enough.

**qwen3:8b leaks guilt through the mood tag.** Measured: an innocent Marge
laughs when accused (`[[MOOD: amused]]`); a guilty one goes `[[MOOD: angry]]` on
turn one. `NpcMoodUpdate` is broadcast to every client, so it is worse in
multiplayer. Holding the *lie* is not the problem — zero confessions across five
escalating turns. See `role-layer.md`.

**The bitmap font's glyph spacing is integer, so sizes 14–18 *space*
identically** — the glyph bitmaps still scale, only inter-glyph spacing is
quantised. Three features now want a font atlas (signage, cutscene captions,
aesthetics), each working around it independently.

**GitHub's API index lags writes.** A PR or issue created seconds ago will not
appear in list/label queries — hit five times in one session. For mergeability,
`git merge-tree --write-tree origin/dev origin/<branch>` is authoritative and
instant.

---

## What a remote environment will NOT have

Verified against the repo. The target is a Linux container.

| Missing | Consequence |
|---|---|
| **Ollama + `qwen3:8b`** on `localhost:11434` (`config/llm.cfg`) | `tools/eval_lines.py` gates 3–4, `tools/bench_npc_models.py`, `tools/bench_schema_models.py` cannot run. `tests/test_llm_live.cpp` self-skips unless `OLLAMA_LIVE=1`, so **the suite still goes green**. |
| **`saves/` (gitignored, untracked)** | `tools/refine_lines.py` is **unrunnable on remote for data reasons alone** — a fresh clone has zero transcripts and zero ratings. It also needs PyGithub installed and reads `GH_TOKEN`/`GITHUB_TOKEN` from the environment directly, so it cannot use Claude Code's own GitHub integration. |
| **Game assets** | A fresh clone has no `assets/models` or `assets/fonts`; run `tools/fetch_assets.sh`, which needs outbound HTTPS to github.com plus `curl`, `unzip` and `shasum`. **`fetch_assets.sh:54` uses `shasum -a 256 -c -`** — a perl script present on macOS but frequently absent on slim Linux images, where the equivalent is `sha256sum`. The fetch dies at the checksum step, not the download. **The unit tests do not need assets**, so this only blocks running the game. |
| **A display / GPU** | The `visual-qa` agent and every screenshot workflow are unavailable. Any ticket with visual acceptance criteria cannot be verified — say so rather than marking it done. |
| **`TOKEN` env var** | Local sessions do GitHub via PyGithub with `GH_TOKEN="$TOKEN"`. Remote Claude Code has its own GitHub integration — use that. (Exception: `refine_lines.py`, above.) |
| **A warm raylib checkout** | The first cmake configure runs FetchContent and downloads raylib 5.5. Slow but automatic. |

### Build and test commands

```bash
make -C tests test                                  # 357 cases, 3 skipped, must stay green
cmake -S . -B build && cmake --build build -j8      # must ALSO be run; see the glob gotcha
```

---

## Test status

**357 cases, 357 passing, 3 skipped, 0 failing** on `dev` at `baf6e37`. Clean
cmake build, no warnings.

The 3 skips, precisely:

- `tests/test_llm_live.cpp:37` — conditional, needs `OLLAMA_LIVE=1`
- `tests/test_persona_roster.cpp:178` — "the core catalog can seat twenty-one
  residents", **unconditionally skipped**
- `tests/test_persona_roster.cpp:285` — "the roster is the full twenty-one
  residents", **unconditionally skipped**

The last two are aspirational and tie directly to the silhouette gotcha and
issue #175. `test_net_loopback.cpp` has **no** skips — all 9 of its cases run.

Coverage is strong on pure logic (`Mystery`, `Storyline`, `MatchClock`, `Zones`,
`LocationLog`, `TopicMatcher`, `LineBank`) and **absent on integration**,
because there is no integration yet.

---

## Open questions — these need the owner, not an agent

1. **Must a storyline template prove it is solvable, or only be structurally
   sound?** Asked twice, never answered. Milestone #23 was deliberately split so
   #186–#188 shipped without it. Issue **#189** carries the analysis: a true
   solvability check is **not computable** from the current format, because clue
   captions are prose and nothing affords an elimination predicate. Three
   options are on the issue; the recommendation is to rename the metric to
   "chain coverage".
2. **Idea 7 has no plan document.** Milestone #23 was reconstructed from
   requirements in `win-cutscene.md`, `opening-murder.md`,
   `alibis-and-testimony.md` and `role-layer.md`. Running `/idea` for storyline
   templates properly would settle question 1 as a side effect.
3. **Is each networked player a distinct knowledge agent?** `journalEntries`
   reads facts known by the single agent id `"player"`. Unresolved, and it
   affects the testimony layer, the vote and the win cutscene.
4. **#153** — authoring the ten persona banks needs human content review. Until
   it lands, `line_bank = off` and there are no `.bank` files.
5. **The credential above.**

---

## Working rules the owner has set

- **Never merge, never push to `main`.** Open **draft** PRs based on `dev`; the
  owner clicks every merge.
- **One PR per ticket**, stacked so each diff is exactly one ticket's worth.
  Descriptions explain *why*, and class members get comments.
- **Never `git add .` or `-A`.** Stage explicit paths only — this is how 6,515
  files got committed.
- Commit format `type(scope): description`, trailer
  `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`.
- Do not modify `.env` or anything in `/secrets/`. `config/secrets.cfg` is
  gitignored.
- Standards skills (`python-standards`, `c-cpp-standards`,
  `complexity-reduction`) are **opt-in** — see `CLAUDE.md`.

---

## Suggested first move

**Rotate the credential.** Five minutes, and it is the only item here with a
blast radius outside this repo.

Then **take #155**. Everything built over the last three sessions is a tested
unit that nothing calls; #155 is the ticket that turns the pile into a running
match. Widen its scope first — see Next steps.

If you want a cheaper warm-up: **#157 is unblocked today** and nobody has
noticed.
