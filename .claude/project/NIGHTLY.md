# Nightly rotation: LLM NPC City

Per-project schedule for the shared `/nightly` skill. This repo's one scheduled cloud routine
runs `/nightly America/Los_Angeles` every night; this table decides what it does. Edit the table, not the routine.

Kit branch: claude/practical-meitner-13ay9k

| Weekday (`date +%u`) | Routine | Arguments |
|---|---|---|
| 1 Mon | research-radar | `lanes: ecosystem, pitfalls` |
| 2 Tue | design-scout | |
| 3 Wed | bug-hunt | |
| 4 Thu | research-radar | `lanes: techniques, prior art, challenges` |
| 5 Fri | comprehension-check | `7 days` |
| 6 Sat | scout | |
| 7 Sun | weekly-digest | `7 days` |

Sunday's digest runs here because this repo is currently the only one listed in `repos.txt`.
If another repo becomes the digest's home repo, set this row to `skip`.

Run by hand when you want them: `/triage` (interactive), `/propose`, `/retro`, `/direction`.

## The routine (claude.ai/code/routines)

- **Repository:** bjl14710/cpp_game_with_llm_npcs only
- **Connectors:** GitHub only (deselect Google Drive)
- **Schedule:** daily, 1:37am America/Los_Angeles
- **Prompt:** `/nightly America/Los_Angeles`

Until the kit is merged to the default branch, `/nightly` isn't loaded as a slash command, so use this prompt instead:

```
Read .claude/skills/nightly/SKILL.md and follow it with time zone America/Los_Angeles. If that file is missing, first run `git fetch origin claude/practical-meitner-13ay9k && git checkout FETCH_HEAD -- .claude`.
```

Create it in the routines UI, not from a chat session: a task created from inside a session gets no repo checkout and no GitHub tools (confirmed by a test on 2026-10-25).
