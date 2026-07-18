#include "InputMap.hpp"

#include "raylib.h"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace llm_npc {

namespace {

// Name <-> key pairs for every key the rebinding UI accepts. Letters, digits,
// and function keys are appended programmatically. Names match the SFML-era
// table exactly so saved keybindings.cfg files stay valid.
const std::vector<std::pair<std::string, int>>& table() {
    static const std::vector<std::pair<std::string, int>> entries = [] {
        std::vector<std::pair<std::string, int>> t = {
            {"Escape", KEY_ESCAPE},
            {"Space", KEY_SPACE},
            {"Enter", KEY_ENTER},
            {"Tab", KEY_TAB},
            {"Backspace", KEY_BACKSPACE},
            {"LShift", KEY_LEFT_SHIFT},
            {"RShift", KEY_RIGHT_SHIFT},
            {"LControl", KEY_LEFT_CONTROL},
            {"RControl", KEY_RIGHT_CONTROL},
            {"LAlt", KEY_LEFT_ALT},
            {"RAlt", KEY_RIGHT_ALT},
            {"Left", KEY_LEFT},
            {"Right", KEY_RIGHT},
            {"Up", KEY_UP},
            {"Down", KEY_DOWN},
            {"Home", KEY_HOME},
            {"End", KEY_END},
            {"PageUp", KEY_PAGE_UP},
            {"PageDown", KEY_PAGE_DOWN},
            {"Insert", KEY_INSERT},
            {"Delete", KEY_DELETE},
            {"Comma", KEY_COMMA},
            {"Period", KEY_PERIOD},
            {"Slash", KEY_SLASH},
            {"Semicolon", KEY_SEMICOLON},
            {"Quote", KEY_APOSTROPHE},
            {"LBracket", KEY_LEFT_BRACKET},
            {"RBracket", KEY_RIGHT_BRACKET},
            {"Hyphen", KEY_MINUS},
            {"Equal", KEY_EQUAL},
        };
        // raylib letter/digit key codes are their ASCII values.
        for (int i = 0; i < 26; ++i) {
            t.emplace_back(std::string(1, static_cast<char>('A' + i)), KEY_A + i);
        }
        for (int i = 0; i < 10; ++i) {
            t.emplace_back(std::string(1, static_cast<char>('0' + i)), KEY_ZERO + i);
        }
        for (int i = 0; i < 12; ++i) {
            t.emplace_back("F" + std::to_string(i + 1), KEY_F1 + i);
        }
        return t;
    }();
    return entries;
}

}  // namespace

int keyFromName(const std::string& name) {
    std::string canon = name;
    if (canon.size() == 1) canon[0] = static_cast<char>(std::toupper(canon[0]));
    for (const auto& [entryName, key] : table()) {
        if (entryName == canon) return key;
    }
    return KEY_NULL;
}

std::string keyNameOf(int key) {
    for (const auto& [entryName, entryKey] : table()) {
        if (entryKey == key) return entryName;
    }
    return {};
}

bool isActionPressed(const KeyBindings& bindings, Action action) {
    const int key = keyFromName(bindings.key(action));
    return key != KEY_NULL && IsKeyDown(key);
}

bool isActionJustPressed(const KeyBindings& bindings, Action action) {
    const int key = keyFromName(bindings.key(action));
    return key != KEY_NULL && IsKeyPressed(key);
}

}  // namespace llm_npc
