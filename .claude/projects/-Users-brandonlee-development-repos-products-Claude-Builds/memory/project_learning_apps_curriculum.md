---
name: project-learning-apps-curriculum
description: Real curriculum authoring is deferred for the 11 leetcode-style learning apps — each is currently smoke-test scaffolding only
metadata: 
  node_type: memory
  type: project
  originSessionId: 3905548e-bb82-43d7-ab2c-f91380ce7e48
---

The 11 forks of `leetcode_type_learning_template` (pytorch, fintech, cybersecurity, docker, kubernetes, do178c, pep_practices, quant, ml_algorithms, sql, tls) under `Claude_Builds/leetcode_type_learning_apps/` ship with **only** 1 lesson + 1 quiz + 1 problem in `content/basic/` — enough to pass `make test`, not enough to learn from.

**Why:** Brandon explicitly deferred real curriculum to keep the initial scaffolding pass fast. The intent is to author per-app curriculum later, one topic at a time, after the 11 forks + builds are in place.

**How to apply:**
- When Brandon returns to any `<topic>_learning_app/`, expect "let's flesh out the curriculum" as the likely next ask — basic/intermediate/advanced content under `content/`.
- Authoring follows the manifest's `levels.*.mix` ratio (lesson / quiz / code). Per-topic mix may need tuning (e.g., SQL is code-heavy, DO-178C is lesson-heavy).
- Don't auto-generate filler — Brandon wants real, topic-accurate content. Ask which topic to start with and what depth (basic only vs. all three levels).
- Reference plan: `/Users/brandonlee/.claude/plans/please-plan-out-how-agile-tower.md` ("Out of Scope" section explicitly defers curriculum).

Related: [[project-vhdlearner]] (the reference learning app, single-subject) shows what a finished curriculum looks like — phases + lessons + quizzes + code challenges hard-coded in `curriculum.ts`. The template's file-based YAML/markdown is the modular successor.
