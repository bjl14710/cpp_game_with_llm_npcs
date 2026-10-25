---
name: design-scout
disable-model-invocation: true
description: Generate ~15 design ideas across six angles, filter against DIRECTION.md, stress-test the top 4 plus a wildcard with a skeptic, and file them as `design` proposals with paste-ready /idea blocks.
argument-hint: Optional direction, e.g. "player experience", "performance", "portfolio-worthy", "sellable"
---

Load the `routine-guardrails` skill first and follow it throughout.

Your job is to propose where this project could go next. No code edits, branches, or PRs.

Direction: "$ARGUMENTS"

## 0. Understand what exists
- Read the context layer, open issues, the latest `research: ... radar` issue if one exists, and `git log` for the last 30 days.
- Write a 5-line summary: what the project does today, what DIRECTION.md says it's building toward, and the 2–3 most recent directions of effort.

## 1. Diverge (~15 one-line ideas, at least two per angle)
- **Finish:** what is 80% built and would become useful with a small push?
- **Deepen:** make an existing feature substantially better (quality, not more features).
- **Connect:** combine two subsystems so they do something neither does alone, including reuse across Brandon's other projects (NPC game, trading platform, HFT repo, Silmulator, finance tracker).
- **Measure:** an eval, benchmark, or dashboard that makes future decisions easier.
- **Subtract:** something to delete, merge, or simplify.
- **Wildcard:** one ambitious idea that would make this repo stand out in a portfolio or interview.
Pull in relevant findings from the latest radar issue and say which idea came from where.

## 2. Converge
Score each on value (as user, learner, job-seeker), effort (S/M/L/XL), risk, and what it would teach. Hard filters: DIRECTION.md non-goals and constraints, rejected issues, the minimal-build-ladder (could it reuse something in the repo?), and licenses for anything new. Keep the top 4 plus the wildcard.

## 3. Stress-test
Spawn the `skeptic` subagent with the 5 finalists and DIRECTION.md. For each it returns the strongest argument against, what breaks or gets harder, and the cheapest one-day experiment that would prove or kill it. Drop any finalist the skeptic sinks; say why.

## 4. File
Each surviving finalist becomes an issue labeled `design`, `proposed`, `size:*`, `needs-human`. Body per guardrails §7, plus:
- The skeptic's case against, and the de-risking experiment
- A paste-ready `/idea` block: idea and goal in 2–3 sentences, out-of-scope items, acceptance-criteria sketch, and decisions to auto-select with defaults (Brandon's /idea runs don't interrogate him)

## 5. Summary
Per guardrails §8, plus rejected ideas one line each with the reason, and anything you need from Brandon to choose between finalists.

---

## Project Lens
- **NPC game:** emergent NPC behavior, gossip and memory payoffs, economy loops, cutscenes. Constraints: stays C++ (no rewrites or engine moves; ideas must be C++ implementations), host-authoritative LAN sync, schema-as-contract, BYOK only, local-first.
- **Trading platform:** new GBM features, Arena competitors, risk and portfolio construction, monitoring the paper runner. Constraints: DSR and purged-CV gates, dry-run only.
- **HFT repo:** realism (replay, queue position, fees), latency, market-making simulation. Constraint: every claim measurable.
- **Silmulator:** new instruments, fault injection, a HIL bridge, test-authoring UX. Constraint: SCPI fidelity.
- **Finance tracker:** insights, budgets, forecasting, alerts. Constraints: privacy, data stays local.
