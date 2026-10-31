# Checkpoint
Session started: 2026-08-21 (overnight autonomous run)
Goal: work through the ready-for-ai queue
Status: IN PROGRESS

## Session deviations from the /overnight-session command file (deliberate)
- BASE BRANCH IS `dev`, NOT `main`. The command says checkout/push/PR against
  main. dev is 145 commits ahead of main; CLAUDE.md forbids pushing to main;
  tools/refine_lines.py pins PR_BASE = "dev" ("Never main - repo rule").
  Branching from main would build against 145-commit-stale code.
- NO `git add .`. The tree carries ~168 dirty paths under build_merge_check/
  (the 679 MB of committed build output that issue #259 exists to remove).
  Every commit stages explicit paths.
- Agents `overnight-coordinator` and `git-historian` are not registered in
  this repo. Using explorer / reviewer / tester / visual-qa instead.

## Issues to process (number order)
155 156 157 173 200 204 205 206 207 208 209 210 211 212 223 224 225 226
231 232 236 254 255 277 284 287

## Issues completed
Issue #155: COMPLETED -- commits 2cc59bc + learning, draft PR #298 (base dev)
  Branch: feature/issue-155-match-drives-clock (base dev)
  Change: MatchPhase::Intro + introSeconds in MatchClock (core, 3 new tests,
    9 existing updated for the new phase); advanceWorldClock() as the SINGLE
    clock-owner call site in main.cpp; --match [seconds] boot flag;
    MenuResult::NewMystery + "New Mystery" main-page action.
  Gates: tests 527 passed / 0 failed / 17 skipped; cmake build clean;
    portability floor holds.
  KEY VERIFICATION: free roam is byte-identical pre/post change -- the same
    --frames 240 --camera 0 30 180 capture hashes 5d4937aa85c299c292597e6dbc276dae
    on both binaries. That was the issue's stated make-or-break regression.
  Match verified visually: --match 1 holds the sky at dayEndHour, deep in the
    dusk band; --match 4 caught mid-sweep in daylight, as expected.
  Concept taught: Inverting a clock's ownership without disturbing its consumers
  Learning files: lessons/0028, drills/0028, vocab/0027 (+ GLOSSARY, PROGRESS)
  Review: reviewer subagent, no critical/high. Three mediums:
    1 FIXED  Menu could silently discard a live match on a misclick -- now
             refuses and toasts (Menu::setMatchActive, a bool, so Menu still
             knows nothing about MatchClock).
    2 FIXED  "assert this rather than assuming it" was only topology + a
             comment. Added WorldState::beginClockFrame() tripwire: a second,
             different writer in one frame trips an assert. Arms on the first
             frame so tests and --hour are unaffected. 5 new tests.
    3 DEFERRED to #157: nothing clears `match` at Ended, so the clock freezes
             at dayEndHour and free roam's day/night never resumes.
             Documented in code + on the issue.
    Two lows (double clamp can disagree; menu row-count budget undocumented)
    judged not worth the churn; noted in the PR body.
  Gates after fixes: 532 passed / 0 failed / 24 skipped; cmake clean;
    portability holds; free roam STILL byte-identical (same md5).
  Issue left OPEN with ai-completed, matching #280/#278/#274/#272 -- the PR is
    a draft awaiting review and `Closes` never fires from a dev-based PR (#267).

Issue #156: COMPLETED -- PR #310 (draft, base feature/issue-155-match-drives-clock)
  Branch: feature/issue-156-plaza-gather, based on feature/issue-155-match-drives-clock
    NOT on dev: the PhaseTransition this consumes is returned-and-dropped by
    advanceWorldClock, which only exists on the #155 branch (PR #298, draft).
    Noted on the issue.
  Mechanism: ONE new field, Npc::gatherTarget_ (std::optional<Vec3>), consulted
    before the schedule lookup in case NpcAction::None. No pathfinding, no
    steering, no formation logic -- the issue's economy note held.
  Map finding: the plaza's computed centre is INSIDE Gus's cart (City.cpp
    authors "cart" at x[-3,3] z[-2,2]), so zoneCentre("plaza") would walk 21
    residents into a collider. That is what the issue's "may need an authored
    marker rather than a computed centre" hint was about. Added
    Zones::plazaGatherSpot(index, count) -- a ring of radius 8, derived from
    the zone so it cannot drift from the map.
  Spec correction caught before commit: first draft released the gather on
    Vote -> Resolution. The issue says release on Resolution -> Investigation,
    so the town stays assembled THROUGH Resolution -- which is when the reveal
    happens. Now keyed on the phase being entered (to == Investigation ||
    matchEnded), so no exit path can strand them.
  Test fixture trap: the first version placed residents at (-70,-52), inside
    the bakery collider, so nobody moved and every movement assertion was
    passing/failing for the wrong reason. Fixture moved to the street at
    z = -30 with a comment explaining why.
  Gates: 542 passed / 0 failed / 48 skipped (10 new); cmake clean, no warnings;
    portability floor holds at macOS 11.0.

  Commits: 648f868 (feature, 9 files) + 7df1675 (learning materials).
  Review: reviewer subagent, no critical/high. 1 medium (plazaGatherSpot's
    docstring promised defined behaviour for count<=0 / negative index but only
    count=1 was tested -- now swept by a test), 2 lows (double blank lines;
    endMatch() returns no PhaseTransition so a future direct caller would skip
    the release -- documented at endMatch itself). All three fixed.
  Concept taught: Layering a temporary override over authored content
  Learning files: lessons/0029, drills/0029, vocab/0028 (+GLOSSARY, PROGRESS)
  Not done: visual-qa capture of the assembled plaza -- needs a live match
    reaching Vote, which is #157's HUD work. Flagged on the issue rather than
    claiming a capture that did not happen.

Issue #157: COMPLETED -- PR #311 (base feature/issue-156-plaza-gather)
  Closed #155's deferred gap: match optional clears one frame after Ended.
  Found TWO pre-existing bugs by screenshot that no test could catch:
    - page-title chain ended in `: "Multiplayer"`, so Sandbox and Model have
      been titled "Multiplayer" since they were added. Now a switch with no
      default: label.
    - main menu started at fixed h*0.5-326 stepping 72; tenth row's bottom
      edge was y=734 in a 720px window -- Quit was ALREADY off-screen before
      an 11th row was added. Layout now derives from the row count.
  Review: no critical. 1 HIGH -- a comment claimed the hand-back read
    phase.matchEnded when the code polls match->phase(), inverting what a
    reader would conclude was safe to call directly. Leftover from an earlier
    draft. Fixed. 2 mediums, 2 lows, all addressed.
  Concept: Diegetic versus HUD signalling for time pressure (lesson 0030)

Issue #304: COMPLETED -- PR #312 (base feature/issue-157-match-hud-settings)
  Taken OUT OF NUMBER ORDER, before #200, because #200 hard-depends on it:
    the leak probe must ask its question with a role block in the prompt, and
    persona_prompt could not render one. Reimplementing the composition order
    in Python would have measured the copy -- placement IS what the probe
    measures.
  Load-bearing check: an uncast persona renders BYTE-IDENTICALLY through
    --state and the plain mode. If that drifts, every with/without-role
    comparison measures the tool instead of the role.
  Verified by scripts/check-persona-prompt-state.sh (8/8); tools/ is outside
    the doctest binary entirely (tests/Makefile globs src/core/*.cpp only).
  Concept: One implementation across a language boundary (lesson 0031)

Issue #200: COMPLETED -- PR #313 (base feature/issue-304-persona-prompt-state)
  The scaffold had never run. Now runs against qwen3:8b; baseline in
    bench/ROLE-LEAK.md, raw data in bench/out/ (gitignored).
  Action leak FIXED (0 forbidden directives / 130 calls). Mood leak PASSES the
    player-exploitable rank test (killer 4th of 10, p=0.50).
  BUT the issue's literal condition FAILS reproducibly: killer vs
    secret-keepers TVD 0.59, killer vs bystanders 0.12, keepers vs bystanders
    0.69. Not a leak -- the direction is inverted. The killer reads ORDINARY;
    the SECRET-KEEPERS stand out (embarrassed on ~59% of turns). So the
    keepers provide no cover, and the killer is defended by an undocumented
    mechanism (being unremarkable among bystanders).
  My first implementation had only the rank test and reported a clean PASS.
    Adding the distributional statistic is what found this. A collapsed
    statistic is blind to structure in the other dimensions.
  Answers plan open question 1: the keeper COUNT is the wrong lever; the
    demeanour line is. Named two directions, took neither -- content calls.
  Concept: Side-channel leakage in metadata (lesson 0032)

Issue #173: SKIPPED (needs-human)
  Precondition unmet: bench_npc_render.py --gate 16.67 still fails (p99 18.42
    ms, 228/900 over, far camera). Did NOT re-run the bench -- verified the
    premise instead: git log 044de83..origin/dev on RaylibRenderer.cpp and
    CharacterParts.cpp is EMPTY, so nothing could have moved the number.
  Removed ready-for-ai: the queue was treating a BLOCKED issue as actionable.

SESSION END TASKS DONE:
  - Learning index rebuilt (was stale at lesson 0026 / vocab 0025). First pass
    over-added 9 rows for 0002-0007 and 0014-0015, which the index's own
    closing note deliberately excludes; removed them.
  - OVERNIGHT_REPORT.md written.
  - Milestone 34 filed earlier this session: #299-#307, #309 ready-for-ai,
    #308 needs-human. Scaffolds on disk, UNCOMMITTED (they carry TODOs).

NEXT: the milestone-34 queue in number order (#299, #300, #301, ...). #299,
      #300 and #301 are independent and can go first; #302 needs all three.

## Issues failed
(none yet)

## Issues skipped
(none yet)
