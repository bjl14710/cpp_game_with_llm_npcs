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
