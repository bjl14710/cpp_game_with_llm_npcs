# Trait Fine-Tune Escalation Path (documented, NOT built)

The trait system (milestone #14) conditions NPC personality through the
prompt: rules + curated few-shot examples per trait, re-asserted after the
memory summary. This document records what the system accumulates and the
criteria under which a LoRA fine-tune would be justified — so that decision
is made from evidence later, not vibes. **No training work exists or is
planned in this milestone.**

## What accumulates, and where

| Data | Location | Shape |
|------|----------|-------|
| Curated trait definitions | `traits/*.trait` (committed) | rules + hand-picked example exchanges |
| Liked replies (candidates) | `saves/ratings/candidates.jsonl` | `{ts, persona, traits[], player, npc}` |
| Disliked replies | `saves/ratings/rejected.jsonl` | same shape |
| Per-NPC memories | `saves/conversations.sqlite3` | first-person summaries |

The rating loop is human-curated BY DESIGN: promotion means manually
copying a candidate's exchange into a `.trait` file. Nothing auto-feeds
prompts (pinned by test in `tests/test_traits.cpp`).

## How this becomes LoRA training data (when justified)

Each candidates row is already one supervised pair:
- **input**: the persona's rendered system prompt (reconstructable from the
  persona file + trait library at that commit) + the player line
- **target**: the liked reply
Rejected rows are hard negatives for preference tuning (DPO-style pairs
when a liked and disliked reply share the same player line and persona).

A conversion script would join candidates/rejected by (persona, traits)
into JSONL chat-format rows per trait; a per-trait LoRA (rank 8-16 on the
current local model) is the smallest sensible unit. Serving would pick the
adapter by the character's dominant trait.

## The escalation criteria (all three, measured, before training anything)

1. **Conditioning demonstrably fails**: trait-adherence spot-checks (does a
   grumpy NPC complain first?) fail in >~25% of rated replies DESPITE the
   post-memory reinforcement — i.e., the prompt lever is already maxed.
2. **Data is sufficient**: ≥200 curated candidate rows for the trait in
   question (fewer overfits a LoRA into parroting).
3. **The failure matters**: the misses are player-visible personality
   breaks, not phrasing taste (rejected.jsonl gives the evidence base).

If (1) is false, keep curating examples — it is cheaper, reversible, and
model-agnostic. Model swaps (the pluggable-backend branch) reset the case
for training entirely; a LoRA binds to one base model, prompts do not.
