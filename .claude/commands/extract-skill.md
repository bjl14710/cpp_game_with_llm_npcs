---
description: Extract a reusable skill from recent work
---

For the pattern "$1" that we just implemented:

1. Identify the abstract pattern beneath the specific implementation
2. Determine when this pattern should activate in the future
3. Create .claude/skills/$1/SKILL.md with:
   - Frontmatter: name and description (description must include trigger
     words that match future contexts where this should activate)
   - "When to use this skill" — explicit activation criteria
   - "Core rules" — non-negotiable constraints
   - "Pattern" — step-by-step approach
   - "Examples" — references to existing code that uses this pattern
   - "Common mistakes" — what to avoid
   - "Variations" — known acceptable adaptations

4. If supporting examples are worth saving, create
   .claude/skills/$1/examples/ with extracted reference files

5. Verify the description's trigger words will activate the skill.
   Test by describing 2-3 future tasks that should trigger it.

Keep the skill focused — one pattern per skill, under 200 lines.
