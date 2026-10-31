# Checkpoint — SESSION 12 (diversity gate + a catalog blocker)  <- MOST RECENT
Session 12: 2026-08-08
Goal: milestones #19/#20/#21 remaining queue (#156,#157,#165,#166,#170-#173)
Status: PARTIAL — 1 completed, 3 blocked-and-labelled, 4 not reached.

Completed: [#172 ee63552 -> PR #174]
Blocked:   [#156, #157 needs-human behind #155 (nothing starts a match);
            #173 needs-human on a CATALOG blocker, see below]
Not reached: [#165, #166, #170, #171]

## THE FINDING OF THIS SESSION
The silhouette budget is HALF what the 21-residents plan assumed. Parts carry
a style family (round / blocky) and lookIsValid rejects any mix, so 6 bodies x
4 heads is 12 valid combinations, not 24:
    3 round bodies  x 2 round heads  = 6
    3 blocky bodies x 2 blocky heads = 6
The current ten already use TEN of the twelve. 21 distinct silhouettes is
unreachable from this catalog by any arrangement — independent of framerate.
So #173 is blocked on new core parts, NOT on the render gate as the plan
assumed. Minimum 3 new parts (round body + round head + blocky head) = 21
exactly; 4 gives 24 with headroom. New modelling work + draw recipes; wants
its own issue.

Found by trying to fix a REAL shipped collision: barista and librarian both
wore body_slim/head_oval. Moving barista to head_tall failed validation with
"mix incompatible styles", which is how the family rule surfaced. Barista is
now body_pear/head_oval (the only unused round-family pair).

## Second finding
NO shipped persona uses the structured trait library. All ten have empty
traitIds; traits/*.trait ships eight traits nothing references. The trait-set
gate therefore checks the free-text `traits =` line (unique 10/10), and a test
asserts traitIds stay empty so the day someone uses them it prompts moving the
gate to the stronger field.

## Branches (pushed)
- feature/issue-172-roster-diversity-gate -> PR #174 (base dev)
- feature/residents-scaffold (scaffold, no PR — folded into #174's base)

## Still open from earlier sessions
- PRs awaiting review: #158-#162 (line bank), #167-#169 (match clock, zones),
  #174 (diversity gate). Nine draft PRs total.
- #155 question unanswered: how does a single-player match start?
- Learning materials still not generated (0027/0026 free).
- SECURITY: credential commit 1e0cd87 still in origin/dev history.

# Checkpoint — SESSION 11 (day phases + zones)  <- MOST RECENT
Session 11: 2026-08-07
Goal: milestones #19 (#154-157) and #20 (#163-166)
Status: PARTIAL — 3 of 8 done, 1 blocked on a design decision, 4 not reached.

Completed: [#154 10016a1 -> PR #167, #163 c1721f3 -> PR #168,
            #164 02029fc -> PR #169]
Blocked:   [#155 needs-human — nothing starts a match; #156/#157 depend on it]
Not reached: [#165, #166]

## Branches (all pushed)
- feature/issue-154-match-clock   -> PR #167 (base dev)
- feature/zones-scaffold          -> pushed, no PR (folded into #168)
- feature/issue-163-zones-lookup  -> PR #168 (base dev)
- feature/issue-164-location-log  -> PR #169 (base 163's branch, STACKED)

## The blocker, in one line
#155 says "when a match is running, drive the clock" — but no issue creates
the concept of starting a match. That is a product decision (menu entry? CLI
flag? default mode?), not wiring, so it was not guessed at. #156 and #157
inherit the block.

## Notable
- MatchClock had to be recreated: its scaffold was lost in session 10's
  rebase. Implemented directly this time, 14 tests, all green first run.
- LocationLog::closeAgent had to RETIRE an agent permanently, not just close
  the stay. Writing the test surfaced it: the caller ticks every agent every
  frame, so a corpse would open a fresh visit and log forever.
- Suite ended at 241 passing / 1 skipped (llm_live) on the zones branch;
  236 on the match-clock branch. cmake green on both.
- Learning materials still not generated (0027/0026 free). /teach, /drill-me,
  /terms live in the unmerged toolkit PR #146.
- SECURITY still open: credential commit 1e0cd87 in origin/dev history.

# Checkpoint — SESSION 10 (NPC line bank, milestone #18)  <- MOST RECENT
Session 10: 2026-08-06/07
Goal: milestone #18, issues #148-#152. Plan: .claude/plans/npc-line-bank.md
Status: COMPLETE (5/5) — five stacked draft PRs open (base dev), all issues
closed ai-completed.

Completed: [#148 bfc661d, #149 becb193, #150 35a3113, #151 6a78734, #152 0e9d91a]
Failed: []  Skipped: []
PRs: #158(148)<-dev, #159(149), #160(150), #161(151), #162(152) — STACKED,
     each based on the previous branch so each diff is one ticket only.

## Session 10 deviation from sessions 5-9 precedent
- USER DIRECTIVE mid-session: one PR per ticket, not one PR per milestone.
  Implemented as a stacked chain. Merge bottom-up: #158 first, then #159...
- Everything else per precedent: base dev, never main, never `git add .`,
  local-only OVERNIGHT_REPORT.md / docs/learning/** / .claude/**.

## Things found by running, not reading
- cmake was BROKEN on entry: e67ab67 added src/core/MatchClock.cpp to
  CMakeLists but the scaffold file is not on this branch (it belongs to
  milestone #19). Fixed in 50c445f. NOTE: tests/Makefile globs src/core/*.cpp,
  so `make -C tests test` stayed green the whole time — a green test suite
  does NOT prove cmake builds.
- ConversationStore keys by Persona::name ("Marge Holloway"), NOT file stem
  ("baker"). refine_lines silently skipped the entire cast until fixed.
- The real transcript corpus is 20 player lines across 8 characters, nothing
  recurring. Bootstrapping banks from play data is not viable yet — issue #153
  (seed ~40 topics by hand) is the actual next step, not more nightly runs.

## Still open / next
- #153 (needs-human): author the ten banks, then flip line_bank = on.
- Milestone #19 (#154-#157, day phases) untouched. Its MatchClock scaffold was
  lost in the rebase and needs recreating on its own branch.
- SECURITY, UNRESOLVED: credential commit 1e0cd87 is still in origin/dev's
  history (.claude/.claude/.credentials.json). Never rotated, never
  force-pushed. Pre-dates this session; user's call.
- Learning materials (lesson/drill 0027, vocab 0026) NOT generated this
  session — see OVERNIGHT_REPORT.md.

# Checkpoint — SESSION 9 (stylized character assets)  ← MOST RECENT
Session 9 started: 2026-07-14
Goal: milestone #17, issues #137-#143 on feature/stylized-characters
(scaffolded, base dev). Plan: .claude/plans/stylized-character-assets.md.
Skill: .claude/skills/stylized-characters/SKILL.md — ENGINE IS RAYLIB, NOT
GODOT (premise correction; never fetch godotshaders/create .gdshader).
Status: COMPLETE (7/7) — draft PR #144 open (base dev), all issues closed ai-completed
Completed: [#137 fa63be3, #138 165141b, #139 71002ab, #140 da9fffa,
            #141 f9639a5, #142 d38ea32, #143 7ea9a96]
Failed: []  Skipped: []
Session highlights: bind-vs-rest re-bake (Assets::rebakeVerticesToRestPose)
unlocked Tier-B locomotion; strict quaternius family gate; face decals with
mood variants (billboard retired for the family); visual-qa gate failed→
fixed→passed (viewmodel outline). Suite 221/235,580. Follow-up candidates
in OVERNIGHT_REPORT.md (KayKit shading gap, decal steep-angle, worldgen
plan still un-queued).

## Session 9 deviations (sessions 5-8 precedent)
- One branch, atomic commit per issue, ONE draft PR (base dev) at the end;
  labels ai-completed + close at PR time via PyGithub recipe.
- Never git add . — explicit paths; local-only: OVERNIGHT_REPORT.md,
  docs/learning/**, .claude/**, docs/qa/**.
- Un-skip tests/test_stylized_parts.cpp stubs in the implementing commit.
- Tier A/B animation policy per skill: static assembly COMMITTED, rigged
  bone-attach ATTEMPTED (timeboxed) — Tier A must never regress.
- Learning numbers next: lesson/drill 0027, vocab 0026.
- worldgen-fix-and-catalog plan exists but is NOT queued (no issues) —
  do not implement it this session.

# Checkpoint — POST-SESSION-8 branch audit + pluggable-llm port (2026-07-14)
- Audit: ~30 branches content-empty (user's merge commits only); SFML lines
  hold unported FEATURES — .claude/plans/legacy-feature-gaps.md (economy,
  enterable buildings, traffic, crowds, graduated law, 5x5 town).
- PORTED 8-pluggable-llm-and-one-click -> PR #135 -> USER MERGED into dev
  (c8e66dd). Port audit finding fixed: think=false fast-path re-added to
  OllamaBackend; SFML strays excluded; model picker re-built on raylib Menu.
- VERIFIED ON DEV (c8e66dd): build clean; suite 216/32,121 green; Ollama
  path live; CLOUD PATH LIVE (provider=openai + base_url=
  http://localhost:11434/v1 -> valid map, 2s); boot/screenshot unchanged.
  llm.cfg restored to provider=ollama after the test.
- dev == everything; main one merge behind (user rolls up).

# Checkpoint — SESSION 8 (sandbox/traits/groups/worldgen queue)  ← MOST RECENT
Session 8 started: 2026-07-13
Goal: milestones #13-#16 in order — issues #109-#114 (sandbox), #115-#119
(traits), #120-#124 (groups), #125-#129 (worldgen).
Status: SESSION 8 COMPLETE — all four milestones shipped.
Completed: [#109-#129, all 21]  Failed: []  Skipped: []

## Session 8 final state (read first on resume)
- PR chain (merge top-down, USER clicks every merge):
    main <- #130(dev) <- #131(sandbox) <- #132(traits) <- #133(groups)
         <- #134(worldgen)
- Suite: 204 cases / 32,078 assertions green; llm_live only skip.
- Bench (#127): qwen3:8b 50% valid (winner, = dialogue model),
  qwen2.5 25%, gemma3 0%; gen-village unsolved by all — retry chain +
  editable drafts absorb it. bench/REPORT.md has the table.
- OVERNIGHT_REPORT session-8 section complete (4 milestones, decisions,
  bench, findings/follow-ups: ambient chatter design sketch, cart mesh,
  cast position bounds, editor QoL).
- Learning: lesson/drill 0026 generate-validate-retry + vocab 0025
  (LOCAL); next free numbers: lesson/drill 0027, vocab 0026.
- No ready-for-ai issues remain. Next work needs new /idea or the
  follow-ups above.

## Milestone #15 outcome (groups)
- feature/group-conversations (base npc-traits): 645d863 #120 follow gate
  (Npc::applyAction VALIDATE: Hostile/Fleeing/Angry drop Follow) | 52da198
  #121 GroupSession core | 539c8e0 #122 wiring (group turns ride npc.ask +
  pendingRoutes; groupPendingId gates streaming; NPC->NPC cap 2; <=3 NPCs)
  | 5a597fd #123 per-perspective memories + attribution (by construction:
  each turn in speaker's own history). PR #133. Suite 199/32,034.
- #124: latency logged per turn to stderr "[llm_npc] group turn: Xs";
  ambient chatter did NOT fall out (needs proximity trigger + overhear UI)
  — write as follow-up in OVERNIGHT_REPORT. Group ratings solo-only.

## NEXT: milestone #16 (worldgen #125-#129) on feature/llm-world-generation
  (branch off group-conversations). Scaffold present: WorldGenValidate.hpp,
  tests/test_worldgen_validate.cpp (4 stubs), tools/bench_schema_models.py.
- #125 validator (offline): validateCast (parsePersonaText+lookIsValid+
  trait ids+unique names+nearest-id suggestion), extractJsonObject,
  renderRetryFeedback; reuse validateMap for maps. CMake: WorldGenValidate.cpp.
- #126 cast gen: WorldGen.{hpp,cpp} prompt builders + parse + retry state
  (pure parts tested offline); live loop via client internal requests.
  IF Ollama down: implement offline parts, mark live smoke needs-human.
- #127 bench: implement bench_schema_models.py + validate CLI mode
  (persona_prompt pattern); runs only if models present — else blocked note.
- #128 map gen (strict validateMap gate); #129 combined + sandbox
  "Generate..." entry (text field like joinAddress_ on Sandbox page).
- FINAL WRAP after #16: OVERNIGHT_REPORT session-8 (all 4 milestones, QA
  verdicts, cart finding, latency numbers, ambient-chatter follow-up, v1
  cuts), learning materials LOCAL (lesson/drill 0026 compile-to-existing-
  systems + vocab 0025; numbers 0027+ free), PROGRESS/index updates,
  planning-session note. PRs chain: #130(dev->main) <- #131 <- #132 <-
  #133 <- (#16's PR).

## Milestone #14 outcome (traits) — facts for later milestones
- feature/npc-traits (base feature/sandbox-map-editor): a98de1d #115
  loader+8 traits | 3172a7a #116 composition (order: identity < rules+
  examples < memory < REINFORCEMENT < ACTIONS, index-pinned test) |
  41ab768 #117 creator picker (ONE trait via UI, 3 via file — logged) |
  6080199 #118 ratings (F1/F2 — logged: +/- collides with text input;
  saves/ratings/*.jsonl; prompt-unchanged pinned) | b31fd24 #119 doc
  docs/design/TRAIT_FINE_TUNE_PATH.md. PR #132, issues closed. Suite
  193/32,004.
- Key APIs: loadAllTraits(dir,&errs); Persona.traitIds; renderSystemPrompt
  (memory, gossip, vector<const TraitDef*>); Npc::setTraitRegistry(ptr)
  + resolvedTraits() (registry installed in resetNpcSideArrays);
  RatingLog(dir).appendCandidate/appendRejected.

## Milestone #13 outcome (sandbox editor) — resume here
- feature/sandbox-map-editor (base dev): 4d1dd03 #109 contract | b4149dc
  #110 seam | ce6230a #111 assets+--map | 4d787eb #112 editor | 4125982
  #113 npc placement | fb3e0db #114 --map spawns NPCs. PR #131 (draft,
  base dev), issues #109-114 closed ai-completed. Suite 188/31,953.
- Visual-qa PASS; finding (not regression): hotdog cart = generic crate
  (same as downtown; fix = curated model choice for "cart").
- v1 cuts logged: no piece rotation; road-paint pieces schema-only (no
  visual pieces in catalog); New auto-names map-N (no name field);
  schedules suppressed in sandbox play.
- Key APIs for #125-#129 (worldgen): SandboxMap::toJson/fromJson,
  validateMap (errors ARE retry feedback), buildCity, resolvePlacedNpc.
- Fixture: saves/maps/fixture-village.json; boot: --frames 90 x.png
  --camera 0 30 180 --hour 12 --map saves/maps/fixture-village.json
- QA shots: docs/qa/screenshots/sandbox/ (local).

## NEXT: milestone #14 (traits, #115-#119)
- Branch feature/npc-traits OFF feature/sandbox-map-editor (stack).
- Scaffold already present (untracked): src/core/Trait.hpp,
  traits/grumpy.trait, tests/test_traits.cpp (5 skipped stubs).
- Read .claude/plans/npc-traits-and-ratings.md + the sandbox-worldgen
  skill. Trait.cpp goes in CMakeLists source list too (like SandboxMap).
- Then #15 groups (feature/group-conversations off npc-traits; scaffold
  GroupSession.hpp + test_group_session.cpp present), #16 worldgen
  (feature/llm-world-generation; scaffold WorldGenValidate.hpp +
  test_worldgen_validate.cpp + tools/bench_schema_models.py present).
- Each milestone: one branch, atomic commits, ONE draft PR (base = prev
  branch), close issues ai-completed, learning materials LOCAL-ONLY.
- OVERNIGHT_REPORT session-8 section still to write (do at final wrap;
  include milestone-13 QA verdict + cart finding + v1 cuts above).
- Learning numbers: session 8 uses lesson/drill 0026+ vocab 0025+
  (0026 = compile-to-existing-systems, from #13; write at wrap).

## Session 8 environment/deviations (session 5-7 precedent + new)
- NEW: chains now target `dev` (created at 4d94b5d = full completed game;
  PR #130 dev->main awaits the USER'S merge click — never merge it).
- feature/sandbox-map-editor reset onto origin/dev (scaffold files are
  untracked, they survive). Chain: dev <- sandbox <- npc-traits <-
  group-conversations <- llm-world-generation.
- One branch + one atomic commit per issue + ONE draft PR per milestone
  (base = previous milestone branch; first = dev). Labels at PR time.
- Read .claude/skills/sandbox-worldgen/SKILL.md before every issue.
- PyGithub recipe; never git add .; local-only: OVERNIGHT_REPORT.md,
  docs/learning/**, .claude/**, docs/qa/**.
- Un-skip each scaffold test stub in the commit that implements it.
- Learning numbers next: lesson/drill 0026+, vocab 0025+.
- buildCity needs a City factory for custom buildings — add static
  City::fromBuildings(buildings, halfSize) (City.hpp), logged.

# Checkpoint — SESSION 7 (mii-style visual overhaul)
Session 7 started: 2026-07-10
Goal: work ready-for-ai issues #101-#107 (milestone #12, plan
.claude/plans/mii-style-visual-overhaul.md, skill .claude/skills/mii-style/SKILL.md)
Status: COMPLETE — all seven shipped, ONE draft PR #108, suite green.
Issues: Completed [#101-#107]  Failed []  Skipped []

## Session 7 outcome (read first on resume)
- Branch feature/mii-style-visual-overhaul (stacked on punch-and-jump):
    98e04a9 #101 pack seam | 56df349 #102 proportions+pastels |
    4b4f4a8 #103 outlines | 8b0c70d #104 Mouth+6-item look |
    c8188d6 #105 expansion | f487dd1 #106 avatar | (#107 = QA gate)
- PR #108 (draft, base feature/punch-and-jump). Chain now:
    #94 <- #99 <- #100 <- #108. Issues closed ai-completed, milestone #12.
- Suite: 182 cases / 31,730 assertions (combo test sweeps 17,280 combos).
- Part A verdict: NO engine migration (recorded in plan + report).
- Visual-QA: PASS w/ caveat — mood-emote billboard clashes with outlined
  style (finding #1, NOT fixed per observations-only brief). 10 playtest
  findings in OVERNIGHT_REPORT.md session-7 section.
- Key new facts: PartCategory has FIVE slots (Mouth); look format is six
  items (five-item + missing-JSON-key default to mouth_smile); recipes
  live in drawPartRecipe (RaylibRenderer.cpp) — packs = catalog rows +
  palettes + recipe branches, tagged via PartDef/PartPalette::pack;
  outlines = grouped inverted-hull passes w/ rlDrawRenderBatchActive
  around rlSetCullFace; avatar = look-only store row "player_avatar"
  (never spawns), viewmodel tints via renderer.setAvatarPalette.
- Learning 0025 (inverted hull) + vocab 0024 LOCAL-ONLY; next: 0026/0025.
- QA shots: docs/qa/screenshots/mii-style/ (gitignored).

## Locked deviations from the literal /overnight-session command (session-5/6 precedent)
- BASE = feature/mii-style-visual-overhaul (scaffold already on it,
  uncommitted), stacked on feature/punch-and-jump. main lacks this code.
  Chain: #94 <- #99 <- #100 <- this branch's PR.
- ONE branch, one atomic commit per issue, ONE draft PR at the end.
  Issue labels (ai-completed) + close applied at PR time via PyGithub.
- No gh CLI: zsh -c 'source ~/.zshrc; export GH_TOKEN="$TOKEN"; python3 ...'.
  Never print the token. Retry WAF 400s.
- NEVER git add . / -A. Stage explicit paths: src/, tests/, personas/,
  .gitignore. .claude/**, docs/learning/**, OVERNIGHT_REPORT.md, docs/qa/**
  stay LOCAL-ONLY (never committed).
- overnight-coordinator / git-historian agents not in registry → inline.
  Plan step per issue = the master plan + skill file (already written).
- Scaffold files (pack fields, test_style_pack.cpp, TODO markers,
  .gitignore entry) fold into #101's commit; later commits remove their
  TODO markers as steps land. Un-skip each test stub in its step's commit.
- Learning numbers next: lesson/drill 0025+, vocab 0024+.

# Checkpoint — SESSION 6 (shared character library)
Session 6: 2026-07-08. Status: milestone COMPLETE — PR #99 (draft, base
feature/aim-styles-preview), issues #95-#98 closed ai-completed, milestone #11.
- Branch feature/shared-character-library: ee7c07e #95 persona look |
  fc336b6 #96 unify NPC rendering | 119db76 #97 hair 6->12 | 53584f3 #98
  ten authored looks. Suite 173/3323 green. Visual-QA subagent PASS
  (shots in docs/qa/screenshots/shared-character-library/, local).
- NPCs are now ALL composites (drawCompositeCharacter + face/dead params);
  rigged models dormant (remote players + unknown net indices only).
- Reorder logged: hair expansion (#97) landed BEFORE authored looks (#98)
  so the cop could wear hair_cap.
- USER REQUESTS — DONE on feature/punch-and-jump (PR #100, draft, base
  feature/shared-character-library): 863485e camera-locked viewmodel
  (fist hidden at rest, punches on click; pistol slide/barrel/grip/hand +
  recoil) | d5f1360 height-aware collision + Space jump (City
  circleIntersectsAny gains feetY param default 0, supportHeightAt;
  gravity in main; Action::Jump rebindable). Suite 176/3337 green.
  Viewmodel local frame: +x is screen-LEFT (right offsets negative).
- Stacked chain now: #94 (aim) <- #99 (shared library) <- #100 (punch/jump).
- Learning materials session 6 WRITTEN (local-only): lesson/drill 0024
  one-shared-asset-pool, vocab 0023. Next numbers: 0025 / vocab 0024.

# Checkpoint — SESSION 5 (aim / styles / preview)
Session 5 started: 2026-07-07
Goal: work ready-for-ai issues #91 (gun aim), #92 (more styles), #93 (preview rotation)
Status: COMPLETE — all three shipped in ONE draft PR #94, suite green.
Issues to process: [#91, #92, #93]  Completed: [#91, #92, #93]  Failed: []  Skipped: []

## Session 5 outcome (read first on resume)
- All three on feature/aim-styles-preview as atomic commits:
    c811e88 #91 aim | 3ea616f #92 styles | 31b8c17 #93 preview | 7be3057 review-fixes
- ONE draft PR #94 (base feature/player-journal, NOT main — main lacks
  CharacterParts.cpp). #91/#92/#93 labeled ai-completed + closed, linked to #94.
- Suite: 165 cases / 2343 assertions, 0 failed, 1 skipped. Game builds clean.
- Strict review of the diff applied: HIGH (drag-vs-click latched at press,
  not re-sampled per frame) + MEDIUM (held drag counts as active input) +
  LOW (#91 downward shots now culled once below ground). Deferred LOWs:
  text-field arrow focus (latent, arrows unused by fields) and head_oval
  non-uniform normals (cosmetic).
- #92 screenshot-verified in-engine (new parts/palettes render correctly).
  #91 in-game feel + #93 whole preview flagged as needs-human in PR #94.
- Learning materials LOCAL-ONLY: lesson 0023 + drill 0023 + vocab 0022
  "single authoritative source / the moonwalk bug class" (lookDirection #91,
  latch #93). NEVER committed. index.html/GLOSSARY.md/PROGRESS.md carry the
  local edits — do NOT git add them.
- shared-character-library: plan authored at .claude/plans/shared-character-library.md
  (audit finding: NPCs render via drawCharacter/rigged glTF, creator via
  drawCompositeCharacter/primitives — two pools, no shared assets; decision =
  make composite CharacterParts the single shared library, render NPCs from
  it). NOT yet turned into issues or implemented — next work item.
- Occupied learning numbers now: lessons/drills 0001-0023, vocab up to 0022.
  Next: lesson/drill 0024, vocab 0023.

## Locked deviations from the literal /overnight-session command (all defensible)
- BASE = feature/player-journal, NOT main. main lacks the target code
  (main:src/core/CharacterParts.cpp does not exist). Branching from main is
  impossible; issues explicitly say base = feature/player-journal.
- Implement all three on feature/aim-styles-preview (already stacked on
  player-journal, already carries the scaffold) as three atomic commits; open
  ONE draft PR referencing #91/#92/#93. Avoids risky splitting of intermixed
  uncommitted scaffold across three branches while a merge-check process has
  been moving HEAD this session.
- NEVER `git add .` / `git add -A`. .gitignore does NOT actually ignore the
  local-only paths (verified: docs/learning/, OVERNIGHT_REPORT.md, .claude/,
  docs/design/, CLAUDE.md, build_merge_check/ are all TRACKABLE). Stage ONLY
  explicit src/ + tests/ paths.
- Learning materials, OVERNIGHT_REPORT.md, .claude/memory/ stay LOCAL-ONLY:
  generated/updated but NEVER committed or pushed (standing user rule).
- No pushes to main, no merges. Draft PRs only.
- GitHub via PyGithub (no gh CLI): zsh -c 'source ~/.zshrc; export
  GH_TOKEN="$TOKEN"; python3 ...'. Never print the token. Retry WAF 400s.
- overnight-coordinator / git-historian agents are not in this registry →
  coordinate inline (I authored the plan + scaffold, so orientation is done).
- Env note: a concurrent merge-check moved HEAD earlier this session (checkout
  feature/issue-38 + 2 merge-main commits + build_merge_check/). It was
  quiescent at session-5 start (HEAD stable on feature/aim-styles-preview).
  Re-verify current branch before each commit.

---

# Checkpoint — SESSION 4 (four /idea projects)
Session 4: 2026-07-06 → 07-07
Goal: character creator, world time, gossip, journal (chained /idea projects)
Status: COMPLETE — all four shipped as stacked draft PRs, suite green.

## Session 4 outcome (read first on resume)
- Milestones #6–#9, tickets #72–#88. Stacked draft PRs (merge top-down):
    main ← #60 ← #68 ← #69 ← #70 ← #71 ← #77 ← #82 ← #89 ← #90
    #77 character creator | #82 world time | #89 gossip | #90 journal
- Suite: 160 cases / 1517 assertions, all green on feature/player-journal.
- OVERNIGHT_REPORT.md fully written for all four projects (repo root,
  uncommitted per rule).
- Learning materials 0020 (socket contracts), 0021 (world bus + injected
  time), 0022 (propose/validate/commit) + vocab 0021 — LOCAL-ONLY, never
  committed. index.html/GLOSSARY.md/PROGRESS.md carry those local edits;
  do NOT git add them.
- The "world bus" (WorldState) is now the shared seam: clock → gossip
  facts → journal reads. Any future shared fact rides the same store.
- July-6 /idea queue is EXHAUSTED — the trailing /scaffold "July 6th 2026
  ideas" is a no-op (all four plans already implemented + PR'd). The
  separate ~/.claude/plans/cached-mixing-fog.md (pluggable LLM + one-click)
  is ALSO already complete on branch 8-pluggable-llm-and-one-click. No
  pending feature work — confirm with the user before starting anything new.
- Env: GitHub via `zsh -c 'source ~/.zshrc; export GH_TOKEN="$TOKEN"; python3 …'`
  (valid token in TOKEN, not GH_TOKEN; never print it). No merges to main.

## (Session 3 city polish: #64–#67, PRs #68–#71 — see OVERNIGHT_REPORT.md)

---- SESSION 2 below ----

# Checkpoint — SESSION 2 (raylib overnight)
Session 2 started: 2026-07-05 ~01:15
Goal: triage stale weapons issues #7-13, then work raylib issues #35-42
Status: IN PROGRESS

## Session 2 environment notes (read first on resume)
- GitHub API: use `zsh -c 'source ~/.zshrc; export GH_TOKEN="$TOKEN"; python3 <script>'`
  (the valid token lives in TOKEN, not GH_TOKEN; PyGithub installed; no gh CLI).
- Base branch for #35 is feature/raylib-scaffold (stacked on the multiplayer
  PR stack tip -> learning branch). Each raylib issue stacks on the previous.
  DO NOT branch from main — multiplayer isn't merged yet.
- Issues #41 (two-instance visual pass) needs human eyes — expect needs-human.
  #42 deletion is gated on #39's side-by-side check — do in draft PR, note gate.
- Read .claude/skills/raylib-migration/SKILL.md before every issue.
- overnight-coordinator/git-historian agents don't exist in this session's
  registry — coordinate inline (same as session 1).
- GLOSSARY.md/PROGRESS.md still carry the user's unrelated uncommitted edits —
  never git add them.
- Occupied learning numbers: 0001-0013 (0002-0007 are local-only Silmulator
  files). Session 2 lessons start at 0014.
- GitHub WAF throws transient 400 "Whoa there" — retry with sleep.

## Session 2 progress
- (updates appended below as issues complete)

---- SESSION 1 (multiplayer) below ----
Session started: 2026-07-05 00:05
Goal: Work through the 7 planned multiplayer issues (plan: .claude/plans/multiplayer-and-aws-deploy.md)
Status: COMPLETE — see OVERNIGHT_REPORT.md (repo root, uncommitted)
GitHub: issues #21-#27, milestone #2, draft PR stack #28-#34 (merge in order)

## Environment notes (read this first on resume)
- GH_TOKEN / gh CLI NOT available — GitHub API ops (issues, labels, PRs) are
  DEFERRED to a morning-sync script. Issue numbers below are the plan's local
  numbering, not GitHub numbers.
- git push over HTTPS may work via keychain — verified only as dry-run so far.
- PyGithub installed (user approved). Issue-creation script:
  scratchpad/create_multiplayer_issues.py (extend into morning sync).
- Branch structure is STACKED (each issue branch off the previous) because the
  issues are sequential by nature: scaffold -> 1 -> 2 -> 3 -> 4 -> 5 -> 7.
  Issue 6 (loopback fixture) is folded into 3 and 4.
- Uncommitted files NOT part of this work (leave alone, do not commit):
  docs/learning/* Silmulator content, CLAUDE.md, .claude/, docs/*.md guides,
  GLOSSARY.md/PROGRESS.md preexisting modifications, docs/walkthroughs/.

## Issues to process
1. net protocol + framing        -> feature/issue-1-net-protocol-framing
2. NetServer snapshots           -> feature/issue-2-netserver-snapshots
3. NetClient connect/poll        -> feature/issue-3-netclient-connect-poll
4. server NPC chat routing       -> feature/issue-4-server-npc-chat
5. Menu host/join + main modes   -> feature/issue-5-menu-host-join
6. loopback fixture              -> folded into 3 and 4
7. docs                          -> feature/issue-7-docs-multiplayer

Issues completed: [scaffold, 1]
Issues failed: []
Issues skipped: []

## Progress log
- scaffold: COMPLETED — feature/multiplayer-scaffold pushed (22c6b03).
  git push WORKS via keychain; only GitHub API ops need the morning sync.
- Issue 1: COMPLETED — feature/issue-1-net-protocol-framing pushed (2b3ee35).
  FrameAssembler + NetMessage codec, 11 tests live. Concept:
  length-prefixed framing.
- Issue 2+3 (in progress, branch feature/issue-2-netserver-snapshots):
  NetSocket.hpp (portability shims; macOS needs SO_NOSIGPIPE not
  MSG_NOSIGNAL), NetServer rewritten publish/drain design (server threads
  touch ONLY sockets+queues; host loop feeds setHostPose/publishNpcPoses,
  drains drainChatEvents — World/LlmClient stay single-threaded), NetClient
  with timeout connect. 5 loopback tests live, 4 chat stubs remain skipped.
  NOTE: dropped scaffold's World&/LlmClient& ctor params — glue lives in
  main.cpp (issue 5) / test fixture instead.
- FOUND+FIXED pre-existing bug: LlmClient::~LlmClient lost-wakeup (stop_
  set + notify without requestMutex_ -> worker misses wakeup, join() hangs
  forever; suite hung on run 3 of a 5x flake check). Fix: set stop_ under
  the mutex. Regression test: 200x construct/destroy loop in
  test_llm_client.cpp. Commit this fix SEPARATELY (fix(llm): ...) on the
  issue-2 branch before the feat commit.

Issue #35: COMPLETED
  Branch: feature/issue-35-raylib-window  PR: #43
  Notes: raylib 5.5 pinned (6.0 anim API risk). --frames N smoke flag added.
  CRITICAL smoke recipe for the night: caffeinate -u -t 3 first (display
  asleep => rlglInit segfault, environmental). Suite 96 green.


Issue #36: COMPLETED
  Branch: feature/issue-36-raylib-input-modes  PR: see above
  Notes: InputMap int key codes keep header raylib-free; SetExitKey(KEY_NULL)
  so Escape stays a game key. Placeholder world/overlays pending #37/#40.


Issue #36: COMPLETED — PR #44.
MID-SESSION SYNC (user merged PRs):
- Stacked PRs #29-34 merged into their BASE BRANCHES, not main; only #28 hit
  main. Opened PR #45 (feature/issue-7-docs-multiplayer -> main, NON-draft)
  to consolidate — user must click merge.
- Old weapons PRs #14-20 merged into initial_npcs_and_world branch (also not
  main). Attempted union merge onto raylib line: ABORTED — weapons code has
  its own TODO stubs (partially scaffolded overnight PRs), pre-mood-era
  merge base. Created issue #46 (needs-human) with the full conflict map;
  commented on #7-13. User CLAUDE.md was shielded (backup in scratchpad).
- After #45 merges, retarget raylib stack bases to main (or leave; GitHub
  auto-retargets when base PRs merge).


Issue #37: COMPLETED — city scene from KayKit packs, screenshot-verified.
  Branch: feature/issue-37-raylib-city-scene
  Notes: --frames N shot.png gives VISUAL verification (Read the png!).
  Fountain uses base tile (flat) — polish in #42. 22 models preloaded.


Issue #38: COMPLETED — animated KayKit characters, screenshot-verified.
  Notes: unskinned-mesh crash fixed (compacted at load); smoke runs use
  fixed plaza camera. PR opened (see above).
INTERRUPT: user /idea+/scaffold arrived — NPC memory + model benchmark
  project with FULL AUTONOMY instructions (no questions; SQLite default;
  Qwen3 8B default; log decisions in OVERNIGHT_REPORT; draft PRs only).
  Their brief says gemma+format:json — REALITY: qwen2.5:3b + directive
  tags. Benchmark the REAL contract; log the discrepancy.
  Issues #39/#40 raylib deferred if context runs short.


PROJECT npc-memory-and-model (task 53) STATUS:
- Branch feature/npc-memory-and-model (off issue-38 tip), commit a256b33
  PUSHED: sqlite vendored, ConversationStore + 6 tests, memory injection
  (renderSystemPrompt(memory), Npc::setMemory), main.cpp hooks (load at
  start, summarize+save on dialogue close, transcript save on quit),
  persona_prompt CLI, bench_npc_models.py. Suite 103/732 green.
- REMAINING: (1) wait for ollama pulls (qwen3:8b, gemma3:4b,
  mistral:7b-instruct — bg task bjqset20t, log /tmp/ollama_pulls.log);
  (2) run: python3 tools/bench_npc_models.py (30-60min, run in bg);
  (3) write bench/REPORT.md from bench/out/results.json; set config/llm.cfg
  model = winner (default qwen3:8b if close per user instruction);
  (4) final commit + draft PR (base feature/issue-38-raylib-characters);
  (5) OVERNIGHT_REPORT.md update (log ALL autonomy decisions from the plan
  file section 'Autonomy decisions'); learning materials if context left.
- Raylib issues #39/#40/#41/#42 NOT started (user project preempted) —
  still labeled ready-for-ai; say so in report.
- DECISION LOG lives in .claude/plans/npc-memory-and-model.md.


Issue #39: COMPLETED — mood emote billboards, screenshot-verified. PR above.
  Decision: KayKit chars have ONE atlas material -> billboard fallback (in plan).


Issue #40 IN PROGRESS (branch feature/issue-40-raylib-ui-parity off #51's):
PLAN: (1) DialogUI: replace sf draw/event with per-frame pollInput()
  [GetCharPressed loop, KEY_BACKSPACE, KEY_ENTER returns submitted text;
  swallowNext = drain chars on open frame] + render() via DrawText, keep
  wrap()/transcript logic byte-identical. (2) Menu: port draw layer
  (DrawRectangleRec/DrawText, hover via GetMousePosition, clicks via
  IsMouseButtonPressed, text field via GetCharPressed) — keep pages incl.
  Multiplayer host/join + key capture via GetKeyPressed. MenuResult enum
  stays. Menu needs int keycodes (InputMap keyNameOf(int)). (3) main.cpp:
  restore FULL dialogue wiring from the SFML main
  (git show feature/raylib-scaffold:src/app/main.cpp lines ~280-450):
  pendingRoutes map, npc.ask on submit, drainDeltas->session/dialog,
  drainReplies->onReplyArrived + stage directions + CallPolice
  mobilization; jail/arrest system w/ wasCaught + countdown HUD;
  nameplates via renderer.worldToScreen + DrawText centered. DialogueSession
  reused as-is (core). (4) build + smoke screenshot + suite + commit + PR
  (base=feature/issue-39-raylib-mood-faces). Then #41 needs-human comment,
  #42 attempt if context allows, else morning report.
BENCH still waiting on model downloads (monitor bq2k1gt4q active).


Issue #40: COMPLETED — full UI/gameplay parity on raylib. PR above.
Issue #41: needs-human (it IS the human two-instance pass; checklist in PR).
Remaining: #42 cleanup (attempt next), bench (models still pulling),
learning materials, final report.


Issue #42: COMPLETED — legacy renderer deleted, docs updated. PR above.
RAYLIB MILESTONE: all 8 issues done (#41 = human pass). Remaining tonight:
bench (2 models still pulling), REPORT, cfg swap, memory PR, report, learning.


BENCH RUNNING (task b8m50km8h): qwen2.5:3b vs qwen3:8b, 25 probes + 20 turns.
  WATCH: 10-min Bash cap may kill it mid-run — if so, split per model:
  python3 tools/bench_npc_models.py --models qwen3:8b (separately).
  gemma3:4b + mistral:7b-instruct still downloading (monitor bq2k1gt4q);
  bench them when READY events fire, merge results into bench/REPORT.md.
  Then: cfg swap per verdict (qwen3 default if close), commit, draft PR
  (feature/npc-memory-and-model -> base feature/issue-42-raylib-cleanup?
  NO — base it on feature/issue-38-raylib-characters where it forked; note
  trivial main.cpp merge with #52/#53 for the reviewer).
Learning: lessons 0014/0015 written LOCALLY (uncommitted per user rule).


SESSION 2 COMPLETE:
- Raylib milestone: PRs #43,#44,#47,#48,#51,#52,#53 (all draft, stacked).
- Memory/model: PR #54 (qwen3:8b + think=false adopted; bench/REPORT.md).
- #41 needs-human (checklist in #52). #45 consolidation awaits merge.
- Lessons 0014/0015 local-only per user rule. OVERNIGHT_REPORT.md final.


NEW /idea (size-contract-and-weapons) — plan at .claude/plans/size-contract-and-weapons.md
Branch: feature/size-contract-and-weapons off integration/npc-memory-into-raylib.
Order: (1) SizeSpec Fill/Uniform in Assets+RaylibRenderer, screenshot; 
(2) combat core port from origin/initial_npcs_and_world per issue #46 map
(union Npc: keep mood-era + graft combat; take core files wholesale; AUDIT
TODO stubs and IMPLEMENT or CUT — no TODOs committed); tests green;
(3) combat app layer on raylib in main.cpp (attack input LMB?, HUD hp/ammo,
death screen mode, callouts above heads via worldToScreen);
(4) OVERNIGHT_REPORT: premise notes (menu/dialogue already shipped #52;
speaking-options = flagged design question; weapons omissions list).
PR: draft, base integration/npc-memory-into-raylib.
Integration PR #57 to main is OPEN (user merges).


FINAL: MR pair delivered — #57 (memory→main, ready) and #58 (sizes+weapons,
draft, based on #57). 124 tests green. Session complete.

NEW /idea motion-collision-viewmodel — plan at .claude/plans/motion-collision-viewmodel.md
Branch feature/motion-collision-viewmodel off feature/size-contract-and-weapons (PR #58's).
(1) Npc::deriveFacingFromMotion(prevPos, dt) — atan2(dx,dz) when
displacement/dt > ~0.5 u/s; called in main's npcLastPos loop AFTER combat
tick; Follow/Arrest/ReturnHome keep their movement but their manual
faceToward-while-moving becomes redundant (leave lookAt for standing).
(2) City::makeDowntown += 4 trafficlight_ne/nw/se/sw (1x1 AABB h4.5 at
(+-32+9,+-32+9)) + 4 bush_a..d entries (2.4x2.4 h1.1 at park spots);
Assets curated_ maps those ids -> trafficlight_A / bush stems + Uniform
specs (4.5 / 1.1); RaylibRenderer deletes hardcoded light+bush loops.
(3) drawViewmodel(const Player&) in RaylibRenderer: table per WeaponKind
{primitive shape+color, rest offset, attack offset}; drawn AFTER EndMode3D?
NO — 3D: draw in 3D near camera before endFrame (small cubes/cylinder at
camera-relative transform) using attackAnimFraction; simplest robust:
2D overlay weapon indicator + 3D thrust… decide: 3D camera-anchored.
Then tests (facing + city completeness), build, screenshot, suite, commit,
push, draft PR base=feature/size-contract-and-weapons, issues via script,
report update.


PR #60 (motion/collision/viewmodel) opened, issues #61-63 filed. Chain:
#57 (ready) -> #58 (draft) -> #60 (draft). Session complete.

---- SESSION 3 (city polish) ----
Session 3 started: 2026-07-05 (overnight)
Goal: work ready-for-ai queue #64-#67 (milestone #5 City polish)
Status: IN PROGRESS

## Session 3 environment notes
- Same GitHub recipe: zsh -c 'source ~/.zshrc; export GH_TOKEN="$TOKEN"; python3 ...'
- BASE DECISION (logged): tonight's branches stack on
  feature/motion-collision-viewmodel (PR #60, retargeted to main, contains
  main). Reason: #64/#65 touch RaylibRenderer code #60 modified; #66 extends
  the obstacle-completeness test that exists only on #60. Chain:
  #60 -> issue-64 -> issue-65 -> issue-66 -> issue-67, each PR based on the
  previous branch (established stacked pattern).
- Smoke recipe: caffeinate -u -t 3 first; ./build/cpp_game_with_llm_npcs
  --frames 90 shot.png; Read the png.
- Suite baseline on #60 branch: 127 cases / 782 assertions.
- Occupied learning numbers: 0001-0015. Session 3 lessons start at 0016.
- Never git add GLOSSARY.md/PROGRESS.md (user's unrelated edits).

Issues to process: [64, 65, 66, 67]
Issues completed: []
Issues failed: []
Issues skipped: []

Issue #64: COMPLETED
  Branch: feature/issue-64-plaza-fountain  PR: #68 (draft, base = #60 branch)
  Concept taught: composite modeling from primitives (asset-pack gaps)
  Learning files: 0016 lesson/drill/vocab — LOCAL-ONLY per user rule
  Notes: --camera x z yaw smoke flag added (own commit) — reuse for #65-#67.
  SizeSpec fountain 1.2->2.6 (pancake-era value); DrawCylinder is a capped
  solid, water discs float above caps. Suite 127/824.

Issue #65: COMPLETED
  Branch: feature/issue-65-road-tiles  PR: #69 (draft, base = issue-64)
  Concept taught: tiling modular kit pieces over a parametric grid
  Learning files: 0017 lesson/drill/vocab — LOCAL-ONLY per user rule
  Notes: tile centers at multiples of 16 (junctions on centers); thickness
  pinned 0.05 (Fill would curb 0.8); cube strips kept as no-assets
  fallback. MYSTERY SOLVED: beige slab in every smoke shot = #60 fist
  viewmodel (fixed screen pos), not a world artifact. Suite 127/824.

Issue #66: COMPLETED
  Branch: feature/issue-66-street-dressing  PR: #70 (draft, base = issue-65)
  Concept taught: set dressing as authored data
  Learning files: 0018 lesson/drill/vocab — LOCAL-ONLY per user rule
  Notes: cars Z-long at identity, Uniform path doesn't rotate -> all spots
  on x=+-32 streets; police car parks BESIDE station block (decision
  logged). Screenshot caught mid-lane oversized boxes -> curb-hugging 4u
  AABBs. New clearance test (personas + crossings). Suite 128/886.

Issue #67: COMPLETED
  Branch: feature/issue-67-atmosphere-pass  PR: #71 (draft, base = issue-66)
  Concept taught: distance fog in a forward renderer
  Learning files: 0019 lesson/drill/vocab — LOCAL-ONLY per user rule
  Notes: one shader serves models (material assignment) AND rlgl batch
  (BeginShaderMode) — matModel identity for batch. exp2 fog d=0.006 toward
  sky color; warm tint pre-fog. UnloadModel leaves material shaders alone
  (raylib shared contract) -> Assets unloads once. Skinning intact
  (CPU skinning). Suite 128/886.

SESSION 3 STATUS: all 4 issues COMPLETED (chain #60 -> #68 -> #69 -> #70
-> #71, all draft). Morning report next.

SESSION 3 COMPLETE: #64-#67 all shipped. Chain: #60 -> #68 -> #69 -> #70
-> #71 (all draft, merge top-down). Suite 128/886. OVERNIGHT_REPORT.md
rewritten (session 3 on top, session 2 preserved below). Learning 0016-0019
local-only; index.html updated locally.

---- SESSION 4 (character creator /idea) ----
Date: 2026-07-06. All five tickets #72-#76 COMPLETED in one stacked
branch feature/character-creator (base = feature/issue-67-atmosphere-pass).
PR: #77 (draft). Milestone #6. Suite 139/1384.
Plan + decision log: .claude/plans/character-creator.md
Key facts for future sessions:
- CharacterParts.hpp: PartDef sockets in local space, CategorySpec wiring,
  styleCompatible gate, randomizeLook(seed) xorshift32 deterministic.
- Assembly: sockets resolve UNSCALED, then one uniform scale to 1.8
  (character height contract). drawCompositeCharacter uses rlPushMatrix.
- CharacterStore: two independent tables (character_persona/character_look)
  in saves/characters.sqlite3; renderPersonaText = inverse of
  parsePersonaText (PersonaLoader).
- Menu Page::Creator; creatorPreview() drawn by main in-world; creator
  page dims at 90 alpha (others 170).
- Creator UI needs HUMAN eyeball pass (noted in PR #77).
- Learning 0020 local-only. Merge chain: #60->#68->#69->#70->#71->#77.

## Session 9 progress (running)
- #137 COMPLETED fa63be3 (pinned mirror fetch — Google Drive unpinnable, hukasu mirror + sha256; LoadModel smoke green)
- #138 COMPLETED 165141b (cel = derivative-normal 3-band + fog one program; outline shader = normal-inflate inverted hull; routing rule enforced)
- #139 COMPLETED 71002ab (strict quaternius family, default pool switch, measured bounds, proportion window 0.14-0.28; KNOWN: pack clips lose shoulder rotation in raylib — #142 evidence in scratchpad pose probes)
- #140 COMPLETED da9fffa (8x4x6 decal set, 3-strip face wrap, billboard retired for family)
