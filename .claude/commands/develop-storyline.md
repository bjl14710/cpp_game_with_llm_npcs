---
description: Collaboratively develops the LLM-NPC game storyline — factions, character arcs, main quest, side quests, and world lore. Saves structured story documents that NPCs and overnight sessions can reference. Run interactively, not overnight.
argument-hint: Optional focus e.g. "main quest" or "factions" or "character arcs" (blank = full world)
---

This is a CREATIVE COLLABORATION command, not an engineering planner.
Run it interactively with you present — it asks questions, proposes ideas,
and refines based on your feedback. Do NOT run this overnight.

Read the existing codebase first to understand the world as it already exists:
```bash
grep -rn "town\|village\|world\|faction\|npc\|character" \
  src/ data/ docs/ 2>/dev/null | grep -v ".o:" | head -30
find data/ -name "*.json" 2>/dev/null
cat docs/STORY.md 2>/dev/null || echo "No story doc yet"
```

---

## PHASE 1 — WORLD FOUNDATION

If no story exists yet, start by establishing the three pillars:

**The Setting:** Ask:
- What kind of world is this? (medieval fantasy, post-apocalyptic, sci-fi,
  modern, alternate history, something else?)
- What's the rough scale? (one town, a region, a continent?)
- What's the dominant tone? (dark and gritty, lighthearted, morally grey,
  hopeful, mysterious?)

**The Central Conflict:** Ask:
- What's wrong in this world that the player can affect?
- Who or what is the antagonist force? (a person, a faction, a system,
  a natural force, an idea?)
- What does a "good ending" look like for the world?

**The Player's Role:** Ask:
- Who is the player character in this world?
- Why are they here, in this town, at this moment?
- What do they want at the start (before the story begins)?

---

## PHASE 2 — FACTION DESIGN

Generate 3-4 factions with competing interests. Good factions:
- Have a clear goal that makes sense for their members
- Conflict with at least one other faction over something specific
- Offer something the player wants (information, weapons, shelter, money)
- Have a reason to interact with NPCs in the town

For each faction, produce:

```markdown
## [Faction Name]

**Goal:** [what they're trying to achieve]
**Methods:** [how they pursue it — moral, ruthless, secretive?]
**Headquarters:** [where in the world they operate from]
**Public face:** [what ordinary townspeople think of them]
**True nature:** [what the player discovers if they dig deeper]

**Key NPCs:**
- [Name] — [role in faction] — [personality in one sentence]
- [Name] — [role in faction] — [personality in one sentence]

**What they offer the player:**
- [resource or information they control]
- [what a player must do to earn it]

**Conflicts with:**
- [Faction X] over [specific thing] — [who's right, if anyone]
```

---

## PHASE 3 — CHARACTER ARCS

For each named NPC already in the game (read from data/ or src/), or for
the planned NPCs, develop a character arc — a change the NPC goes through
over the course of the game based on player choices.

```markdown
## [NPC Name] — [Role e.g. Blacksmith, Guard Captain, Merchant]

**Starting state:** [what they believe, what they want, what they fear]
**Secret:** [something true about them the player can discover]
**Arc:** [how they change based on player interaction]

If player builds HIGH trust (>70):
  → [what the NPC reveals, offers, or does]
  → [how their dialogue changes]

If player builds LOW trust or is hostile:
  → [how they respond, what they withhold]
  → [do they become an obstacle?]

If player ignores them entirely:
  → [do they have their own plot that resolves without the player?]

**Connection to main plot:** [how this character ties into the central conflict]
```

---

## PHASE 4 — QUEST STRUCTURE

Design the quest line as a branching structure, not a linear path.

**Main Quest — [Title]:**
```markdown
ACT 1 — Arrival
  Trigger: [what starts it]
  Key decision: [first choice that matters]
  Branch A: [if player chooses X] → leads to [consequence]
  Branch B: [if player chooses Y] → leads to [different consequence]

ACT 2 — Escalation
  The conflict deepens. What was unclear becomes clear.
  Key revelation: [something the player learns that reframes what they knew]
  Faction decision: [which faction does the player commit to, if any?]

ACT 3 — Resolution
  Ending A (high trust, helped the town): [description]
  Ending B (sided with antagonist): [description]
  Ending C (neutral, survived but changed nothing): [description]
  Ending D (chaos — player burned everything down): [description]
```

**Side Quests** — generate 3-5, each:
- Started by a specific NPC at a trust threshold
- Completable without touching the main quest
- Gives the player something useful AND reveals world lore
- Has a consequence that persists (the world remembers)

---

## PHASE 5 — SAVE THE OUTPUT

Save everything to `docs/STORY.md`. Structure:

```markdown
# [Game Title] — Story Bible

## World Overview
[Setting, tone, central conflict — 1 paragraph]

## Factions
[All faction documents from Phase 2]

## Character Arcs
[All NPC arc documents from Phase 3]

## Quest Structure
[Main quest + side quests from Phase 4]

## Lore Notes
[Details about the world that don't fit elsewhere — history, mythology,
 geography, technology level, magic rules if any]

## NPC Dialogue Seeds
[For each named NPC: 3-5 seed phrases that capture their voice,
 used when writing system prompts]
```

Also save faction and NPC data in structured JSON for game reference:
`data/story/factions.json`
`data/story/npc_arcs.json`

---

## PHASE 6 — CREATE IMPLEMENTATION TICKETS

After the story is approved by you, offer to create GitHub issues for
implementing the story elements:

```
/plan-github "story implementation — NPC system prompts and dialogue"
```

This creates issues for:
- Writing each NPC's system prompt from their arc document
- Adding faction knowledge to relevant NPCs
- Adding quest trigger logic to the game state
- Adding dialogue variations for trust thresholds

---

## HOW THIS COMMAND WORKS

Unlike engineering commands, this is a conversation:
1. Ask the Phase 1 questions and wait for your answers
2. Propose ideas based on your answers — don't just list options
3. Pick the most interesting direction and develop it fully
4. Ask for feedback and adjust
5. Move to the next phase only when you're happy with the current one

Push back on generic choices. "A dark medieval town with an evil king"
is boring. Ask what makes THIS town, THIS conflict, THIS set of NPCs
interesting and specific. The best game stories are specific.

The story document is the creative foundation. The implementation tickets
are engineering work. Keep them separate.
