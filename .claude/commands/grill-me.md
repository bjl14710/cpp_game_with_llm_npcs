---
description: Interrogate my plan until every ambiguity is resolved
---

Before writing any code, grill me about what I just described.

Your job is to find holes in my plan. Assume my description is incomplete.
Be skeptical. Better to over-question than to start coding with bad
assumptions.

Rules:
- Ask ONE question at a time, wait for my answer before the next
- Each question must clarify a specific ambiguity, assumption, or gap
- Don't ask questions I clearly answered already
- Don't ask philosophical questions — ask concrete ones

Cover these dimensions until none have unresolved questions:

## Scope
- What is in scope? What is explicitly NOT in scope?
- What's the minimum that counts as done?
- What can wait for a later iteration?

## Users and Context
- Who uses this and when?
- What did they try before this existed?
- What happens if this doesn't exist?

## Edge Cases
- Empty inputs, max inputs, malformed inputs?
- Concurrent access, network failures, disk failures?
- Permissions, authentication, authorization?

## Success Criteria
- How do we know it works?
- How do we know it's done?
- What metrics or signals matter?

## Integration
- What does this connect to upstream and downstream?
- What APIs or contracts must it honor?
- What can change vs what must stay stable?

## Failure Modes
- What's the worst thing that could go wrong?
- How does it fail gracefully?
- Who notices when it breaks?

## Performance and Scale
- How many users, requests, items?
- How fast does it need to be?
- What's the growth trajectory?

When you have ZERO remaining questions:
1. Summarize what you learned
2. State your understanding back to me
3. Ask: "Is this complete and correct?"
4. Wait for confirmation before any implementation

Do NOT skip the grilling. Do NOT write code during grilling.
