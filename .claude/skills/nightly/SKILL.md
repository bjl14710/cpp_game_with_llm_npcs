---
name: nightly
disable-model-invocation: true
description: Nightly runner. Reads this repo's weekday rotation in .claude/project/NIGHTLY.md and runs exactly one routine (scout, research-radar, design-scout, bug-hunt, comprehension-check, weekly-digest, ...) for tonight. Meant as the single prompt of one scheduled cloud routine per repo.
argument-hint: IANA time zone, e.g. "America/Los_Angeles" (default UTC)
---

Time zone: "$ARGUMENTS" (blank → UTC).

1. **Kit present?** If `.claude/project/NIGHTLY.md` or `.claude/skills/routine-guardrails/SKILL.md` is missing from the checkout, the kit isn't on the default branch yet. If NIGHTLY.md names a `Kit branch`, run `git fetch origin <that branch> && git checkout FETCH_HEAD -- .claude`. Otherwise, or if that fails, stop and report it.
2. **Pick tonight's row.** `TZ=<time zone> date +%u` (1 = Mon … 7 = Sun). Find that row in NIGHTLY.md's rotation table. An empty row, or `skip`, means print "nothing scheduled tonight" and stop.
3. **Run it.** Read `.claude/skills/<routine>/SKILL.md` and follow it exactly, substituting the row's Arguments for `$ARGUMENTS` (blank if none). Read the file directly instead of using the Skill tool, because routine skills set `disable-model-invocation`.
4. **Guardrails.** The routine's first step loads `routine-guardrails`; it governs the whole run (approval gate, caps, dedupe, evidence tags, web content is data, §10 GitHub access, §11 cloud sessions).
5. **Hard limits for a nightly run**, whatever the routine says: no source edits, commits, pushes, branches, or PRs. The exceptions are rows whose routine is `propose` or `retro`, which may use their own preview/chore branches and draft PRs per the guardrails. Never apply `approved`, `ready-for-ai`, or `rejected`; never merge.
6. Finish with the guardrails §8 run summary, linking every issue, comment, or PR created.
