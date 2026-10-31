# Skill: /handoff [focus_description]
When triggered, generate a temporary markdown handoff document to transfer context to a fresh session. Do not duplicate whole source files; reference existing paths or URLs.
The document must strictly contain:
1. **Core Goal**: What we are trying to accomplish.
2. **Current State & Decisions**: What worked, what failed, and why choices were made.
3. **Existing Artifacts**: Paths to active files, scripts, or spec sheets.
4. **Immediate Next Steps**: Bulleted actions for the incoming agent.
If [focus_description] is provided, narrow the summary and next steps exclusively to that sub-task. Output the exact command to spin up the next session using this file.

