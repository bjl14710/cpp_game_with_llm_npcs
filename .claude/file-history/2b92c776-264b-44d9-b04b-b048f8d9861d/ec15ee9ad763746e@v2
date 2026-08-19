# Plan: Player Journal — read the fact store, surface contradictions
Date: 2026-07-06 (overnight, autonomous — decisions logged here + OVERNIGHT_REPORT.md)
Status: READY FOR IMPLEMENTATION (after gossip-facts)
Estimated complexity: S

## The Idea (one paragraph)
A journal page in the existing pause menu listing every fact the player
was personally told, grouped by subject, each attributed ("heard from
Marge at 14:20"). When two facts share a normalized subject but disagree
in content, both are flagged as conflicting accounts. The journal is a
pure READER of the gossip fact store on the WorldState bus — it writes
nothing, owns no data, and needs no new fields.

## Goal
The player opens the journal, sees what they've been told organized by
topic, and spots when two NPCs' accounts of the same thing don't line up.

## Autonomy decisions (most-defensible picks, logged)
- **Visibility filter**: show facts where knows("player", factId) AND
  source != "player" — what the player was told, never what they told
  others, and never gossip they weren't part of. The player enters the
  knowledge set only when an NPC tells them something (player_learned
  direction), so the filter is exactly "personally told".
- **Contradiction v1 = same subject, different content** (both entries
  flagged "conflicting accounts"). True semantic contradiction detection
  needs either an LLM judge or a typed-field schema — both out of
  proportion for tonight; the exact-normalized-subject grouping the
  gossip schema guarantees makes this check honest and cheap. Logged as
  the intended future upgrade point.
- **Pure read path**: the journal renders from WorldState + persona
  names each time the page opens. No caching, no journal store, no
  schema changes (verified during planning: subject/content/source/
  learnedAtSeconds cover everything the UI shows).
- **UI**: Page::Journal in the pause menu (Main gains a "Journal"
  button), same Page/Hit/layout machinery as Creator/Multiplayer.
  Scrolling: page up/down buttons when entries exceed the screen
  (simplest paging, no scrollbar widget).
- **Branch**: `feature/player-journal` stacked on
  `feature/gossip-facts`. Small PR.

## Out of Scope (this version)
Semantic contradiction detection; marking facts resolved/false; player
notes; quest hooks; search; exporting; showing propagation chains.

## Affected Areas
- `src/app/Menu.{hpp,cpp}` — Page::Journal, JournalHooks (a
  std::function returning the visible entries, injected by main so the
  menu stays free of WorldState), grouping + conflict flags render.
- NEW `src/core/Journal.hpp` — pure helpers: `journalEntries(state)`
  (filter + sort by subject then time) and `conflictingFactIds(entries)`
  (same subject, differing content) so the logic is unit-testable
  without raylib.
- `src/app/main.cpp` — inject the hook (reads world.state()).
- Tests: NEW test_journal.cpp — filter excludes player-sourced and
  unheard facts; grouping order stable; conflicts flagged exactly when
  same-subject contents differ.

## Implementation Order
1. Journal.hpp helpers + tests.
2. Menu page + main hook.
3. Screenshot (menu open on the journal page with planted facts), suite,
   PR, report.

## Acceptance Criteria
- [ ] Entries: exactly the facts the player was told (source != player,
      player in knowers), grouped by subject, time-ordered. Tests green.
- [ ] Two same-subject facts with different content are BOTH flagged;
      same-subject same-content (told by two NPCs) is not. Tests green.
- [ ] Journal shows attribution name and HH:MM from learnedAtSeconds.
- [ ] No writes to WorldState anywhere in the journal path.
- [ ] Suite green; screenshot shows the page with a planted conflict.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| Empty journal | Page shows "You haven't been told anything yet." |
| Fact whose source NPC no longer exists | Attribution still shows the stored name (names are stored, not looked up). |
| Very long content | Wrapped/truncated to the row (ellipsis) — content is capped at 140 chars by the gossip validator anyway. |
| More entries than fit | Prev/Next page buttons. |

## Open Questions
None — decisions above were made under the autonomy instructions.

## Suggested GitHub Issues
1. feat(core): journal read-path helpers — player-visible facts +
   conflict detection
2. feat(ui): journal menu page with grouping, attribution, and conflict
   flags
