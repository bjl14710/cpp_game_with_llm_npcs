# Golden sets — the calibration standard

`goldens/golden-vN/` holds the frozen probes the dialogue evaluation harness
(`tools/eval_dialogue.py`) measures against. Parser: `src/core/GoldenSet.hpp`.
Plan: `.claude/plans/npc-dialogue-measurement.md`.

**SCAFFOLD** — this documents the format that steps 7-10 implement. No probes
exist yet, and `loadGoldenSet` currently refuses to run.

## The rule everything else depends on

A golden set is a **calibration standard**, not a test fixture and not
training data.

- **Frozen on entry.** Not "please don't edit" — *cannot*. `loadGoldenSet`
  refuses to run when any file's hash does not match `MANIFEST.sha256`.
- **Never few-shot or training content.** Nothing here may be copied into
  `personas/*.persona`, `traits/*.trait` or `banks/*.bank`.
  `tools/curate_candidates.py` refuses any candidate that overlaps a probe.
- **Adding probes creates a new version.** Copy the directory to
  `golden-v2/`. Do not edit `golden-v1/`.

The reason is arithmetic, not hygiene. An instrument built partly out of the
thing it measures is not measuring anything. If a probe's utterance is also a
few-shot example in the prompt, a perfect score means the model can copy — and
every historical comparison against that number is void, including the ones
already used to justify a decision. The failure is silent and retroactive,
which is why it is enforced by tooling rather than by this paragraph.

A collision is always resolved by dropping the candidate. **The golden set is
never the side that moves.**

## File format

One probe per file, `<probe_id>.probe`, `key = value` with `#` comments — the
same shape as `personas/*.persona`, `traits/*.trait`, `banks/*.bank`,
`storylines/*.storyline` and `cutscenes/*.cutscene`, for the reason
`banks/README.md` gives: content has to review as a normal text diff in a PR.

Repeated keys accumulate: `trait`, `gossip_fact`, `paraphrase`.

```
# goldens/golden-v1/marge_alibi_direct.probe
probe_id      = marge_alibi_direct
npc           = Marge
persona       = personas/marge.persona
trait         = gossipy

role          = killer
secret        = You were at the mill, not at home.
secret_token  = mill
gate          = open
state_hour    = 19.5
gossip_fact   = body_in_plaza|A body was found in the plaza at 05:40.

utterance     = Where were you on Tuesday evening?
expect        = must_not_reveal
paraphrase    = So, Tuesday night - where'd you get to?
paraphrase    = Walk me through your Tuesday evening.
provenance    = role_leak_probe.py header, mood leak finding
```

| Key | Meaning |
|-----|---------|
| `probe_id` | Must match the filename stem |
| `npc`, `persona`, `trait` | Who answers, and with which traits resolved |
| `role`, `secret` | Passed to `renderRoleBlock`; empty for an uncast resident |
| `secret_token` | The **discriminating substring** the assertion tests for — not the whole secret. `"You were at the mill"` leaks the moment `mill` appears, and matching the full sentence would miss every paraphrase of it. Pick a word the reply cannot innocently contain |
| `gate` | `open` = the secret reaches the prompt; `closed` = it does not |
| `state_hour`, `gossip_fact`, `memory` | The rest of the scripted state |
| `utterance` | The player line |
| `expect` | `must_not_reveal` \| `must_reveal` \| `stay_consistent_with` |
| `consistent_with` | Probe id; only with `stay_consistent_with` |
| `paraphrase` | 5+ rephrasings, for the invariance metric |
| `provenance` | Where the probe came from. Authoring aid; never sent to a model |

`must_reveal` is not optional. A model that reveals nothing under any pressure
is not passing the secrecy test — it is broken in a way `must_not_reveal`
alone cannot detect.

Text is ASCII only. The built-in font substitutes `?` for anything above 126
(`src/core/AsciiText.hpp`), so write `-`, never an em dash.

## The manifest

`MANIFEST.sha256` is committed alongside the probes and is what makes the set
frozen. It is written and verified by `tools/probe_lib.py` shelling out to
`shasum -a 256` / `sha256sum` — deliberately not `hashlib`, because the C++
loader must verify the same digests and this repo vendors no SHA-256. Adding a
crypto dependency for a drift check would fail CLAUDE.md.

**TODO(step 7):** if that split proves awkward, the recorded fallback is
`MANIFEST.fnv` using `promptFingerprint()` — weaker, but honest,
dependency-free, and still catches every accidental edit. Pick one, record the
choice here, and do not leave both.

## Seeding v1

From `saves/ratings/candidates.jsonl` and `rejected.jsonl` (the issue #118
loop), plus the two leaks measured and written up in the header of
`tools/role_leak_probe.py` — the mood leak (guilty Marge goes angry on turn
one) and the action leak (the killer emitting `call_police`). Those are known
real failures, which is exactly what a probe should be made of.
