# Plan: NPC Line Bank + Nightly Refinement Loop
Date: 2026-08-06 (rewritten 2026-08-08 after the original was lost)
Status: SHIPPED (phase 1) — PRs #158–#162; #153 open. Phase 2 NOT READY.
Estimated complexity: L

## The Idea

Every NPC reply costs a round trip to `qwen3:8b` — **2.7 s to first token, 4.5 s
total** (`bench/REPORT.md`). Most of what players say is the same forty-odd
things. This banks high-quality authored replies for those topics so they serve
instantly and locally, plus a nightly job that reads the day's transcripts,
notices uncovered topics, has Claude author variants, scores every candidate,
and opens a **draft PR whose diff is human-reviewable text**.

The bank is plain text in the repo, same `key = value` shape as
`personas/*.persona` and `traits/*.trait`, so a night's work reviews like a
normal code change. As a side effect it accumulates a scored, in-character
corpus — exactly the training set phase 2 needs and does not otherwise have.

## Goal

A player pressing `T` and typing a common line gets an in-character reply in
**under 50 ms instead of 2.7 s**, and the maintainer gets a nightly draft PR
whose diff is the lines that changed, with a scorecard proving they got no worse.

## Decisions taken

| Question | Answer |
|---|---|
| Cache or finetune? | **Cache now; finetune is phase 2, specced but NOT READY.** The cache's corpus becomes the training set, so the ordering is the cheapest path to having training data at all. |
| Who authors the lines? | **Claude, grounded in real play.** The nightly job clusters real transcripts, then Claude writes variants against that persona's *actual* rendered system prompt. |
| When may a banked line serve? | **Any confident topic match**, with guardrails: 3–5 variants per (persona, topic, familiarity), LRU rotation, no repeat within a session. The threshold is a config dial. |

## Why lexical matching, not embeddings

The paraphrase objection ("got anything to eat?" vs "what do you sell?") is
solved at **authoring** time: Claude emits 8–12 `trigger =` phrasings per topic.
Matching is then normalize + token-set/trigram Jaccard, which buys most of what
an embedding gives and in exchange is:

- deterministic and unit-testable **with no network** — critical for a job that
  runs unattended at 3 a.m. and for doctest;
- free of a second resident model competing with the 8B for 16 GB;
- unable to silently degrade when Ollama is down.

`TopicMatcher` is shaped so an embedding scorer can replace `similarity`'s body.
The trigger is a **measured** hit rate below 40%, not a hunch.

## Why the interception point is `LlmClient::submit`

`Npc::ask` already returns a `uint64_t` and `onReplyArrived` already strips
`[[MOOD:]]`/`[[ACTION:]]` tags, appends history and drives the face. A synthetic
reply on the **same queues under a real id** means:

- `DialogueSession`'s state machine is untouched;
- tags on banked lines run through the same `parseDirectives`;
- history, memory and gossip update identically;
- **zero call sites change**, and `submitGroupTurn`/`submitWorldgen` pass no
  speaker id so they bypass the bank *by construction*.

Deltas are paced at `line_bank_cps` (default 220) so a banked reply *streams*.
Delivering 200 characters in one frame reads as a different system.

## The five-gate rubric

| # | Gate | Passes when |
|---|---|---|
| 1 | tag_compliance | parses via the **real** `parseDirectives`, one known mood tag, nothing left over |
| 2 | stream_budget | streams faster than the live model generates |
| 3 | fidelity | LLM judge ≥ 4/5 against the persona's rendered prompt |
| 4 | non_regression | blind, order-shuffled A/B vs live `qwen3:8b`; wins or ties ≥ 70% |
| 5 | leakage | no fourth-wall break, spoken tags, prior-meeting reference in a `familiarity = first` line, or knowledge-boundary violation |

**Gate 4 is load-bearing** — it answers "does the cache make the game worse to
make it faster?" with a measurement instead of a promise.

**Gate 2 was reframed during implementation.** The original "p95 served < 50 ms"
conflated the lookup (microseconds, unobservable from Python) with the
deliberate streaming at `line_bank_cps`. It now asks the question that matters:
would caching this line make the game *slower* than not caching it? Real hit
rate and served latency come from the C++ `BankStats` in #153. The gate rarely
fires at realistic lengths — a guardrail, not an active filter.

Gate 1 shells out to `persona_prompt --parse` rather than re-implementing
`parseDirectives` in Python. `tools/bench_npc_models.py` duplicates the mood
keyword list and its own comment admits it must be hand-synced; this avoids
repeating that.

## Out of Scope

- Any weight training. Phase 2 below.
- New C++ or Python dependencies. Matcher is stdlib C++17; tooling is stdlib
  Python plus PyGithub.
- Embedding models.
- Group conversations and world generation — never cached, by construction.
- Changing `RatingLog`'s contract. Its header says thumbs-up lines never feed
  live prompts automatically, and a test pins it. The nightly job reads the log
  as *input signal*; the human still approves via the merge.
- Multiplayer sync of the bank. Banked replies go through `HostChatRouter` like
  live ones.

## Bank format

```
persona = Marge Holloway

topic = greeting_first
  familiarity = first
  trigger = hello
  trigger = hi there
  reply = Well, good morning! Sourdough's just out. [[MOOD: happy]]
  reply = Morning, love. What can I get you? [[MOOD: happy]]
```

`familiarity` (`any` / `first` / `returning`) is the one guard against a banked
line contradicting persisted memory: a returning player must never get a
first-meeting greeting. Everything else is authored memory-agnostic, enforced by
gate 5.

A `#` only opens a comment as the **first non-space character** — `Config.cpp`'s
`readKv` strips from any `#`, which is right for settings and wrong for prose
("Room #3 is upstairs." must survive).

## Implementation Order — all shipped

1. `TopicMatcher` + 9 tests (#148, PR #158)
2. `LineBank` + 10 tests (#149, PR #159)
3. `LlmClient` integration + 4 tests (#150, PR #160)
4. `tools/eval_lines.py` + `persona_prompt --parse` (#151, PR #161)
5. `tools/refine_lines.py` (#152, PR #162)
6. **Author the ten banks and flip `line_bank = on`** (#153, `needs-human`)

## What running it revealed

- **`ConversationStore` keys by `Persona::name`** ("Marge Holloway"), not file
  stem ("baker"). The first nightly job looked up the stem, found nothing for
  the entire cast, and reported success.
- **The corpus is far too thin to bootstrap from.** The real save holds 20
  player lines across 8 characters with nothing recurring. Waiting for the
  nightly job to fill banks will not work — #153's hand-seeded ~40 topics is the
  actual path.
- The "nothing to do" message initially said "every topic already covered" when
  the truth was "nothing recurred, and there are no banks". Those are different
  states and conflating them hid an empty corpus behind a reassuring message.

## Phase 2 — LoRA finetune (SPECCED, NOT READY)

Once the bank holds a few thousand scored exchanges, that corpus can LoRA a
*smaller* base model so it natively produces what `qwen3:8b` currently needs 8B
parameters for. The win is that **uncached** replies get faster too.

Sketch on the M1 Pro / 16 GB: export to JSONL → `mlx-lm` LoRA on
Qwen2.5-3B-Instruct (4-bit, rank 8–16) → fuse → GGUF via `llama.cpp` →
`ollama create` → re-run the full rubric plus `bench_npc_models.py`, promote only
on a win. **Both tools are new dependencies and need approval.**

**A GGUF is 2–5 GB and cannot go in a git PR.** The daily PR carries the
training JSONL, the adapter (~10–30 MB), the Modelfile and the scorecard; weights
go to a Release pinned by sha256 and fetched by `tools/fetch_assets.sh`, which
already implements exactly that pattern.

Open risk: nightly retraining on a corpus you also generated is a feedback loop
that can narrow a model's range. A frozen holdout and a diversity rubric
dimension would need to exist first.

## Open Questions

1. The ~40 seed topics for #153.
2. Nightly schedule — fold into the existing nightly run, or its own cron?
3. Cloud spend ceiling. Default assumed 100k tokens/night, enforced as a hard
   abort.
4. A tracked duplication: `refine_lines.py` reimplements the matcher's
   similarity in Python for clustering. Fix is a `topic_match` CLI beside
   `persona_prompt`; carries a TODO so it cannot drift unnoticed.
