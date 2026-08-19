# Legacy Branch Audit — Unported Features & Branch Dispositions
Date: 2026-07-14 (from the full branch-vs-dev audit)
Status: REFERENCE — each gap needs its own /idea to become work

## Why these can't be merged
The branches below predate the raylib rewrite (SFML era). A git merge
would resurrect the deleted engine and conflict on everything — the same
reason the weapons system was hand-PORTED in feature/size-contract-and-
weapons (issue #46's conflict map) rather than merged. Features here need
re-building on the modern game. Per the user's standing rule: when old
and new disagree, the LATER (current) state wins.

## Unported features (from 7-weapons-money-and-mayhem, 18 commits, June 2026)
Ranked by how much game they'd add today:

1. **Economy** (`d2db326`) — money, a bank, buying weapons from shops.
   Nothing like it exists in dev. Would compose with: shop spots (City),
   dialogue (buy via conversation?), the trait system (salesman!).
2. **Enterable buildings** (`9edfbd3`, `1b4cc30`, `fab91d6`) — hollow
   buildings with doorway collision, shop interiors, swinging doors,
   shopkeepers INDOORS (plus a grocer and guitar shop). Directly answers
   the session-7 playtest finding "town feels like figures parked in a
   lot". Big collision-model change (rooms, not solid AABBs).
3. **Traffic** (`5426733`, `0c43644`) — cars that drive the avenue grid,
   yield to pedestrians, can knock them down. Answers "streets feel
   oversized and empty" (finding #7).
4. **Pedestrian crowds** (`8b55889`) — wandering procedural pedestrians
   beyond the 10 personas. Answers findings #4/#8 (empty plaza).
5. **Graduated law** (`60a1cfc`, `832d682`, `99698cf`) — offense charges
   and scaled sentences; arrest walks you to a CELL in the station
   (current game teleports + fixed 10s); police stand-down fix (already
   ported in spirit via calmDown).
6. **Bigger town** (`9a43e80`) — 5x5 downtown grid. Cheapest to redo
   fresh in City::makeDowntown data (or as a sandbox map!).
7. Superseded, do NOT port: articulated characters / rounded NPCs / blob
   shadows / gradient sky / textures (`3e3df16`, `10ece81`, `86e3dcb`,
   `100e5ce`, `79bc3f0`) — the Mii composite + outline + day/night
   systems replaced all of it.

## Branch dispositions
| Branch | Verdict |
|---|---|
| `8-pluggable-llm-and-one-click` (local) | **PORTED** → PR #135. Delete local branch after merge. |
| `7-weapons-money-and-mayhem` | KEEP as the archive of the unported features above. |
| `5-detail-the-town`, `6-bigger-livelier-city` (local) | Strict ancestors of 7-weapons — delete-safe while it stays. |
| `initial_npcs_and_world`, `feature/issue-7..13` | Old weapons line; content ported long ago (issue #46). Archive-or-delete at will. |
| ~30 branches at "1 ahead" | Only the user's PR merge commits — zero content. Delete-safe. |
| Everything at "0 ahead" | Fully contained in dev. Delete-safe. |

Deletion one-liner for the provably-empty set (run when ready):
`git push origin --delete <branch>` per branch — or keep them; they cost
nothing but list noise.
