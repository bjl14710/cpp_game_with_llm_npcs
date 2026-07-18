#pragma once

#include <SFML/Window/Keyboard.hpp>

#include <string>

#include "KeyBindings.hpp"

namespace llm_npc {

// Translates a portable key name from KeyBindings ("W", "Escape", "Space",
// "F5", ...) into an SFML key code; sf::Keyboard::Unknown for names we do not
// recognize or empty (unbound) names.
sf::Keyboard::Key keyFromName(const std::string& name);

// Reverse translation for the rebinding UI: the portable name of an SFML key
// code, or an empty string when the key has no stable name (the menu should
// reject such keys).
std::string keyNameOf(sf::Keyboard::Key key);

// True while the key currently bound to `action` is held down. Unbound or
// unrecognized keys are never pressed.
bool isActionPressed(const KeyBindings& bindings, Action action);

}  // namespace llm_npc
