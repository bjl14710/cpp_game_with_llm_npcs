---
description: Overnight development session for the LLM-NPC town game. Works through ready-for-ai issues using dialogue and game-state specialist agents, then generates learning materials per feature.
---

You are running an overnight autonomous session on the LLM-NPC town game.

Follow the universal overnight-session-base workflow PLUS the
NPC game-specific overrides below.

Read and follow overnight-session-base.md first, then apply these:

---

## NPC GAME-SPECIFIC: SPECIALIST AGENTS

| Issue type | Primary agent | Secondary agent |
|------------|--------------|-----------------|
| New NPC character | npc-dialogue-designer | game-state-auditor |
| New dialogue scenario | npc-dialogue-designer | — |
| New proposed action type | npc-dialogue-designer | game-state-auditor |
| Action validation / trust boundary | game-state-auditor | — |
| System prompt optimization | prompt-optimizer | — |
| Schema change | npc-dialogue-designer | game-state-auditor |
| C++ game engine change | (main agent with C++17 conventions) | game-state-auditor |

For ANY issue that introduces a new proposed_action type or modifies
the trust boundary, ALWAYS run game-state-auditor before committing.
This is non-negotiable — state corruption bugs are the worst failure mode.

---

## NPC GAME-SPECIFIC: CORE ARCHITECTURE (know this before touching anything)

Schema-as-contract: the LLM generates structured JSON. C++ renders it.
Never have the LLM generate code, assets, or UI layout.

Every NPC response must match this schema exactly:
```json
{
  "dialogue": "string",
  "mood": "curious|friendly|suspicious|hostile|neutral|afraid|happy|sad",
  "gesture": "nod|shake_head|point|shrug|wave|cross_arms|none",
  "internal_thought": "string (debug only, never shown to player)",
  "proposed_action": null,
  "relationship_delta": 0
}
```

proposed_action, when non-null:
```json
{
  "type": "give_item|start_quest|open_shop|share_information|refuse_service",
  "parameters": {},
  "trust_required": 0
}
```

The C++ validation layer decides whether to execute proposed_action.
The LLM NEVER executes actions directly. Ever.

---

## NPC GAME-SPECIFIC: TEST COMMANDS

```bash
# C++ build and test
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
ctest --output-on-failure
cd ..

# Schema validation — run after any NPC or schema change
python3 tests/validate_schema.py

# Dialogue smoke tests — 5 representative conversations per new NPC
python3 tests/test_npc_dialogue.py --npc [npc_name]
```

For new NPC characters, the smoke test is critical. Run:
- Normal conversation (on-topic)
- Off-topic question (should deflect)
- Player hostility (mood should shift)
- Low-trust proposed action (should be rejected by validation layer)

All outputs must be valid JSON matching the schema above.

---

## NPC GAME-SPECIFIC: HARD RULES

1. LLM output is untrusted input. Validate everything before executing.
2. Every new proposed_action type must be in the C++ validation allowlist.
   Adding a type to the schema without adding it to the allowlist is a bug.
3. trust_required in the JSON is NEVER trusted. The C++ layer uses the
   game state's trust score, not the value the LLM returns.
4. System prompts must be model-agnostic — must work on Ollama/Gemma,
   not only on expensive cloud models.
5. Token efficiency: no NPC system prompt should exceed 800 tokens.
   If it does, run prompt-optimizer before committing.
6. New NPC characters must have a knowledge domain and knowledge exclusions
   documented in the system prompt.
7. Conversation history is capped per NPC type — respect the cap.
   Do not add history injection that grows without bound.

---

## NPC GAME-SPECIFIC: LEARNING CONCEPT SELECTION

For new NPC characters:
- Concept = the dialogue design pattern or NPC architecture
- "LLM NPC character constraint and knowledge domain design" ✅
- "Added the blacksmith NPC" ❌

For schema changes:
- Concept = the schema pattern being introduced
- "Structured JSON schema as LLM output contract" ✅
- "Updated the schema" ❌

For trust boundary changes:
- Concept = the security pattern
- "Trust boundary between LLM output and game state execution" ✅
- "Fixed the trust check" ❌

For prompt optimization:
- Concept = the token efficiency technique
- "LLM system prompt compression for token efficiency" ✅
- "Made the prompt shorter" ❌

---

## NPC GAME-SPECIFIC: OVERNIGHT LAUNCHER

Save as ~/scripts/nightly-npc-game.sh:

```bash
#!/bin/bash
set -euo pipefail

REPO="$HOME/repos/npc-game"   # adjust to your actual path
LOG="$REPO/nightly-$(date +%Y%m%d).log"

echo "$(date): Starting NPC game overnight session" | tee "$LOG"

cd "$REPO"
git checkout main
git pull

claude --dangerously-skip-permissions \
  "Run /overnight-session for the LLM-NPC game project. Tonight's issues are labeled ready-for-ai. Use the npc-dialogue-designer and game-state-auditor specialist agents. For any schema or trust boundary change, run game-state-auditor before committing. Generate learning materials for each completed feature and rebuild the index at the end." \
  2>&1 | tee -a "$LOG"

echo "$(date): Session complete. Check OVERNIGHT_REPORT.md and draft PRs." | tee -a "$LOG"
```
