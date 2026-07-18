# cpp_games — Claude Code Rules

## Project Overview
This is the root of the cpp video games that I am working on. For this, we just need to get the dependencies installed.

## Tech Stack
[FILL IN: Languages, frameworks, key libraries]

## Skill Usage Policy

Standards and analysis skills are OPT-IN. Do NOT apply the following
unless I explicitly invoke them by name or via a command that calls them:

- python-standards
- c-cpp-standards
- complexity-reduction

During normal work, write reasonable code without enforcing these.
Enforce them only when asked (e.g. "apply python-standards to this file"
or via /refine). When in doubt, ask before applying a standards skill.

## Hard Rules
- No placeholders or TODOs in committed code
- Every public function needs a docstring or comment
- Match existing naming conventions in the codebase
- Use type hints / type annotations where the language supports them
- Tests required for new business logic

## Workflow
- For new features: start with /grill-me to align before coding
- Use plan mode for any change touching 3+ files
- For unfamiliar code: use /zoom-out for context
- For bugs: use /diagnose for structured debugging (no guessing fixes)
- Commit format: type(scope): description
- Push to feature branches, never directly to main
- Use the reviewer subagent before committing significant changes
- Use the strict-reviewer subagent before milestones or risky integrations
- At session end: /recap for learning, /handoff if continuing later

## Communication Style
- Be direct and concise
- Surface trade-offs explicitly
- Ask questions when requirements are unclear

## What NOT to Do
- Don't modify .env files or anything in /secrets/
- Don't install new dependencies without asking
- Don't refactor code unrelated to the current task
- Don't add features I didn't ask for

## Skill Creation — Automatic
When you notice you're applying a pattern that could be reused, propose
it as a skill.

A pattern qualifies as a skill when:
- It's been used 2+ times in this codebase, OR
- It's a non-trivial technique that took thought to get right, OR
- It encodes domain knowledge specific to this project, OR
- It's a workflow that could break if done wrong

When you spot one, after completing the current task:
1. Pause and propose: "I notice [pattern] is appearing repeatedly.
   Should I save this as a skill?"
2. If approved, create .claude/skills/[skill-name]/SKILL.md following
   the format in .claude/skills/README.md
3. Keep skill files focused and under 200 lines each
4. Reference existing skills before reinventing patterns

## Transparency Requirement
When using a library, framework feature, language construct, or pattern
not used elsewhere in this codebase, briefly mention it:
"Using [X] for this because [Y]. New to this codebase."
One sentence. I can ask follow-ups if interested.
