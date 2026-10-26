# Controls

All keys (except the mouse and a few fixed UI keys) can be rebound in-game:
press `Escape` → **Controls**, click a key chip, then press the new key.
Bindings are saved to `config/keybindings.cfg` immediately and can also be
edited there by hand.

## Defaults

| Action          | Default key | Config name     |
|-----------------|-------------|-----------------|
| Move forward    | `W`         | `move_forward`  |
| Move backward   | `S`         | `move_backward` |
| Strafe left     | `A`         | `strafe_left`   |
| Strafe right    | `D`         | `strafe_right`  |
| Talk to NPC     | `T`         | `talk`          |
| Open menu       | `Escape`    | `menu`          |

Mouse: look around (the cursor is captured while walking).

## Fixed keys

These always work regardless of bindings:

- `Escape` — leaves a conversation, backs out of menu pages, cancels a
  pending key capture. It is the universal "get me out" key.
- `Enter` — sends your typed line during a conversation.
- `Backspace` — deletes while typing.

## Conversations

1. Walk up to an NPC until `[T] Talk to <name>` appears.
2. Press the talk key. The world pauses input and a chat panel opens.
3. Type and press `Enter`. The NPC's reply streams in word by word.
4. Press `Escape` to walk away (mid-reply is fine — the NPC still finishes
   the thought and remembers it next time you talk).

## Graphics toggles

The pause menu's main page has a **Noir / Rain** row just above Quit:

- **Noir: on/off** — the Neo-Noir night look. As the world clock moves
  into dusk (18:00-21:00) the city blends into rain-slick noir: warm lamp
  pools, wet reflective streets, a near-colorless grade where only red
  survives, vignette and film grain. Dawn (05:00-07:00) blends it back
  out. Daytime is never affected; off restores the original night.
- **Rain: on/off** — falling rain during the noir night.

Both are saved to `config/graphics.cfg` (`noir_night`, `rain`) as soon as
they're clicked and can be edited there by hand.

## Rebinding rules

- Binding a key that another action already uses **swaps** the two keys; a
  toast in the menu tells you what swapped.
- Keys without a stable name (exotic media keys, etc.) are rejected.
- A broken or missing `config/keybindings.cfg` falls back to the defaults
  above; duplicate assignments in a hand-edited file are resolved in favor of
  the action listed first in the table.
