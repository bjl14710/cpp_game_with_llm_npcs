#include "InputMap.hpp"

#include <array>
#include <cctype>
#include <utility>
#include <vector>

namespace llm_npc {

namespace {

// Name <-> key pairs for every key the rebinding UI accepts. Letters and
// digits are appended programmatically in buildTable().
const std::vector<std::pair<std::string, sf::Keyboard::Key>>& table() {
    static const std::vector<std::pair<std::string, sf::Keyboard::Key>> entries = [] {
        std::vector<std::pair<std::string, sf::Keyboard::Key>> t = {
            {"Escape", sf::Keyboard::Escape},
            {"Space", sf::Keyboard::Space},
            {"Enter", sf::Keyboard::Enter},
            {"Tab", sf::Keyboard::Tab},
            {"Backspace", sf::Keyboard::Backspace},
            {"LShift", sf::Keyboard::LShift},
            {"RShift", sf::Keyboard::RShift},
            {"LControl", sf::Keyboard::LControl},
            {"RControl", sf::Keyboard::RControl},
            {"LAlt", sf::Keyboard::LAlt},
            {"RAlt", sf::Keyboard::RAlt},
            {"Left", sf::Keyboard::Left},
            {"Right", sf::Keyboard::Right},
            {"Up", sf::Keyboard::Up},
            {"Down", sf::Keyboard::Down},
            {"Home", sf::Keyboard::Home},
            {"End", sf::Keyboard::End},
            {"PageUp", sf::Keyboard::PageUp},
            {"PageDown", sf::Keyboard::PageDown},
            {"Insert", sf::Keyboard::Insert},
            {"Delete", sf::Keyboard::Delete},
            {"Comma", sf::Keyboard::Comma},
            {"Period", sf::Keyboard::Period},
            {"Slash", sf::Keyboard::Slash},
            {"Semicolon", sf::Keyboard::Semicolon},
            {"Quote", sf::Keyboard::Quote},
            {"LBracket", sf::Keyboard::LBracket},
            {"RBracket", sf::Keyboard::RBracket},
            {"Hyphen", sf::Keyboard::Hyphen},
            {"Equal", sf::Keyboard::Equal},
        };
        for (int i = 0; i < 26; ++i) {
            t.emplace_back(std::string(1, static_cast<char>('A' + i)),
                           static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::A) + i));
        }
        for (int i = 0; i < 10; ++i) {
            t.emplace_back(std::string(1, static_cast<char>('0' + i)),
                           static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::Num0) + i));
        }
        for (int i = 0; i < 12; ++i) {
            t.emplace_back("F" + std::to_string(i + 1),
                           static_cast<sf::Keyboard::Key>(static_cast<int>(sf::Keyboard::F1) + i));
        }
        return t;
    }();
    return entries;
}

}  // namespace

sf::Keyboard::Key keyFromName(const std::string& name) {
    std::string canon = name;
    if (canon.size() == 1) canon[0] = static_cast<char>(std::toupper(canon[0]));
    for (const auto& [entryName, key] : table()) {
        if (entryName == canon) return key;
    }
    return sf::Keyboard::Unknown;
}

std::string keyNameOf(sf::Keyboard::Key key) {
    for (const auto& [entryName, entryKey] : table()) {
        if (entryKey == key) return entryName;
    }
    return {};
}

bool isActionPressed(const KeyBindings& bindings, Action action) {
    const sf::Keyboard::Key key = keyFromName(bindings.key(action));
    return key != sf::Keyboard::Unknown && sf::Keyboard::isKeyPressed(key);
}

}  // namespace llm_npc
