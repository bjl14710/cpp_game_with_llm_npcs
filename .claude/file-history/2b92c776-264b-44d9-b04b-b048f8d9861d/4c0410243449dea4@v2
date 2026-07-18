#pragma once

#include <string>

#include "KeyBindings.hpp"

namespace llm_npc {

// Translates a portable key name from KeyBindings ("W", "Escape", "Space",
// "F5", ...) into a raylib KeyboardKey code (as int, so this header stays
// raylib-free for includers that don't draw); 0 (KEY_NULL) for names we do
// not recognize or empty (unbound) names. The name set is unchanged from the
// SFML era, so existing keybindings.cfg files keep working.
int keyFromName(const std::string& name);

// Reverse translation for the rebinding UI: the portable name of a raylib
// key code, or an empty string when the key has no stable name (the menu
// should reject such keys).
std::string keyNameOf(int key);

// True while the key currently bound to `action` is held down. Unbound or
// unrecognized keys are never pressed.
bool isActionPressed(const KeyBindings& bindings, Action action);

// True on the frame the key bound to `action` was pressed.
bool isActionJustPressed(const KeyBindings& bindings, Action action);

}  // namespace llm_npc
