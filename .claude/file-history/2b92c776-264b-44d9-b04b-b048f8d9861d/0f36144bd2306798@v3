# LLM NPC City

A small first-person 3D city where every inhabitant is a live LLM character.
Walk the streets, find the baker, the cop, the busker, the hot-dog vendor —
press `T` and talk to them about anything. Each NPC has their own personality,
their own knowledge of the city (and pointed ignorance of everything else),
and remembers your conversation.

Powered locally by [Ollama](https://ollama.com) — no cloud, no API key.

## Quick start (Windows)

1. **Install Ollama** from <https://ollama.com/download>, then pull the model:

   ```
   ollama pull qwen2.5:3b-instruct
   ```

2. **Install MSYS2** from <https://www.msys2.org> and, in the *UCRT64* shell:

   ```
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-sfml
   ```

3. **Double-click `run.bat`** (or run `./run.ps1`). It checks Ollama is up,
   builds, and launches the game.

## Playing

- `WASD` to walk, mouse to look around.
- Walk up to anyone: `[T] Talk to <name>` appears — press `T`.
- Type, press `Enter`, and watch the reply stream in live. `Escape` to leave.
- `Escape` opens the pause menu: resume, quit, or **rebind every key with the
  mouse** (changes save instantly).

Full reference: [docs/CONTROLS.md](docs/CONTROLS.md).

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

- `config/llm.cfg` — model, host/port, temperature, `keep_alive` (latency).
  Any model you have pulled in Ollama works; smaller = snappier.
- `config/keybindings.cfg` — key bindings (also editable in-game).
- `personas/*.persona` — the characters themselves. Edit freely; the file
  format is documented in [docs/DEVELOPER.md](docs/DEVELOPER.md).

## Development

Architecture, headless test workflow, and the latency playbook live in
[docs/DEVELOPER.md](docs/DEVELOPER.md). Short version:

```sh
make -C tests test      # full offline unit-test suite, no SFML required
```
