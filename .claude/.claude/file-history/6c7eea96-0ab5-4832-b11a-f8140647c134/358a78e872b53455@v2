#include "KeyBindings.hpp"

#include <cctype>
#include <fstream>

#include "Config.hpp"

namespace llm_npc {

namespace {

// Canonicalize a key name: single letters become uppercase so "w" and "W"
// refer to the same key; multi-character names ("Escape") pass through.
std::string canonical(std::string name) {
    if (name.size() == 1) name[0] = static_cast<char>(std::toupper(name[0]));
    return name;
}

constexpr std::array<const char*, kActionCount> kActionNames = {
    "move_forward", "move_backward", "strafe_left", "strafe_right", "talk", "menu",
};

constexpr std::array<const char*, kActionCount> kActionLabels = {
    "Move forward", "Move backward", "Strafe left", "Strafe right", "Talk to NPC", "Open menu",
};

}  // namespace

KeyBindings KeyBindings::defaults() {
    KeyBindings kb;
    kb.keys_ = {"W", "S", "A", "D", "T", "Escape"};
    return kb;
}

const char* KeyBindings::actionName(Action a) {
    return kActionNames[static_cast<std::size_t>(a)];
}

const char* KeyBindings::actionLabel(Action a) {
    return kActionLabels[static_cast<std::size_t>(a)];
}

const std::string& KeyBindings::key(Action a) const {
    return keys_[static_cast<std::size_t>(a)];
}

std::optional<Action> KeyBindings::actionForKey(const std::string& keyName) const {
    const std::string canon = canonical(keyName);
    if (canon.empty()) return std::nullopt;
    for (std::size_t i = 0; i < kActionCount; ++i) {
        if (keys_[i] == canon) return static_cast<Action>(i);
    }
    return std::nullopt;
}

std::optional<Action> KeyBindings::rebind(Action a, const std::string& keyName) {
    const std::string canon = canonical(keyName);
    const std::size_t idx = static_cast<std::size_t>(a);
    if (canon.empty() || keys_[idx] == canon) return std::nullopt;

    std::optional<Action> displaced = actionForKey(canon);
    if (displaced) {
        // Swap: the displaced action inherits this action's old key.
        keys_[static_cast<std::size_t>(*displaced)] = keys_[idx];
    }
    keys_[idx] = canon;
    return displaced;
}

bool KeyBindings::load(const std::filesystem::path& path) {
    *this = defaults();
    auto kv = readKv(path);
    if (kv.empty()) return false;

    const KeyBindings def = defaults();
    for (std::size_t i = 0; i < kActionCount; ++i) {
        if (auto it = kv.find(kActionNames[i]); it != kv.end()) {
            keys_[i] = canonical(it->second);
        }
    }

    // Deduplicate: earliest action in enum order keeps a contested key.
    for (std::size_t i = 0; i < kActionCount; ++i) {
        bool taken = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (!keys_[i].empty() && keys_[j] == keys_[i]) taken = true;
        }
        if (!taken) continue;
        // Fall back to this action's default unless that is taken too.
        std::string fallback = def.keys_[i];
        for (std::size_t j = 0; j < kActionCount; ++j) {
            if (j != i && keys_[j] == fallback) fallback.clear();
        }
        keys_[i] = fallback;
    }
    return true;
}

bool KeyBindings::save(const std::filesystem::path& path) const {
    std::ofstream out(path);
    if (!out) return false;
    out << "# Key bindings. Edit in-game via the menu, or by hand here.\n";
    for (std::size_t i = 0; i < kActionCount; ++i) {
        out << kActionNames[i] << " = " << keys_[i] << '\n';
    }
    return static_cast<bool>(out);
}

}  // namespace llm_npc
