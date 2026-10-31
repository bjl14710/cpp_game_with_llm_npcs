# LLM NPC City

A small first-person 3D city where every inhabitant is a live LLM character.
Walk the streets, find the baker, the cop, the busker, the hot-dog vendor —
press `T` and talk to them about anything. Each NPC has their own personality,
their own knowledge of the city (and pointed ignorance of everything else),
and remembers your conversation.

Runs locally on [Ollama](https://ollama.com) out of the box — no cloud, no API
key. It can *optionally* use cloud models (Claude, GPT, Gemini) through a single
[OpenRouter](https://openrouter.ai) key; see [Configuration](#configuration).
Where this is all heading — a build-your-own-characters product — is sketched in
[docs/VISION.md](docs/VISION.md).

## Quick start (Windows)

1. **Install Ollama** from <https://ollama.com/download>, then pull the model:

   ```
   ollama pull qwen2.5:3b-instruct
   ```

2. **Install MSYS2** from <https://www.msys2.org> and, in the *UCRT64* shell:

   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake
   ```

3. **Fetch the art packs** (once): `bash tools/fetch_assets.sh` — two CC0
   low-poly packs, pinned and checksummed (see `assets/LICENSES.md`).

4. **Double-click `run.bat`** (or run `./run.ps1`; on macOS, `run.command` or
   `./run.sh`). It starts Ollama if needed, pulls the model on first run,
   builds (CMake fetches raylib automatically), and launches the game.

## Playing

- `WASD` to walk, mouse to look around.
- Walk up to anyone: `[T] Talk to <name>` appears — press `T`.
- Type, press `Enter`, and watch the reply stream in live. `Escape` to leave.
- `Escape` opens the pause menu: resume, quit, or **rebind every key with the
  mouse** (changes save instantly).

Full reference: [docs/CONTROLS.md](docs/CONTROLS.md).

## Multiplayer (2-4 friends)

One player hosts; up to three more walk the same city and talk to the same
NPCs — one shared brain per character, so the baker remembers what your
friend told her.

**Hosting:** `Escape` → **Multiplayer** → **Host on port 40605**. Your machine
runs the world and the LLM; nobody else needs Ollama. Tell your friends your
IP address (`ipconfig` / `ifconfig`, e.g. `192.168.1.20`).

**Joining:** `Escape` → **Multiplayer** → click the address field, type
`<host-ip>:40605`, press `Enter`. Connection problems show up right there —
wrong address, full session (4 players max), or mismatched game versions.

What to expect in a session:

- Everyone sees everyone walk around, plus each NPC's live position and mood.
- NPC conversations are private to read, public in effect: if your friend
  angers Officer Brooks, you'll see her scowl.
- An NPC answers one person at a time; a second question waits its turn.
- Only the host's world persists — when the host quits, the session ends.

**Beyond your LAN:** the internet can't reach your living room uninvited.
Either forward TCP port 40605 on the host's router, or skip router config
entirely with [Tailscale](https://tailscale.com)/ZeroTier (both make a
private network where your friends use the host's Tailscale IP). The game
does not do NAT punching itself.

## Meet the city

Ten residents, each with their own patch of downtown:

| Who | Where |
|-----|-------|
| Marge Holloway, baker | Marge's Bakery, north-west |
| Officer Dana Brooks | outside the police station |
| Theo Park, barista | Bean There Coffee |
| Ms. Adaeze Obi, librarian | the library steps |
| Hal Jensen, hardware man | Jensen Hardware |
| Gus Romano, hot-dog vendor | the cart, dead center of the plaza |
| Ray Okafor, taxi driver | idling by his cab, east of the plaza |
| Yuki Tanaka, tourist | photographing the fountain |
| Benny "Strings" Malone, busker | the park's street corner |
| Mr. Albert Whitfield, retired teacher | the park bench |

They know each other — ask Gus about the squeaky cart wheel and someone at
the hardware store may have opinions.

## Configuration

- `config/llm.cfg` — provider, model, host/port, temperature, `keep_alive`
  (latency). Any model you have pulled in Ollama works; smaller = snappier. Pick
  a model in-game from the pause menu (Esc → Model) too.
- **Cloud models (optional).** Set `provider = openrouter` and a `model` like
  `anthropic/claude-haiku-4.5` in `config/llm.cfg`, then provide an OpenRouter
  key via the `OPENROUTER_API_KEY` environment variable or a `config/secrets.cfg`
  (copy `config/secrets.cfg.example`; it's gitignored). Cloud needs an
  OpenSSL-enabled build (CMake auto-detects OpenSSL; on macOS `brew install
  openssl@3`). The same code path also works against Ollama's OpenAI shim
  (`base_url = http://localhost:11434/v1`) with no key and no TLS.
- `config/keybindings.cfg` — key bindings (also editable in-game).
- `personas/*.persona` — the characters themselves. Edit freely; the file
  format is documented in [docs/DEVELOPER.md](docs/DEVELOPER.md).

## Development

Architecture, headless test workflow, and the latency playbook live in
[docs/DEVELOPER.md](docs/DEVELOPER.md). Short version:

```sh
make -C tests test      # full offline unit-test suite, no graphics libs required
```
