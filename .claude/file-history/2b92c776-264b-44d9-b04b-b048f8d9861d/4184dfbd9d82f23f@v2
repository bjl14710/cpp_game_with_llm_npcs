# Game Feel & Creative Audit — LLM NPC City

Date: 2026-07-07
Scope: A creative / game-feel audit of the whole project as it stands on
`feature/player-journal` (milestones through #88). This is a **design document,
not a work order** — nothing here is implemented. Every recommendation is
grounded in the actual code (file:line where it matters) and sized S/M/L so you
can gauge effort before committing.

## How to read this

- Each recommendation has a stable **ID** (e.g. `PACE-1`), a **Now / Change /
  Why this game / Size** block, and an **`/idea` seed** — a paste-ready prompt
  that already contains enough scope to hand to `/idea` without re-explaining.
- Sizes: **S** = a session or two, one localized system. **M** = a real feature,
  several files / a new subsystem, still bounded. **L** = substantial, multi-part.
- Read the two framing notes below first — they change how you should weigh
  everything else.

---

## Framing note 1 — a premise correction

The brief described this as a "top-down, conversation-driven" game. **It is not
top-down.** It is **first-person**: perspective camera at eye height 1.7,
70° FOV, mouse-look, with a first-person weapon viewmodel in the lower-right
(`RaylibRenderer.cpp:138-147`, `main.cpp:103-109`). That matters for this audit
because first-person raises the bar on exactly the things that are currently
missing — footstep audio, head-bob, camera feel, view-space feedback — and it
changes what "seeing your character" means for the character creator (you only
see your own avatar in multiplayer or a menu preview, never in normal play).

If a top-down or third-person camera is actually the direction you want, that is
itself a large creative decision and should be its own `/idea` before any of the
polish below — the two cameras want different feedback design. This document
audits the game **as it is: first-person.**

## Framing note 2 — the central thesis

**This project's soul is ~9 KB of hand-written persona prose, and almost none of
it reaches the player's eyes or ears.**

The ten residents are genuinely excellent: named, voiced, and densely
interlinked. Marge the baker knows Officer Brooks buys her rye every Friday and
that Theo "burns his espresso"; Theo disputes exactly that; Hal the hardware
owner and Gus the hot-dog vendor have a running feud about a squeaky cart
bearing; Whitfield the retired teacher taught half the shopkeepers. This is a
coherent, walkable social web authored entirely in text
(`personas/*.persona`, roster fixed at 10 by `tests/test_persona_roster.cpp:22`).

But a player who *looks at and listens to* the world gets a silent, static asset
grid with a nice sky. The town has **no name**. The authored shop signs
("Marge's Bakery") exist as data and are **never rendered**
(`City.cpp:27-34`; no `DrawText3D`/billboard for building names anywhere). The
feuds are invisible. There is **no audio of any kind** — not one sound
(`grep` for `InitAudioDevice|LoadSound|PlaySound` across `src/` finds only a
comment). The fountain Benny says "hums in B-flat" is silent.

And the one thing that persists — the thing the design already gets right — is
what you **say**, not what you **do**. NPC memory and gossip carry your words
across sessions (`conversations.sqlite3`, `facts.sqlite3`); meanwhile your
deeds evaporate: the clock resets to 09:00, killed NPCs revive, jail leaves no
record, ammo only ever depletes (see Section 4).

Almost every recommendation below is a specific instance of one idea: **close
the gap between the quality of the writing and the poverty of its
presentation, and give the systems you already built (memory, gossip, journal,
contradictions, schedules) a reason to matter to the player.**

---

## Top of the backlog — if you only do five things

Ranked by impact-per-effort across all five sections:

| Rank | ID | One line | Size |
|------|-----|----------|------|
| 1 | **PACE-1** | Give the already-built gossip + journal + contradiction machinery a goal: a light rumor/mystery loop | M |
| 2 | **FEEL-1** | Add an audio layer — the game is 100% silent; this is the single biggest feel deficit | M |
| 3 | **UX-1 / UX-3** | Fix the core talk loop: stop wiping transcripts, add scrollback, add a persistent NPC header | S–M |
| 4 | **ATM-1 / ATM-2 / ATM-5** | Make the writing visible: render shop signs, name the town, seed the gossip bus with the authored feuds | S |
| 5 | **VIS-3** | Replace the raylib default bitmap font everywhere — it makes the whole game read as a debug build | S |

`PACE-6` (pick a genre spine) technically precedes all of this — it's a decision,
not a build, and it's the first thing to resolve.

---

# Section 1 — Visual style & art direction

**State of play.** There is genuine art direction here, and it is undermined by a
few loud placeholders. The KayKit city-bits models (buildings, roads, cars,
props) and rigged KayKit adventurer NPCs are cohesive and intentional; the
four-band day/night sky and the exp² distance fog give the whole scene real
atmospheric unity — the fog shader is bound to every material *and* wraps the
primitive batch, so everything hazes together (`Assets.cpp:226-230, 265-269`;
`RaylibRenderer.cpp:154-160`). The palette is a deliberate warm, low-saturation
low-poly daytime. That's a real identity.

What breaks it: the "lighting" is a single flat brightness scalar with no
direction and **no shadows anywhere**, so everything floats; player-created
avatars are untextured primitive spheres/cylinders sitting next to textured
models; and all UI uses raylib's default debug font.

### VIS-1 — Unify the two character idioms (pick one and commit)

**Now.** Two entirely different construction methods share only a 1.8-unit
height contract. NPCs and remote players are textured, rigged KayKit glTF
models with skeletal animation (`RaylibRenderer.cpp:254-306`). Player-created
avatars are untextured flat-colored primitives — a tapered cylinder body, a
sphere head, cube eyes — assembled from the socket system, faking a walk with a
`sin` bob and no skeleton (`RaylibRenderer.cpp:308-382`, `RaylibRenderer.hpp:73`).

**Change.** Decide on ONE character language. Either (a) commit the whole cast to
the geometric/primitive look as a real style choice — Mii / *Human: Fall Flat* /
*Poly* territory — and re-skin the KayKit NPCs into the same vocabulary, or (b)
build player avatars out of the same KayKit part vocabulary the NPCs use so a
created character is indistinguishable from a resident. Right now the primitive
avatars read as placeholder programmer art next to the models, and the socket
system (the thing you invested in) is on the weaker-looking side of the divide.

**Why this game.** The headline feature of the character creator is "make a
resident who lives in this town." That promise dies the instant a created
avatar stands next to Marge and looks like a different game's debug capsule.
Cohesion of the cast is the whole point of a town of characters.

**Size.** M if you fold player parts onto the KayKit skeleton; L if you restyle
the entire cast into the primitive idiom.

**`/idea` seed:** *"The game has two visually inconsistent character systems —
rigged textured KayKit NPCs vs untextured primitive player-created avatars.
Design a single unified character look so a player-created resident is
visually indistinguishable from an authored NPC. Evaluate both directions
(port player parts onto the KayKit rig vs restyle all NPCs into the primitive
socket idiom) and recommend one, honoring the existing socket-contract and
1.8-unit size-contract systems."*

### VIS-2 — Ground everything with contact shadows

**Now.** No shadows of any kind — no shadow map, no blob, no ambient occlusion.
"Light" is one scalar that multiplies fragment RGB (`Assets.cpp:61`,
`DayNight.hpp:54-65`). Characters and props visually float on the ground plane.

**Change.** Add a cheap grounded shadow under every character and prop — a dark
soft-edged blob decal or a projected disc. No need for real shadow mapping in v1.

**Why this game.** In first-person you are constantly walking up to characters and
standing at conversation range (3.5 units) looking right at their feet. Floating
NPCs are the most visible break in an otherwise coherent low-poly scene, and
contact shadow is the single cheapest fix with the highest grounding payoff.

**Size.** S.

**`/idea` seed:** *"Characters and props in the scene cast no shadow and appear
to float. Add cheap contact shadows (blob decals or projected discs) under every
character and prop, tied into the existing day/night light level so they fade at
night. No full shadow-mapping required."*

### VIS-3 — Replace the raylib default bitmap font across all UI

**Now.** Every piece of text — dialogue transcript, input box, nameplates, menus,
HUD — uses raylib's built-in default bitmap font at small sizes
(`DialogUI.cpp:15,188`; `Menu.cpp` throughout; no `LoadFont`/`LoadFontEx`
anywhere in `src/`). It is pixel-blocky and generic.

**Change.** Choose and load one or two typefaces with real character (a warm
humanist sans for UI, maybe a distinct face for names/signs), size the UI around
them, and render them with proper measured layout instead of the current
9px-per-char wrap estimate (`DialogUI.cpp:141-142`).

**Why this game.** This game is *read* far more than it is watched — the player
spends most of their time looking at streamed NPC dialogue. Typography is a huge
fraction of a text-forward game's identity and mood (compare *Kentucky Route
Zero*, *Night in the Woods*, *Pentiment*, all of whose fonts are load-bearing
art direction). The default font alone makes the entire game read as an
unfinished tech demo regardless of how good the scene looks.

**Size.** S.

**`/idea` seed:** *"All in-game text uses raylib's default bitmap font, which
makes the game look like a debug build. Choose a typeface with character for a
text-forward LLM-dialogue game, load it, restyle the dialogue box / nameplates /
menus / HUD around it, and replace the crude fixed-width wrap estimate in
DialogUI with measured text layout."*

### VIS-4 — Give day/night a directional key light and long shadows

**Now.** Day/night only changes three colors (sky, fog, a global brightness
multiply) — there is no light *direction*, so dawn and dusk are a color wash,
not a raking light (`DayNight.hpp`, `RaylibRenderer.cpp:121-135`). Noon and
sunset differ only in tint.

**Change.** Add one directional light whose angle and warmth are driven by the
same world clock, and (paired with VIS-2) let it drive long, angled shadows at
dawn/dusk. Even a faked half-lambert term in the existing fog shader would add
form and time-of-day drama.

**Why this game.** You already invested in a full day/night cycle as a signature
feature, and it currently pays off only as a background gradient. Directional
light is what makes "it's evening" *feel* like evening — long shadows across the
plaza, warm light down one side of a building — and it makes the town worth
looking at as the clock turns, which is the whole reason the clock exists.

**Size.** M.

**`/idea` seed:** *"The day/night cycle only changes sky/fog color and a flat
brightness scalar — there is no directional lighting, so dawn/dusk read as a
color wash with no raking light or shadows. Add a single directional light whose
angle and color track the existing world clock (DayNight.hpp), integrated with
the fog shader, to give the town form and a real sense of time passing."*

### VIS-5 — Write and commit to an art-direction bible, then tune to it

**Now.** The palette is coherent-by-instinct (warm low-sat flats + fog) but there
is no stated target — no document that says what this town is *supposed* to feel
like, so tuning decisions (fog density 0.006, warm tint 1.06/1.0/0.92, sky ramps)
are individually reasonable but undirected (`Assets.cpp:61,89`, `DayNight.hpp`).

**Change.** Decide the identity in one page — cozy storybook? sun-bleached
Americana? gentle noir? — then tune fog, tint, sky ramps, material saturation,
and the character palette toward that single reference. This is the cheap
decision that makes VIS-1..4 pull in the same direction instead of each being a
local fix.

**Why this game.** A ten-resident town lives or dies on feeling like *a place with
a personality*, and right now the visual mood is pleasant but anonymous —
it could be any low-poly asset demo. The writing already has a strong voice
(warm, small-town, gently comic); the visuals should be tuned to match *that*
specific tone, not to a generic "nice low-poly" default.

**Size.** S for the doc; M for the tuning pass it implies.

**`/idea` seed:** *"The visuals are coherent but undirected — warm low-poly flats
with fog, but no stated art-direction target. Produce a one-page art-direction
bible that commits to a specific mood matching the warm small-town comic tone of
the persona writing, then specify the concrete tuning (fog density, tint, sky
color ramps, saturation, character palette) that would move the current look
toward it."*

---

# Section 2 — Game feel / juice

**State of play. This is the weakest area, and it has one dominating cause: the
game is completely silent.** No music, no ambience, no footsteps, no UI clicks,
no gunshot, no voice — a full grep confirms zero audio anywhere in `src/`. On top
of that: movement is instant (no acceleration), the camera has no smoothing / no
shake / no head-bob, animation hard-switches between clips with no crossfade,
and NPC facing snaps instantly. The one genuine juice primitive in the whole
game is the red hurt-vignette when you're shot (`main.cpp:1094-1098`) — proof the
team *can* do feel; it just hasn't been applied anywhere else.

### FEEL-1 — Add an audio layer (highest ROI in the document)

**Now.** Silence. `InitAudioDevice` is never called; no sound or music asset is
ever loaded or played anywhere in the codebase. The only occurrence of the word
"audio" is a comment describing future intent (`CombatEvents.hpp:9`).

**Change.** Stand up audio: an ambient town bed that shifts with day/night,
footstep sounds synced to walk cadence, UI/typewriter blips for dialogue, a
looping fountain hum near the plaza, and combat sounds (gunshot, melee impact,
hurt). Start minimal — ambient bed + footsteps + UI clicks — then layer.

**Why this game.** Silence is uncanny in a first-person walking game, and it is
*doubly* uncanny in a town that the writing insists is full of life and sound
(Benny busking, the fountain humming in B-flat, Gus's squeaky cart). The gap
between "a character tells you the fountain hums" and "the fountain is silent"
actively undercuts the fiction. Audio is also the cheapest possible way to make
the town feel inhabited — a distant murmur and birdsong do more for
"lived-in" than any number of props.

**Size.** M for the full layer; a first slice (device init, ambient loop,
footsteps, UI clicks) is S–M.

**`/idea` seed:** *"The game has no audio at all — total silence. Design and build
an audio layer: init the raylib audio device, add a day/night-aware ambient town
bed, footstep sounds synced to movement, UI/typewriter sounds for the dialogue
box, a positional fountain-hum loop in the plaza, and combat sounds. Specify a
minimal first slice and the full layering. Source CC0 audio the way the project
sources CC0 art."*

### FEEL-2 — Make the reply *land* — conversation juice

**Now.** An NPC reply streams token-by-token into the transcript with a `_`
cursor (`DialogUI.cpp:154-162`) and the mood face changes on the billboard —
but there is no sound, no punctuation-paced reveal, no motion on the speaker.
The single most-repeated moment in the game (reading a reply) has almost no feel.

**Change.** Give the reply landing weight: a soft per-character typewriter tick
(from FEEL-1), a reveal cadence that pauses on sentence punctuation, and a subtle
"speaking" motion on the NPC (a small head bob / gesture clip while text streams,
settle when done). Punctuate the *end* of a reply with a tiny emote pop.

**Why this game.** Talking is not a side activity here — it is the entire game,
done hundreds of times. Every ounce of juice on the reply moment compounds more
than juice anywhere else. This is where *Firewatch*, *Kentucky Route Zero*, and
*Oxenfree* spend their feel budget, and it's why their conversations feel
intimate rather than transactional.

**Size.** S.

**`/idea` seed:** *"Talking to NPCs is the core loop but the reply moment has
almost no feel — text just streams in silently. Add conversation juice: a
punctuation-paced typewriter reveal with a soft per-character sound, a subtle
'speaking' animation on the NPC while their reply streams, and a small emote
beat when the reply finishes."*

### FEEL-3 — Camera and movement feel pass

**Now.** Movement is instant velocity with no acceleration or deceleration —
release a key and you stop dead (`main.cpp:540-545`, `kWalkSpeed 7.0`). The
camera is rebuilt raw each frame with no smoothing, no head-bob, no FOV kick, no
shake ever (`RaylibRenderer.cpp:137-147`). Entering/leaving dialogue hard-flips
the cursor and freezes the player instantly (`main.cpp:582-597`).

**Change.** Add light movement accel/decel; a subtle walk head-bob (which also
gives footsteps something to sync to); a small, optional easing on the dialogue
transition instead of the instant freeze + cursor snap; and reserve a gentle FOV
nudge or camera settle for entering conversation.

**Why this game.** First-person makes the body the camera — floaty, weightless
first-person movement is felt immediately and constantly. And the jarring
cursor-flip/freeze on every single conversation (done hundreds of times) is a
papercut that a short easing would soften into something that feels like
"leaning in to talk" rather than "the game paused."

**Size.** S–M.

**`/idea` seed:** *"First-person movement and camera feel weightless: instant
velocity with no accel/decel, no head-bob, no camera smoothing, and a hard
cursor-flip + freeze every time a conversation opens. Design a movement/camera
feel pass — light acceleration, a subtle walk head-bob, and a smooth
'lean-in-to-talk' transition into and out of dialogue."*

### FEEL-4 — Extend the existing juice vocabulary to more events

**Now.** You have exactly two feel primitives and they're good: the red
hurt-vignette on being shot (`main.cpp:1094-1098`) and the viewmodel punch/recoil
tween (`World.cpp:145`, `RaylibRenderer.cpp:412`). Everything else — hitting an
NPC, firing the (invisible) pistol projectile, the arrest teleport, death /
respawn — has no view-space feedback at all. The pistol bullet is simulated but
never even drawn (`World.cpp:34-42`).

**Change.** Reuse the vocabulary you already have: a brief hit-flash on a struck
NPC, a small screen-shake / kick on melee and gunshot, a visible tracer or muzzle
flash for the pistol, a whoosh + fade on the arrest teleport, and a proper fade
on death/respawn instead of the instant cut. Build one tiny easing/tween helper
(there is none — all fades are hand-rolled linear) so these are consistent.

**Why this game.** Combat is a secondary system here, but when it happens it
currently feels like nothing — you click and an NPC silently plays a death clip.
Even as a minor mode it should read as consequential, and a small shared juice
toolkit (easing, shake, flash) pays forward into the conversation and UI polish
above.

**Size.** S.

**`/idea` seed:** *"Combat and state-change events (hitting an NPC, firing the
pistol, arrest teleport, death/respawn) have no view-space feedback — the only
existing juice is a hurt-vignette and a viewmodel recoil. Add a small shared
easing/tween + screen-shake + flash helper and apply it: hit-flash on struck
NPCs, a visible pistol tracer/muzzle flash, screen kick on attacks, and fades on
teleport and death."*

---

# Section 3 — UX & dialogue flow

**State of play.** The conversation interface is functional and has some real
craftsmanship — token streaming, a colored transcript (player blue / NPC gold /
system gray), a "thinking" indicator, and a clean one-message-at-a-time guard.
But for the interaction the player performs *hundreds of times*, it has
accumulated friction that a cozy talk-heavy game cannot afford: the transcript is
**wiped every time you re-open a chat**, there is **no scrollback**, the
who-am-I-talking-to line **scrolls away**, the player is **rooted in place** and
can't queue or act while the model thinks, and the "thinking" animation is
**dead code** where a live one was intended.

### UX-1 — Stop discarding transcripts; add scrollback

**Now.** `dialog.reset()` clears the entire transcript every time a conversation
opens (`main.cpp:576`, `DialogUI.cpp:127-134`), and there is no scroll control at
all — only the most-recent lines that fit on screen are visible; older lines
scroll off and cannot be recalled (`DialogUI.cpp:171-191`). The player's actual
history of talking to Marge is destroyed the moment they walk away and come back.

**Change.** Persist a per-NPC transcript so re-opening a conversation shows what
was said before, and add mouse-wheel / page scrollback within a conversation. The
NPC's LLM-side history already persists (`Npc.hpp:32`, capped 10 turns) — this is
about showing the *player* the same continuity the *model* already has.

**Why this game.** The transcript is the single most valuable artifact the game
produces — it *is* the content. Throwing it away on every conversation, in a game
whose entire pitch is "these characters remember you," is the sharpest
contradiction between what the systems do and what the player sees. And with no
scrollback, a long reply can push the start of the exchange off-screen mid-chat.

**Size.** S–M.

**`/idea` seed:** *"The dialogue transcript is wiped every time a conversation
re-opens, and there is no scrollback within a conversation, so the player's
record of what an NPC said is destroyed even though the NPC's LLM history
persists. Persist a per-NPC visible transcript across conversations and add
mouse-wheel/page scrollback in the dialogue box."*

### UX-2 — Live thinking indicator + never trap the player

**Now.** The "`... X is thinking...`" indicator is **static text**, not animated
(`DialogUI.cpp:163-168`) — and the dedicated `busy()` / `setThinking()` plumbing
built for exactly this is **dead code, never called anywhere** (grep confirms
zero call sites). While the model thinks, the player is frozen, cursor-locked,
input disabled, unable to queue a next line or do anything but wait
(`main.cpp:585-620`).

**Change.** Animate the indicator (pulsing dots / a small spinner), wire up the
already-built `setThinking` path, and make sure Esc always closes instantly even
mid-generation. Consider letting the player pre-type their next line while the
reply streams.

**Why this game.** Local LLM latency is real and unavoidable — the player waits
for *every* reply. A dead, static "thinking" string during a multi-second wait
reads as a hang. This is the moment the interface most needs to feel alive, and
the code to make it so is already written and sitting unused.

**Size.** S.

**`/idea` seed:** *"The 'NPC is thinking' indicator during LLM latency is static
text, and the setThinking()/busy() plumbing built for it is dead code. Wire up a
live animated thinking indicator, guarantee Esc closes the conversation instantly
even mid-generation, and let the player pre-type their next message while a reply
streams."*

### UX-3 — Persistent conversation header (name, role, mood, relationship)

**Now.** Who you're talking to is established by one seeded system line ("You are
talking to Marge…", `main.cpp:579-581`) that scrolls away, plus the speaker-name
prefix on each transcript line. There is no fixed header, no portrait
(`DialogUI` has no header element at all).

**Change.** Add a persistent header to the dialogue box: NPC name, role, their
current mood face (you already bake six mood textures — reuse them), and, once
PACE-2 exists, a relationship/trust indicator. It never scrolls.

**Why this game.** In a ten-character town you are constantly context-switching
between residents, and the mood face is one of the game's signature touches yet
it only appears as a small billboard out in the world, not in the conversation
you're actually having. A fixed header anchors the exchange and puts the
character's *face and feeling* where the player is looking.

**Size.** S.

**`/idea` seed:** *"The dialogue box has no persistent header — who you're talking
to is only a system line that scrolls away. Add a fixed conversation header
showing the NPC's name, role, and current mood face (reuse the baked mood
textures), with room for a relationship indicator later."*

### UX-4 — Conversation starters / suggested prompts

**Now.** The player faces a blank input box with no hint of what this particular
NPC can talk about. The rich `knowledge` boundaries in each persona (Marge knows
bread and her regulars; the librarian won't discuss the internet) are invisible
until you happen to ask the right thing.

**Change.** Offer 2–4 context-aware suggested prompts when a conversation opens
(and after replies) — e.g. "Ask about the fountain," "Ask about Officer Brooks,"
"Ask what's fresh today." Clicking one fills/sends it; typing is still free.

**Why this game.** An LLM NPC is an open text box, which is simultaneously its
magic and its intimidation — new players don't know what's *in scope* for this
character or that the town is densely cross-referenced. Suggestions teach the
affordance (these people know specific, interconnected things) and are the
fastest on-ramp to discovering the social web. Compare the topic wheels in
*Oxenfree* / Bioware dialogue, adapted to free text.

**Size.** M.

**`/idea` seed:** *"New players face a blank text box and can't tell what a given
NPC knows about, so the rich authored knowledge/cross-references stay hidden.
Add 2–4 context-aware suggested conversation prompts per NPC (derived from their
persona knowledge and the town's social web) shown on conversation open and after
replies; clicking sends, free typing still works."*

### UX-5 — Input editing & history recall

**Now.** The chat input is append-only: printable ASCII only, backspace-from-end
only, no left/right cursor, no recall of previous lines, no paste
(`DialogUI.cpp:71-93`). No length cap (unlike the menu fields).

**Change.** Add up-arrow recall of your recent messages, basic cursor
movement/editing, and consider allowing common punctuation the ASCII filter
currently permits but nothing beyond 126 (em-dashes, curly quotes, accented
names would all fail).

**Why this game.** You type constantly, and NPC names include characters the
filter is fine with — but the lack of any line editing or history recall makes
re-phrasing or fixing a typo mean deleting the whole line. Small, but it's felt
on every message.

**Size.** S.

**`/idea` seed:** *"The dialogue text input is append-only — no cursor movement,
no editing mid-line, no recall of previous messages, printable-ASCII only. Add
up-arrow history recall and basic in-line editing to the DialogUI input."*

---

# Section 4 — Pacing & progression

**State of play. This is the existential section. There is currently no reason to
keep playing past your first few conversations.** There is no goal, no score, no
XP, no quest, no objective, and no win or lose state anywhere in the code (a
repo-wide grep confirms it). Nothing unlocks over time; you can do exactly as
much in minute 11 as in minute 1 — arguably less, since spent pistol ammo never
comes back. The only things that carry forward are *what you said* (NPC memory)
and *facts you introduced* (gossip); every *deed* is erased — the clock resets to
09:00 each launch, killed NPCs revive, jail leaves no record.

The crucial upside: **you have already built most of a social-deduction /
mystery engine and put no goal on top of it.** Gossip facts spread NPC-to-NPC
through a shared bus; the journal already groups them by subject and *flags
contradictions between NPCs* ("conflicting accounts"); schedules make specific
characters meet at specific places and times; memory persists. That is the
skeleton of *Return of the Obra Dinn* / *The Case of the Golden Idol* /
*Pentiment* — a town of testimonies you cross-reference. It is inert only because
nothing asks the player to use it.

### PACE-6 — Decide the genre spine (do this first)

**Now.** The game is a set of well-built systems with no unifying answer to "what
am I doing." Combat, jail, economy-less shops, gossip, journal, schedules, and a
character creator coexist without a spine that tells the player why they're here.

**Change.** Pick one spine and let it subordinate the rest. Given what's already
built, the strongest candidate is a **social-deduction / small-mystery** game
(the gossip+journal+contradiction+schedule machinery points straight at it). The
alternatives are a **cozy social sim** (relationships + daily rhythm, *Animal
Crossing* / *Stardew* / *Kind Words*) or an **emergent immersive-sim sandbox**
(*The Sims* meets a talkable town). Recommend the mystery spine because it needs
the least new machinery and gives the contradiction system — which is genuinely
novel for an LLM-NPC game — a payoff.

**Why this game.** Every other recommendation in this section is a different
answer depending on this one. Deciding it is a one-page creative call, not a
build, and it prevents you from bolting a generic XP bar onto a game whose actual
distinctive asset is *a town of LLM characters who remember, gossip, and
contradict each other.*

**Size.** S (a decision doc). Gates everything below.

**`/idea` seed:** *"The game has many built systems (gossip, journal with
contradiction flagging, schedules, memory, combat, character creator) but no
unifying goal. Evaluate three genre spines — social-deduction mystery, cozy
social sim, emergent immersive-sim sandbox — against the systems already built,
and recommend one with a rationale, defining what the player's core motivation
and session-to-session pull would be under it."*

### PACE-1 — Give the gossip + journal + contradiction machinery a goal (flagship)

**Now.** Gossip spreads and the journal flags "conflicting accounts" between
NPCs (`Journal.hpp:25-49`, `Gossip.cpp`), but nothing rewards the player for
gathering, spreading, or *resolving* anything. The contradiction detector — the
most original thing in the game — surfaces disagreements the player has no reason
to care about.

**Change.** Layer a light rumor/mystery loop on top: rumors the player can
*verify* by cross-referencing NPCs, contradictions that are *clues* to catch
someone in a lie, and small mysteries that resolve when the player assembles
consistent testimony (an Obra-Dinn-style "you've correctly connected who-did-what"
confirmation). No new core systems — this is a goal and a payoff on machinery
that already runs.

**Why this game.** This is the single highest-leverage change available: it turns
three systems you already shipped from set-dressing into *the game*, and it's a
genre no one else can easily do, because "a town of characters whose testimony
genuinely varies and can be contradicted" is exactly what LLM NPCs are uniquely
good at. Seeded with the authored feuds (ATM-5), you'd have a mystery whose
suspects actually improvise.

**Size.** M.

**`/idea` seed:** *"The gossip system spreads facts and the journal already flags
contradictory accounts between NPCs, but the player has no reason to care. Design
a light rumor/mystery loop on top of the existing WorldState fact bus + journal:
rumors the player verifies by cross-referencing NPCs, contradictions that serve
as clues, and small mysteries that resolve when consistent testimony is
assembled — Obra Dinn / Golden Idol style, reusing the fact-store and
contradiction machinery rather than adding new core systems."*

### PACE-2 — Relationship / trust progression per NPC

**Now.** NPCs remember you across sessions (`ConversationStore`), but that memory
gates nothing and tracks no number — it's narrative continuity with no mechanical
consequence. Every NPC is exactly as open to you in minute 11 as at hello.

**Change.** Add a per-NPC trust/relationship value that rises with conversation,
remembered kindness, and helpful acts, and gates deeper disclosure — secrets,
new suggested topics, schedule invitations, willingness to gossip *to* you. Store
it alongside the memory that already persists.

**Why this game.** This is the "what can I do in minute 11" answer that fits the
grain: not new weapons, but characters who *open up*. It's the *Persona* social
link / *Stardew* hearts model, and it's especially potent with LLM NPCs because
"trust level" can be injected into the prompt to genuinely change how a character
talks to you. It also gives the character creator's personas a progression to
participate in.

**Size.** M.

**`/idea` seed:** *"NPC memory persists but gates nothing — every NPC is equally
open from the first hello. Add a per-NPC trust/relationship value that rises with
conversation and remembered kindness, persists with the existing memory store,
is injected into the NPC's prompt to change how they speak to the player, and
gates deeper disclosure (secrets, new topics, gossip, schedule invites)."*

### PACE-3 — A daily rhythm with reasons to return

**Now.** There's a full clock and schedules, but nothing is time-gated, timed, or
rewarded — no shop hours that matter, no curfew, no event at a set hour. Time
only moves the scenery and where four NPCs stand, and it resets to 09:00 every
launch (`WorldState.cpp:11-13`).

**Change.** Give the day structure the clock can drive: a recurring town event at
a set hour (Benny's evening set in the park; the bakery's morning rush), moments
that only happen at a time/place, NPCs whose availability or mood depends on their
schedule. Make "come back tomorrow / this evening" mean something.

**Why this game.** A living town needs a *rhythm*, not just a clock — the reason
*Animal Crossing*, *Night in the Woods*, and *Shenmue* pull you back is that the
day has beats you don't want to miss. You already pay the cost of a day/night
cycle and schedules; this is the design that makes them a pacing engine instead
of ambient motion.

**Size.** M.

**`/idea` seed:** *"The world clock and NPC schedules exist but nothing is
time-gated or rewarded — time only changes scenery. Design a daily rhythm that
gives players reasons to be somewhere at a certain time: recurring town events at
set hours, time-and-place-specific moments, and schedule-driven NPC availability
and mood — Animal Crossing / Night in the Woods style — building on the existing
clock and Schedule system."*

### PACE-4 — Persist the world and let deeds leave a mark

**Now.** Only words persist. The clock resets to 09:00, the player's HP / ammo /
position reset, killed NPCs load alive (no death table), and arrest/jail leaves
no record. The world remembers what you *said*, never what you *did*.

**Change.** Persist world-time across sessions (the store was designed generic
enough — the value just isn't saved), and let at least some deeds carry: a killed
NPC stays gone (or is conspicuously absent/recovering), an arrest is remembered by
the cop, a reputation forms. Model it on the memory system that already works.

**Why this game.** The pitch is a *continuous, living* town. A world that snaps
back to 09:00 with everyone alive and no memory of last night's rampage
undermines exactly the continuity the memory/gossip systems are trying to sell.
Deeds mattering is what separates "a chat demo with a 3D lobby" from "a place."

**Size.** S–M.

**`/idea` seed:** *"Only conversation memory persists — the clock resets to 09:00,
killed NPCs revive, and arrests leave no record, so the player's deeds never mark
the world. Persist world-time across sessions and make key deeds durable (deaths,
arrests, a forming reputation), modeled on the existing conversation-memory
persistence."*

### PACE-5 — Feed player deeds into gossip as a reputation

**Now.** Gossip only carries facts extracted from *conversations*
(`Gossip.cpp:69-77`); the fact bus never hears about what the player physically
*does*. Attack someone in the street and no one talks about it.

**Change.** Emit player deeds (attacked an NPC, got arrested, helped someone) as
facts onto the same gossip bus, so word spreads and NPCs react — wary greetings,
refusing to talk, the cop keeping an eye on you. This is reputation for free on a
system you already built.

**Why this game.** It closes the loop between the two halves of the game (the
"immersive-sim" verbs and the "talk to everyone" verbs) that currently don't
speak to each other, and it makes the gossip system visibly *about the player*,
not just about ambient NPC chatter. It's the most natural possible payoff for the
propagation machinery.

**Size.** M.

**`/idea` seed:** *"The gossip bus only carries facts from conversations — it
never hears what the player physically does, so violence and arrests spread no
word. Emit player deeds (attacks, arrests, good turns) as facts onto the existing
gossip bus so they propagate and NPCs react to the player's reputation."*

---

# Section 5 — Atmosphere & narrative texture

**State of play.** The narrative texture is carried almost entirely by the
persona prose, which is excellent — and it is trapped there. The town is
genuinely hand-authored (one literal map function, `City.makeDowntown()`
`City.cpp:18-76`, with named landmarks and prop placement reasoned in comments),
not procedural, which is a real strength. But everything the writing promises is
invisible to eye and ear: the town has no name, the authored shop signs are
never drawn, the character feuds have no physical presence, there are no
pedestrians or moving traffic, and there is no sound. A player who reads the NPCs
feels a lived-in town; a player who only looks and listens sees a quiet static
grid.

### ATM-1 — Render the authored signage in the world

**Now.** Every named building carries authored sign text — "Marge's Bakery,"
"Bean There Coffee," "City Library," "Gus's Hot Dogs" (`City.cpp:27-34`;
`City.hpp:16-17` documents it as "sign / label text") — and **none of it is ever
rendered.** There is no 3D building-name text anywhere in the renderer.

**Change.** Draw the shop signs in the world: floating 3D text, awning labels, or
billboard signs over each named storefront. The data is already there and stable.

**Why this game.** This is the most literal possible instance of the central
thesis — the game *already knows* the bakery is called Marge's Bakery and simply
never tells the player's eyes. Rendered signs instantly make the town legible and
navigable (NPCs give directions "in terms of shops," `baker.persona`, which only
works if the player can *see* the shops), and it's nearly free.

**Size.** S.

**`/idea` seed:** *"Every named building has authored sign text (City.cpp) that is
never rendered in the 3D world. Draw the shop signs — floating 3D labels, awnings,
or billboards over each named storefront — using the existing Building.name data,
styled with the chosen UI typeface."*

### ATM-2 — Name the town and state a setting fiction

**Now.** The town has no name anywhere (grep across personas/README/docs finds
none — only "downtown"/"the city") and no stated era or premise beyond "a small
first-person 3D city where every inhabitant is a live LLM character."

**Change.** Name the town and write a one-paragraph setting fiction — when, where,
what kind of place, why these ten people. Thread the name through signage, the
menu, and personas.

**Why this game.** A place with a name is a place; an unnamed grid is a level. The
writing is *specific* about everything except where it happens, which leaves the
whole richly-detailed social web floating in an anonymous void. A name and a
sentence of premise cost almost nothing and give every existing persona line a
home.

**Size.** S.

**`/idea` seed:** *"The town has no name and no stated setting fiction. Choose a
name and write a one-paragraph premise (era, place, character of the town, why
these ten residents) consistent with the existing persona writing, and thread the
name through signage, the menu, and persona references."*

### ATM-3 — Make the persona web physically present (environmental storytelling)

**Now.** The feuds and details that make the cast feel alive are text-only: Gus's
cart "squeaks because Gus won't buy the right bearing," Hal "fixed the fountain
pump twice," Benny has an "open guitar case," Whitfield "naps on the bench."
None of it exists as a prop or an event in the world.

**Change.** Author small environmental echoes of the writing: the hot-dog cart
actually squeaks (positional audio, from FEEL-1), a little "pump repaired — twice
—H.J." detail at the fountain, Benny's open guitar case with a few coins,
Whitfield asleep on his bench in the afternoon per his schedule. Pick the
highest-flavor handful.

**Why this game.** Environmental storytelling is how a town feels *authored rather
than assembled*, and here the authoring is already done — it's sitting in the
persona files waiting to be given a physical form. Each small echo rewards players
who both talk *and* look, binding the two halves of the experience together.

**Size.** M.

**`/idea` seed:** *"The rich character details and feuds in the persona files
(Gus's squeaky cart, Hal fixing the fountain pump twice, Benny's open guitar
case, Whitfield napping on his bench) exist only as text. Pick the
highest-flavor handful and give them physical presence in the world as props,
positional audio, or scheduled bits of business, so players who look are rewarded
with the same texture players who talk get."*

### ATM-4 — Add ambient life: pedestrians and moving traffic

**Now.** The only movers are the ten named NPCs. There are no pedestrians or
crowds (grep: zero), the five cars are static collider props, and the four
traffic lights never cycle — they're just obstacles (`City.cpp:42-60`).

**Change.** Add a handful of anonymous background pedestrians on simple looping
paths and a couple of cars that actually drive the avenue grid, plus traffic
lights that cycle. Keep them clearly ambient (no dialogue) so they don't dilute
the ten principals.

**Why this game.** A town of exactly ten people standing still reads as a
diorama. Even a few looping walkers and one moving car transform the sense of the
place from "cast on a stage" to "a town the cast lives in" — and it makes the
principals feel like *the ones worth talking to* precisely because they stand out
against ambient life. (Note: a moving-traffic + crowd system already exists on the
divergent `8-pluggable-llm-and-one-click` branch — worth mining rather than
rebuilding.)

**Size.** M.

**`/idea` seed:** *"The town has only its ten named NPCs moving — no pedestrians,
static cars, and traffic lights that never cycle. Add ambient life: a few
anonymous looping pedestrians, a couple of cars that drive the avenue grid, and
cycling traffic lights, kept clearly non-interactive so the ten principals still
stand out. Check the 8-pluggable-llm-and-one-click branch, which already has a
crowd/traffic system, before building from scratch."*

### ATM-5 — Seed the gossip bus with the authored feuds

**Now.** The fact store starts **empty** every game; gossip only fills from player
conversations (`Gossip.cpp:69-77`). So the journal and any rumor spread are barren
until the player generates content — and all the authored cross-character texture
(the espresso dispute, the cart-bearing feud) lives only in static `knowledge`
strings, never on the live bus.

**Change.** Pre-seed the fact bus at startup with the feuds and rumors already
written into the personas, so the town has a living, spreading rumor-state from
minute one — and so contradictions exist for the journal to flag before the
player has done anything.

**Why this game.** This is what makes the gossip and contradiction systems
*legible immediately* instead of after an hour of priming, and it's the direct
feedstock for the PACE-1 mystery loop. The content already exists in prose; this
is transcribing it into the structured form the systems consume.

**Size.** S.

**`/idea` seed:** *"The gossip fact bus starts empty each game, so the journal and
rumor spread are barren until the player generates facts, even though the persona
files are full of authored feuds and rumors. Pre-seed the fact store at startup
with the existing cross-character feuds and rumors (including deliberately
contradictory accounts) so the town has a living, spreading rumor-state and
flaggable contradictions from minute one."*

### ATM-6 — Ambient soundscape (atmosphere slice of FEEL-1)

**Now.** Silent — including the fountain that Benny explicitly says "hums in
B-flat" (`musician.persona`) and the busking he does for a living.

**Change.** The atmosphere-specific slice of the audio layer: a positional
fountain hum, distant town murmur, birdsong that shifts across the day/night
bands, and Benny's guitar audible as you approach the park.

**Why this game.** Ambient sound is the highest-value-per-effort atmosphere tool
there is, and the writing has *already told the player what the town sounds
like*. Delivering it closes another eye-and-ear gap and makes the day/night cycle
audible, not just visible.

**Size.** S (as a slice of FEEL-1).

**`/idea` seed:** *"Author the ambient soundscape for the town: a positional
fountain hum (the writing says it hums in B-flat), distant town murmur, day/night-
shifting birdsong, and Benny's busking audible near the park — tied into the
day/night clock. (Slice of the broader audio-layer idea.)"*

### ATM-7 — Place the unused atmosphere assets + night streetlight glow

**Now.** `streetlight.bin` and `firehydrant.bin` exist under
`assets/models/city/` but are never placed in `City.cpp`, so they never appear —
free atmosphere sitting unused on disk.

**Change.** Place streetlights and hydrants along the blocks, and give the
streetlights an emissive glow at night driven by the day/night light level
(pairs with VIS-4). Small, high-flavor, mostly authoring.

**Why this game.** Streetlights that warm up at dusk are one of the most evocative
"a town winding down for the night" signals there is, and the model is already
downloaded. It's the cheapest way to make the (currently under-delivered)
night half of the day/night cycle feel intentional.

**Size.** S.

**`/idea` seed:** *"streetlight.bin and firehydrant.bin are on disk but never
placed in the city. Place streetlights and hydrants along the blocks and make the
streetlights glow emissively at night, driven by the existing day/night light
level."*

---

# Appendix A — What's already good (don't break these)

These are strengths the recommendations above should preserve, not disturb:

- **The persona writing.** Ten named, voiced, densely interlinked residents with
  consistent geography and real comic specificity. This is the crown jewel; every
  recommendation is in service of *revealing* it, not replacing it.
- **Cross-session NPC memory.** The baker genuinely remembers you next launch
  (`ConversationStore`, `main.cpp:243-246`). Rare and valuable — the model for
  how *deeds* should persist too (PACE-4).
- **The gossip + journal + contradiction engine.** Structurally novel for an
  LLM-NPC game and 80% of a social-deduction game already built (PACE-1).
- **Atmospheric coherence via the fog shader.** One shader unifies models and
  primitives so everything hazes together — the reason the mixed-idiom scene holds
  together at all (`RaylibRenderer.cpp:154-160`).
- **The socket / size-contract character system.** The right architecture for
  composable characters; VIS-1 is about its *look*, not its bones.
- **Token-streaming dialogue.** Replies stream live token-by-token — the correct
  foundation; Section 3 is polish on top of a sound base.
- **Colliders authored from one source of truth.** Every solid prop is authored in
  `City.cpp` so visuals and collision can't diverge — keep this discipline when
  adding signage (ATM-1) and ambient props (ATM-4).

# Appendix B — References drawn on

Named so you can play/inspect the specific thing each recommendation borrows:

- **Return of the Obra Dinn**, **The Case of the Golden Idol**, **Pentiment** —
  cross-referencing testimony and catching contradictions as the core loop
  (PACE-1). Pentiment especially: a hand-authored town, dialogue-driven, with art
  style *as* identity.
- **Persona (social links)**, **Stardew Valley (hearts)** — relationship level as
  progression that gates disclosure (PACE-2).
- **Animal Crossing**, **Night in the Woods**, **Shenmue** — daily rhythm and NPC
  schedules as the reason to return (PACE-3); Shenmue specifically for NPCs on
  believable daily routines.
- **Firewatch**, **Oxenfree**, **Kentucky Route Zero** — conversation as the
  entire game, and the feel/typography that makes it intimate (FEEL-2, VIS-3,
  UX-3).
- **The Sims** — deeds feeding reputation and NPC reaction; ambient autonomy
  (PACE-5, ATM-4).
- **Kind Words** — tone reference for a gentle, talk-first game (PACE-6 cozy
  option).
