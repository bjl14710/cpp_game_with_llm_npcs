# Detective mode — plan index and build order

Fourteen plans covering the multiplayer detective/mystery mode, written 2026-08-06
to 08. This index exists because the set is large and the dependencies are not
obvious from the filenames.

**These were lost once.** They were written as untracked files and a `git clean`
during a rebase destroyed them — the same event that deleted a code scaffold and
broke the cmake build. They are committed now, like the 27 plans that survived
that event because they were tracked.

## Status at a glance

| Plan | Milestone | Status |
|---|---|---|
| `npc-line-bank.md` | #18 | **Shipped** — PRs #158–#162; #153 open (`needs-human`) |
| `detective-day-phases.md` | #19 | **Partly shipped** — #154 (PR #167); #155–#157 open |
| `clue-like-zones.md` | #20 | **Partly shipped** — #163, #164 (PRs #168, #169); #165, #166 open |
| `twenty-one-residents.md` | #21 | **Partly shipped** — #172 (PR #174); #170, #171, #173, #175 open |
| `opening-murder.md` | — | Planned, not issued |
| `alibis-and-testimony.md` | — | Planned, not issued |
| `vote-and-retaliation.md` | — | Planned, not issued |
| `role-layer.md` | — | Planned, not issued |
| `cutscene-system.md` | — | Planned, not issued |
| `opening-murder-cutscene.md` | — | Planned, not issued |
| `wrong-accusation-cutscene.md` | — | Planned, not issued |
| `win-cutscene.md` | — | Planned, not issued |
| `networked-match.md` | — | Planned, not issued. Build LAST |
| `retire-worldgen-sandbox-mode.md` | — | Planned, not issued. Any time |

Storyline templates (the mystery generator's authored content) is the one idea
never planned. Three plans push requirements onto it, so it is the most
constrained document still to write.

## Build order

**The spine — makes a playable single-player mystery:**

1. `alibis-and-testimony` — the fact system already does most of it, so this is
   the cheapest real progress
2. `opening-murder` — seeds what the alibi layer serves
3. `detective-day-phases` — the day/phase machine (started)
4. `vote-and-retaliation` — reads the ground truth back

**Then, to make it repeatable and confident:** storyline templates → `role-layer`.

**Then, to make it feel like Clue:** `clue-like-zones` (started) →
`twenty-one-residents` (started).

**Then multiplayer:** `networked-match`.

**Presentation, after the spine works:** `cutscene-system` →
`opening-murder-cutscene` / `wrong-accusation-cutscene` / `win-cutscene`.

**Cleanup, any time:** `retire-worldgen-sandbox-mode`.

## The one thing to decide early even though it is built late

`win-cutscene`'s flashback needs the fact chain queryable as an ordered "clues
that mattered" list. That requirement is cheap to honour while building
`alibis-and-testimony` and the storyline templates, and expensive to retrofit. An
unordered bag of clues cannot drive a montage that reads as reasoning.

## Findings worth not rediscovering

Each is measured or read from the code, not assumed. They are the reason these
documents are worth keeping.

- **The world clock has no day counter.** `advanceTime` is a pure `fmod` over 24
  hours; "day 2" was not representable. (`detective-day-phases`)
- **`Journal.hpp` already detects contradictions** — same subject, different
  content, flagged on both sides. A lie needs zero new fields; it needs a liar.
  (`alibis-and-testimony`)
- **Buildings are solid AABBs.** No interiors anywhere; enterable buildings live
  only in unported SFML branches. Zones do not need them. (`clue-like-zones`)
- **21 NPCs cost 35.3 ms/frame vs 26.4 at ten** — 38 fps down to 28, ~0.81 ms per
  NPC, and the game is already under 60 at ten. (`twenty-one-residents`)
- **Only 12 valid silhouettes exist, not 24.** Style families (`round`/`blocky`)
  cannot mix, so 21 distinct residents is unreachable without new parts.
  (`twenty-one-residents`)
- **qwen3:8b holds a lie easily but leaks guilt through the mood tag.** An
  innocent Marge laughs when accused; a guilty one goes angry on turn one, and
  mood drives the rendered face. Measured. (`role-layer`)
- **`NpcMoodUpdate` is broadcast to every client**, so that leak is worse in
  multiplayer. (`networked-match`)
- **`HostChatRouter` already routes reply text per-player**, so prompts and role
  blocks never leave the host. (`networked-match`)
- **The renderer takes a `CameraPose` per frame**, so a cutscene needs no renderer
  change at all. (`cutscene-system`)
- **Worldgen manages 50% schema validity** and is the only other consumer of the
  single `LlmClient` worker. (`retire-worldgen-sandbox-mode`)
- **`ConversationStore` keys by persona name, not file stem.** (`npc-line-bank`)
- **The bitmap font's glyph spacing is integer**, so sizes 14–18 are identical.
  Wanted-a-font-atlas count: three features. (`clue-like-zones`,
  `cutscene-system`)
