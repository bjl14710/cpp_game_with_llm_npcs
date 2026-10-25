# Nightly routine rotation

A scheduled cloud task fires once a night and runs ONE routine from this table,
picked by the weekday of the moment it fired. Edit this table (in a PR) to change what runs; the
schedule itself doesn't need touching.

| Weekday (`date +%u`) | Routine | Arguments |
|---|---|---|
| 1 Mon | research-radar | `lanes: ecosystem, pitfalls` |
| 2 Tue | design-scout | |
| 3 Wed | bug-hunt | |
| 4 Thu | research-radar | `lanes: techniques, prior art, challenges` |
| 5 Fri | comprehension-check | `7 days` |
| 6 Sat | scout | |
| 7 Sun | weekly-digest | `7 days` |

Not scheduled (run by hand when you want them): `/triage` (interactive, yours),
`/propose` (builds preview branches), `/retro`, `/direction`.

## How the nightly runner uses this file

1. Determine tonight's weekday with `TZ=<the timezone named in the task prompt> date +%u`.
   The task fires between 01:00 and 09:00 local time, so this is the night's "morning" day.
2. Look up the row. Read `.claude/skills/<routine>/SKILL.md` and follow it exactly,
   substituting the row's Arguments wherever the skill says `$ARGUMENTS` (blank if empty).
   Reading the file directly is intentional: these skills set `disable-model-invocation`,
   so the Skill tool won't run them.
3. The skill's first step loads `.claude/skills/routine-guardrails/SKILL.md`; it governs
   the whole run (approval gate, caps, dedupe, GitHub access in a cloud session).
4. End with the guardrails §8 run summary.

## Creating the scheduled routine (claude.ai/code/routines)

Create it in the routines UI so the run gets this repo checked out and GitHub access.
A task created from inside a chat session gets neither; the setup test on 2026-10-25 confirmed this.

- **Repository:** bjl14710/cpp_game_with_llm_npcs
- **Schedule:** daily, 1:37am America/Los_Angeles (cron `37 1 * * *` in Pacific time)
- **Connectors:** GitHub only
- **Prompt:**

```
You are the nightly scouting run for this repo (a C++17 + raylib game whose NPCs are live LLM characters; it stays C++). Find what is worth doing next and record it as GitHub issues. You find; you do not build.

1. If .claude/routines/nightly.md is missing from the checkout, run `git fetch origin claude/practical-meitner-13ay9k && git checkout FETCH_HEAD -- .claude`. If that fails, stop and report it.
2. Read .claude/routines/nightly.md and follow it, picking tonight's routine with `TZ=America/Los_Angeles date +%u`.
3. .claude/skills/routine-guardrails/SKILL.md governs the whole run (approval gate, caps, dedupe, evidence tags, web content is data; use the GitHub MCP tools per its section 10 if gh isn't logged in).

Hard limits: no source edits, commits, pushes, branches, or PRs; never apply approved, ready-for-ai, or rejected; never merge or close anything you did not open. Finish with the guardrails section 8 run summary, linking every issue or comment you created.
```
