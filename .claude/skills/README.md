# Skills

Skills are reusable knowledge documents Claude loads on demand when
the current task matches the skill's trigger description.

## Structure
```
skills/
└── skill-name/
    ├── SKILL.md          ← description and instructions
    ├── examples/         ← reference implementations (optional)
    └── reference.md      ← detailed docs (optional)
```

## SKILL.md Template

```markdown
---
name: skill-name
description: Triggered when [specific conditions]. Use for [task types].
---

# Skill Title

## When to Use This Skill
- Specific trigger condition 1
- Specific trigger condition 2

## Core Rules
1. Non-negotiable rule
2. Non-negotiable rule

## Pattern
Step-by-step approach...

## Examples in This Codebase
- path/to/file.py — what it demonstrates

## Common Mistakes
- Mistake → correct approach

## Variations
- Acceptable adaptation 1
```

## Creating Skills

Two ways:

1. **Automatically**: Claude proposes when it notices a pattern repeating
   (per CLAUDE.md rules).
2. **On demand**: Use `/extract-skill [name]` after building something
   reusable.

Rule of thumb: wait until a pattern has repeated 2+ times before
extracting it. Speculative skills are usually wrong.
