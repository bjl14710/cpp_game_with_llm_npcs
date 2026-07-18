# Vision: Character.ai in a 3D world

A roadmap from "a little LLM-NPC city that runs on local Ollama" to a product
where players **build their own characters with one AI and talk to them through
another**, inside a walkable 3D world.

This document is the long-horizon plan. It explains where the project is today,
the architecture the product needs, what it costs to run, how it could be sold,
and the staged path between here and there. The pricing and API facts were
researched in June 2026; see [Sources](#sources).

---

## 1. The idea

Today every inhabitant of the city is a live LLM character: walk up, press `T`,
talk, and they answer in character and remember the conversation. The long-term
product turns that from a fixed cast into **a sandbox the player populates**:

- A player describes a character in a sentence ("a grumpy retired astronaut who
  runs a tea shop and hates pigeons").
- A **creation** model turns that into a full persona — identity, voice,
  knowledge, quirks — and drops them into the world as a new NPC.
- The player then **converses** with that character, powered by a strong chat
  model, with the character staying in role.

Think **Character.ai, but the characters live in a 3D town you walk around in**,
and you author them yourself. The game engine is the shell; the LLMs are the
soul.

---

## 2. Where we are today (and what this branch added)

**Already shipped:** a hand-rolled OpenGL/SFML first-person engine, a downtown
map, procedurally-generated crowds and traffic, a law/arrest system, weapons and
an economy, and the directive-tag protocol (`[[ACTION:]]`/`[[MOOD:]]`/
`[[CHARGE:]]`) that lets a model *act* in the world, not just talk.

**This branch (`8-pluggable-llm-and-one-click`) lays the platform groundwork:**

- **One-click launch.** `run.command`/`run.sh`/`run.ps1`/`run.bat` start the LLM
  server, pull the model on first run, build, and launch — no manual setup.
- **A pluggable LLM backend.** `LlmClient` now delegates to an `LlmBackend`
  interface. `OllamaBackend` is the local default; `OpenAiBackend` speaks the
  OpenAI-compatible wire format used by OpenRouter, OpenAI, Gemini's compat
  layer, and even Ollama's own `/v1`. Switching providers is a config change.
- **In-game model picker.** Pick the model NPCs think with from the pause menu;
  the choice persists.
- **Cloud-ready, key-safe.** Cloud HTTPS turns on automatically when the build
  finds OpenSSL; the API key is read from an environment variable or a
  gitignored `config/secrets.cfg` and is **never** committed.

The remaining work is product, not plumbing.

---

## 3. Product architecture

### 3.1 One gateway, many models — OpenRouter

[OpenRouter](https://openrouter.ai) exposes a single OpenAI-compatible endpoint
(`https://openrouter.ai/api/v1/chat/completions`) and **one** API key that
reaches Claude, GPT, Gemini, and 300+ other models. Switching models is a string
change (`anthropic/claude-haiku-4.5`, `google/gemini-2.5-flash`,
`openai/gpt-5-mini`, …). Because our `OpenAiBackend` already speaks that format,
**the gateway is done** — the product just needs a key and model strings.

This directly serves the "install and add more later" goal: new models appear
without code changes.

### 3.2 Two LLM roles

The product uses LLMs in two distinct ways, and they want different models:

| Role | When | What it does | Good fit | Why |
|------|------|--------------|----------|-----|
| **Creation** | once per character | sentence → a full `.persona` (identity, traits, voice, knowledge, quirks) | Gemini 2.5 Flash / Flash-Lite | one-shot, cheap, strong instruction-following |
| **Conversation** | every line of dialogue | stays in character, streams a reply, emits action/mood tags | Claude Haiku 4.5 / GPT-5 mini | fast, cheap, good roleplay + tool-tag discipline |

Creation reuses what already exists: the `.persona` file format
(`PersonaLoader`) and `Persona::renderSystemPrompt()`. A creation call is just a
non-streaming request whose system prompt says "emit a persona file in this
exact format," whose output is written to `personas/` and loaded into a new NPC.

### 3.3 The in-game character creator (next big feature)

1. Player opens a "Create a character" prompt and types a description.
2. A creation-model request returns a `.persona` (validated against the parser).
3. The file is saved and an NPC is spawned on open ground (reusing the existing
   procedural-placement code path).
4. The player walks up and talks — now powered by the conversation model.

Stretch: generate an appearance seed (the renderer is already procedural and
seed-driven) and a portrait, so created characters look distinct.

### 3.4 User content

Personas are plain text files, so characters are trivially **shareable**: export
a `.persona`, import someone else's, or browse a community pack. This is the
seed of a marketplace (see roadmap Phase D).

---

## 4. What it costs to run

This is the number that decides the business. Assume a typical NPC turn:

- **Input ≈ 800 tokens** — persona + action/mood protocol (~500, cacheable) +
  short history (~280) + the player's line (~20).
- **Output ≈ 60 tokens** — NPCs speak in 1–3 short sentences by design.

Per-message cost at June-2026 list prices (input / output per million tokens):

| Model | Price (in / out) | Cost / message | Messages per $1 |
|-------|------------------|----------------|-----------------|
| GPT-5 mini | $0.125 / $1.00 | ~$0.00016 | ~6,000 |
| Gemini 2.5 Flash | $0.30 / $2.50 | ~$0.0004 | ~2,500 |
| Claude Haiku 4.5 | $1.00 / $5.00 | ~$0.0011 | ~900 |

**Creating a character** (one-shot, ~250 in / ~400 out) costs roughly
**$0.001 on Gemini Flash** — about a tenth of a cent, ~900 characters per dollar
(Flash-Lite is cheaper still).

**Levers:**

- **Prompt caching** discounts repeated input (the persona + protocol block) by
  up to ~90%, cutting per-message cost by roughly a third to a half in practice.
- **Batching** (where applicable) is ~50% off, though it doesn't fit live chat.
- **Model choice** spans a ~7× range; the conversation model is the main dial.

**Takeaway:** a dollar buys *thousands* of in-character messages and *hundreds*
of created characters. The token cost is small enough that almost any sane
pricing covers it with margin.

---

## 5. How it could be sold

Three models, roughly in order of effort and liability:

1. **Bring-your-own-key (BYO-key).** The player pastes their own OpenRouter key
   (exactly what `config/secrets.cfg` / the env var already support). You sell
   the *game*; they pay the provider directly. **No key liability, no billing
   backend, ship today.** Best first commercial step.
2. **Tiered: free local + paid cloud.** Ollama stays the free, offline default;
   "smart characters" is a paid upgrade that flips the provider to cloud. Natural
   fit for the existing provider switch.
3. **Hosted credits / token packs.** You sell credits; the game spends them on
   cloud LLMs. This is the "Character.ai but you own the world" full product —
   and it **requires a thin server-side proxy**: the game calls *your* server,
   your server holds the real key and meters usage. **Never ship your own key in
   the client** — it would be extracted instantly. This is the only option that
   needs real backend/billing/abuse infrastructure.

Given the cost model in §4, credit packs (or a small subscription) clear the
token cost with healthy margin; the hard parts are billing, abuse, and
moderation, not unit economics.

---

## 6. Staged roadmap

| Phase | Scope | Main risk |
|-------|-------|-----------|
| **A — Platform groundwork** *(this branch)* | one-click launch, pluggable backend, OpenRouter-ready cloud path, in-game model picker, key-safe secrets | none material; cloud is dormant until a key is added |
| **B — Cloud live + creator** | a real OpenRouter key path end-to-end; the in-game AI character creator (sentence → persona → spawned NPC) | persona-format drift (validate model output against the parser); cloud latency vs local |
| **C — BYO-key product** | polished key entry/onboarding, the free-local / paid-cloud tier toggle, packaging & code-signing for distribution | UX of key setup for non-technical players; platform packaging |
| **D — Hosted credits + marketplace** | server-side proxy holding the key + metering, credit/billing system, persona sharing/marketplace, moderation pipeline | key security, payment/abuse, content moderation at scale |

Phase A is complete. Phase B is the next concrete build and the one that makes
the product demo-able.

---

## 7. Risks & open questions

- **Key security.** For any resale model the key lives only on a server you
  control; the client talks to your proxy. BYO-key sidesteps this entirely.
- **Content moderation.** User-authored characters can be steered to misbehave.
  Provider safety layers help, but a public product needs prompt hardening, a
  moderation pass on creation, and reporting.
- **Latency.** Local Ollama is LAN-fast; cloud adds round-trip + queueing. The
  streaming UI hides much of it, but the conversation model should be a "fast"
  tier (Haiku/mini/Flash), not a frontier model.
- **Cost runaway.** Per-user budgets and provider-side rate limits are needed
  before credits or a shared key are exposed.
- **Determinism for tests.** Cloud replies aren't reproducible; keep the offline
  Ollama path (and the existing fakes) as the test substrate.

---

## Sources

Pricing and API facts gathered June 2026 (figures move; re-check before relying
on them):

- Anthropic / Claude Haiku 4.5 pricing — <https://www.cloudzero.com/blog/claude-api-pricing/>
- OpenAI / GPT-5 mini pricing — <https://www.cloudzero.com/blog/openai-pricing/>
- Google / Gemini 2.5 Flash pricing — <https://ai.google.dev/gemini-api/docs/pricing>
- OpenRouter (unified OpenAI-compatible gateway, 300+ models, streaming) — <https://openrouter.ai/docs/quickstart>
- Ollama OpenAI-compatible endpoint (`/v1/chat/completions`) — <https://ollama.com/blog/openai-compatibility>
