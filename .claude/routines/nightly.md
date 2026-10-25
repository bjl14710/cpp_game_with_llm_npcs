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
