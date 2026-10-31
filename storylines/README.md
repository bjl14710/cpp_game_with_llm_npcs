# Storylines

Authored mystery templates: the content `generateMystery` deliberately leaves
empty. `src/core/Mystery.cpp` picks who died, who did it, where and when, and
stops — its comment says quantity of evidence and witnesses "belongs to the
storyline templates and their validator". These are that.

Loader: `src/core/Storyline.cpp`. Validator: `validateStoryline` (issue #187).
Casting onto a live roster: issue #188.

**No plan document backs this format.** Idea 7 never had an `/idea` run, so the
shape here is derived from what four finished plans already require of it —
`win-cutscene.md` (ordered chains), `opening-murder.md` (templates own
quantity), `alibis-and-testimony.md` (a lie is an ordinary fact),
`role-layer.md` (the killer is chosen elsewhere). If one of those is wrong, this
format is the thing to change.

## Format

One file per mystery, `<id>.storyline`, same `key = value` shape as
`personas/*.persona`, `traits/*.trait` and `banks/*.bank`. Blank lines and `#`
comments are ignored. Leading whitespace on section-scoped keys is cosmetic.

`#` only opens a comment as the **first non-space character** — captions are
prose, so `Two cups on the counter, table #3` survives intact.

```
# storylines/the_late_delivery.storyline
id = the_late_delivery
title = The Late Delivery
min_residents = 6

role = neighbour
  kind = witness
  note = lives above the bakery; sees the alley

role = rival
  kind = red_herring
  note = argued with the victim that afternoon

clue = 1
  zone = bakery_block
  caption = The bakery's back door was unlocked all evening.
  points_at_killer = true

clue = 2
  zone = coffee_block
  caption = Two cups on the counter, one untouched.
  slot = rival
  points_at_killer = false

witness = neighbour
  zone = bakery_block
  hour = 21.5
  about = culprit
  observed = someone leaving by the alley door
```

| Key | Scope | Meaning |
|---|---|---|
| `id` | file | Stable key. Required — a file without one is skipped. |
| `title` | file | Player-facing name. |
| `min_residents` | file | Smallest roster this template can be cast onto. |
| `role` | starts a role | Author's slot id. Which resident fills it is decided at cast time. |
| `kind` | role | `killer`, `witness`, `red_herring` or `bystander`. |
| `note` | role | Authoring aid. Never shown to a player. |
| `clue` | starts a clue | The **authored chain order**, 1-based. See below. |
| `zone` | clue, witness | A `Zones.hpp` id, e.g. `bakery_block`. |
| `caption` | clue | ≤ 140 chars, so it fits a `KnownFact`. |
| `slot` | clue | The role this clue bears on. Optional. |
| `points_at_killer` | clue | `true` for a real clue, `false` for a red herring. |
| `witness` | starts a witness | The role slot doing the seeing. |
| `observed` | witness | What they claim to have seen. |
| `hour` | witness | World hour, 0–24. |
| `about` | witness | **Who the testimony is about**: another slot id, or the literal `victim`. Optional, but see below. |

## Why `about` is the key that makes a mystery a mystery

`Journal.hpp` flags a contradiction as *one subject, more than one content*, so
two accounts can only ever clash if they land on the same subject. `about` is
what decides that subject.

| `about` | Fact subject | Catches |
|---|---|---|
| set | `<that person> whereabouts` | two residents disagreeing about one person |
| omitted | `<speaker> testimony` | one resident changing their own story |

Both are real and both are tested. But **a template whose witnesses never set
`about` cannot produce a disagreement between two people**, because two speakers
are two subjects — and `validateStoryline` rejects it for exactly that reason.

This is not a style preference; it is issue #214. Before it, facts keyed on the
speaker unconditionally, the validator demanded a contradiction defined as *same
zone, within half an hour, different observation*, and that shape could never
produce a subject collision. The validator required something the engine could
not deliver, and the tests pinned the requirement. The rule now is the engine's
own rule: **same `about`, different `observed`.**

### An alibi is testimony about yourself

There is no `alibi` key and there does not need to be. A slot naming itself in
`about` produces a fact on its own whereabouts subject, so the speaker's account
and anyone's sighting of them collide automatically:

```
witness = culprit
  zone = coffee_block
  hour = 21.5
  about = culprit
  observed = nursing a coffee alone

witness = neighbour
  zone = bakery_block
  hour = 21.5
  about = culprit
  observed = the culprit at the bakery's back door
```

Both land on `<killer> whereabouts`, the journal flags both sides, and nothing
anywhere records which one is the lie. The player decides. That is the game.

## Why `clue` carries an explicit order

`order` is **authored chain order, not discovery order**. The win cutscene
replays the clues that mattered as an argument, in the order that argument
runs — not as a replay of the players' wandering. `win-cutscene.md` names this
as the requirement that is cheap to honour now and expensive to retrofit:

> a template producing an unordered bag of clues cannot drive a montage that
> reads as reasoning

The parser keeps what you authored and never re-sorts.

## Why roles are slots, not names

A template has to work on any roster, and the roster is 21 residents who are
otherwise ordinary. So a template names the *part* — `neighbour`, `rival` — and
casting assigns a resident at match time.

**The killer is the exception.** `generateMystery` already chose it before a
template is cast, and `role-layer.md` scopes that decision to there. A template
declaring `kind = killer` describes the part the already-chosen killer plays; it
does not pick who that is. Reassigning it would silently break `voteIsCorrect`.

## Why there is no `is_lie` flag

`alibis-and-testimony.md` decided a lie is an ordinary fact: same subject,
different content, and `Journal.hpp`'s existing contradiction check flags both
sides. A template creates a lie by authoring two witnesses whose observations
disagree — not by marking one false. Nothing in the game ever knows which is
which, which is the point.

## Editing by hand

Fine. Two rules: `id` must match the filename stem, and run the validator before
committing so a hand-written template faces the same checks a generated one
does.
