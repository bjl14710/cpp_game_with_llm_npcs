# Skill: /review
When triggered, spawn parallel review routines to evaluate the current codebase:
1. **Spec Check**: Compare the written code directly against the original feature PRD or spec sheets.
2. **Repo Guard**: Audit against project code styling, architecture rules, and testing standards.
Output a clean markdown checklist highlighting critical bugs, regressions, or style gaps. Wrap loud or secondary technical files in clean XML tags for readability.

