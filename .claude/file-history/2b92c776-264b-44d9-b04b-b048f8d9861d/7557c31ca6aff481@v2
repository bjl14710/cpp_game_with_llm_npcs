# Plan: Shared-World Multiplayer + Future AWS Deployment Skill
Date: 2026-07-04
Status: READY FOR IMPLEMENTATION
Estimated complexity: XL (multiplayer core), M (deploy skill, later phase)

## The Idea (one paragraph)
Today the game is single-player only: one process owns the `World`, its `Npc`s, and
a local `LlmClient` talking to Ollama. This plan adds a second mode where 2-4
friends join one shared world over the network — they see each other move, and
they see the *same* NPCs with the *same* mood and memory, because the NPC
simulation and all LLM calls run once, on the host's machine, and are broadcast
to everyone. The host runs Ollama locally (not on AWS) for now — free-tier AWS
RAM is too small to run a model reliably. AWS is a separate, later phase: a
headless "dedicated server" build and a `deploy-aws-server` skill that pushes it
to a free-tier EC2 instance once one exists.

## Goal
A player can host a game from the existing app, share an IP:port with up to 3
friends, and everyone sees each other walking the same city and talking to the
same NPCs — with NPC mood and memory staying consistent across all players.

## Out of Scope (this version)
- AWS deployment itself (Phase B, later — see below). v1 is LAN/host-machine only.
- Player accounts, matchmaking/lobby servers, or any auth beyond a join
  IP:port (+ optional plaintext join code).
- World/NPC state persistence across server restarts (resets each session).
- NAT traversal / UPnP — joining across the open internet requires the host to
  port-forward or use something like Tailscale/ZeroTier; this plan documents
  that requirement but does not automate it.
- Voice chat, player-vs-player combat/interaction, more than 4 players,
  spectator mode.
- Encryption/TLS on the game socket (LAN/trusted-friends threat model for v1).
- Cloud LLM billing/proxy UI — the host uses their own local Ollama.
- Any change to single-player behavior — it must stay byte-for-byte the same
  when the player doesn't choose Host/Join.

## Affected Areas

**New core modules (SFML-free, doctest-tested, mirroring `LlmClient`'s
worker-thread pattern):**
- `src/core/NetMessage.hpp` — message types (JoinRequest, Welcome, PlayerInput,
  WorldSnapshot, ChatOpen, ChatDelta/ChatReply passthrough, NpcMoodUpdate,
  NpcSpeechBubble, Disconnect, protocol version) serialized with the
  already-vendored `nlohmann::json` (`external/json.hpp`) — zero new deps.
- `src/core/NetFraming.hpp/.cpp` — length-prefixed message framing over a raw
  TCP stream (4-byte big-endian length + JSON payload), analogous to
  `StreamAssembler`'s line framing. Pure buffer logic, no sockets — the most
  directly unit-testable piece.
- `src/core/NetServer.hpp/.cpp` — owns the authoritative `World`/`Npc`
  simulation and the one `LlmClient`. Accepts TCP connections (raw
  BSD sockets/Winsock, matching the cross-platform pattern already used by
  vendored `httplib.h`), applies `PlayerInput`, steps the world, broadcasts
  `WorldSnapshot` at a fixed tick rate, and routes NPC chat (see below).
- `src/core/NetClient.hpp/.cpp` — connects to a host, sends local player input,
  receives snapshots and applies them to a local read-only view of remote
  players/NPCs for rendering, forwards NPC chat text the player types.

**Modified:**
- `src/core/World.hpp/.cpp` — expose a way to step simulation from external
  input (server) vs. local input (today's single-player path), without
  changing single-player call sites.
- `src/core/Npc.hpp/.cpp` / `src/core/DialogueSession.hpp/.cpp` — the server
  must serialize concurrent chat requests to the same NPC (it's already a
  single-worker-thread `LlmClient`, so this is mostly "queue and label whose
  request is whose," not a rewrite).
- `src/app/main.cpp` — new startup modes: Solo (today, unchanged), Host
  (runs `NetServer` + renders player 0 locally), Join (runs `NetClient`,
  renders local input + received snapshots).
- `src/app/Menu.hpp/.cpp` — new `Page::Multiplayer` with "Host Game" / "Join
  Game" entries and a simple text-entry for `ip:port` (+ optional join code),
  following the existing `Page`/`setModels`-style pattern from the model
  picker.
- `CMakeLists.txt` — add the new core `.cpp` files to `llm_npc_core`.
- `README.md` / `docs/developer_guide.md` — hosting/joining instructions,
  port-forwarding caveat.

**New test files:**
- `tests/test_net_framing.cpp` — length-prefix framing edge cases (split
  reads, oversized length, malformed JSON) mirroring `test_stream.cpp`'s
  style.
- `tests/test_net_protocol.cpp` — message (de)serialization round-trips.
- `tests/test_net_loopback.cpp` — `NetServer` + `NetClient` on `127.0.0.1`:
  join handshake, snapshot exchange, and one NPC chat round trip driven
  through a `FakeBackend` (no real Ollama needed, matching the existing
  `FakeOllama.hpp` test pattern).

**Later, Phase B (not built now — see below):**
- New executable target `cpp_game_server` (headless, links `llm_npc_core` +
  `NetServer`, no SFML/OpenGL — needed because AWS has no display).
- `.claude/skills/deploy-aws-server/SKILL.md` — provisioning + deploy script.

## Implementation Order

1. **`NetMessage` + `NetFraming`** — protocol types and wire framing, fully
   unit-tested in isolation, no sockets yet.
2. **`NetServer` skeleton** — accept loop, per-connection thread, join
   handshake (protocol version check), holds the authoritative `World`.
3. **`NetClient` skeleton** — connect, handshake, send/receive raw messages.
4. **Player movement sync** — `PlayerInput` → server steps `World` → broadcast
   `WorldSnapshot` → clients render remote players. Verifiable with two local
   instances on localhost before touching NPCs.
5. **Shared NPC chat routing** — client's `ChatOpen`/message → server runs the
   existing `DialogueSession`/`LlmClient` path once → streams `ChatDelta`/
   `ChatReply` back to the requesting client, and broadcasts a lightweight
   `NpcMoodUpdate`/`NpcSpeechBubble` to everyone else so bystanders see mood
   and speech without opening the conversation themselves.
6. **Menu integration** — Host/Join pages, connection status and error
   display, wired into `main.cpp`'s new startup modes.
7. **Loopback integration test** — join, move, chat, disconnect, using
   `FakeBackend` for the LLM side.
8. **Docs** — README/developer_guide hosting & joining section, including the
   port-forwarding/Tailscale caveat.

*(Phase B, later: headless server build target, then the `deploy-aws-server`
skill and the RAM/local-Ollama-vs-cloud-API decision for AWS specifically —
scoped separately once an AWS instance exists, per your answer.)*

## Acceptance Criteria
- [ ] Given a host on port P, when a second instance joins `127.0.0.1:P`, then
      both windows show two independently-moving player avatars in the same
      `World`.
- [ ] Given two joined clients, when either starts a conversation with the
      same NPC, then that NPC's mood/memory updates identically for both, and
      a third connected client sees the mood/speech-bubble update without
      opening the conversation itself.
- [ ] Given a joining client points at an unreachable or wrong `ip:port`, then
      it shows a clear connection-failed message and returns to the menu
      without crashing.
- [ ] Given the host's Ollama is unreachable, when a client sends a chat
      message, then only that client receives the existing "could not reach
      Ollama..." error text — other players are unaffected.
- [ ] Two players messaging the same NPC concurrently both get answered, in
      order, with no crash or interleaved/corrupted replies.
- [ ] `make -C tests test` passes, including the new net framing/protocol/
      loopback tests.
- [ ] Solo play (no Host/Join chosen) is behaviorally unchanged from today.

## Edge Cases and Error Handling
| Situation | Expected behaviour |
|-----------|--------------------|
| Client disconnects mid NPC conversation | Server drops that player's in-flight request; NPC state for remaining players is unaffected. |
| Host's Ollama down/slow | Existing `ChatReply.errorMessage` path reused; error routed only to the requesting client. |
| Two players message the same NPC at once | Requests serialize through the existing single-worker `LlmClient`; second player sees a "NPC is busy" indicator until their turn. |
| Wrong/unreachable join address | Client-side connect timeout → menu shows a connection-failed message, no crash. |
| Host/client running different game builds | Handshake includes a protocol version; mismatch is rejected with a clear message instead of silently desyncing. |
| Malformed or oversized incoming frame | Framing layer validates the length prefix and drops that connection cleanly rather than misreading the stream. |
| More than 4 players attempt to join | Server rejects the connection with a clear "session full" message. |

## Open Questions
- **AWS phase LLM backend**: once actually deploying to AWS free tier, local
  Ollama likely won't fit in ~1GB RAM. At that point, decide whether to (a)
  try a very small quantized model anyway, or (b) switch the *deployed*
  server to the cloud `OpenAiBackend`/OpenRouter path already built in this
  codebase. Deferred until an AWS instance exists, per your answer — not a
  blocker for v1.
- Exact tick rate / snapshot frequency for `WorldSnapshot` broadcast (start
  around 10-15Hz and tune by feel — not testable in the abstract).
- Whether the join code is just "share your LAN IP" or a short human-typed
  code the host sets — small UX decision, doesn't affect architecture.

## Suggested GitHub Issues
1. `feat(net): message protocol and length-prefixed framing` — `NetMessage`/`NetFraming` + tests, no sockets.
2. `feat(net): authoritative NetServer with player join/leave and world snapshot broadcast` — server accept loop + movement sync.
3. `feat(net): NetClient connect, input send, remote snapshot rendering` — client side of movement sync.
4. `feat(net): route NPC dialogue through the server with mood/speech broadcast` — shared NPC conversations.
5. `feat(menu): Host Game / Join Game UI with connection status` — Menu + main.cpp wiring.
6. `test(net): loopback client/server integration test with FakeBackend` — end-to-end without real Ollama.
7. `docs(multiplayer): hosting and joining guide` — README/developer_guide update.
8. *(Phase B, later)* `build(server): headless dedicated-server executable target`.
9. *(Phase B, later)* `chore(deploy): deploy-aws-server skill for free-tier EC2`.
