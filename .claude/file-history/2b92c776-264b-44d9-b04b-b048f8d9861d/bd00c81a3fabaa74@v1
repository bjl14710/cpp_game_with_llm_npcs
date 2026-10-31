#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace llm_npc {

// Every rebindable game action. Count is a sentinel, not an action.
enum class Action {
    MoveForward,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    Talk,
    OpenMenu,
    Count,
};

constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

// Maps actions to keys by portable key *names* ("W", "Escape", "Space") so
// the core stays SFML-free; the app layer translates names to sf::Keyboard
// codes. An empty name means the action is unbound.
class KeyBindings {
   public:
    // W/S/A/D movement, T to talk, Escape for the menu.
    static KeyBindings defaults();

    // Stable config-file key for an action, e.g. "move_forward".
    static const char* actionName(Action a);

    // Human-readable label for menus, e.g. "Move forward".
    static const char* actionLabel(Action a);

    // The key name currently bound to `a`.
    const std::string& key(Action a) const;

    // The action bound to `keyName`, if any.
    std::optional<Action> actionForKey(const std::string& keyName) const;

    // Bind `keyName` to `a`. If another action already used that key, the two
    // actions swap keys and the displaced action is returned so the UI can
    // announce the swap. Binding an action to its current key is a no-op.
    std::optional<Action> rebind(Action a, const std::string& keyName);

    // Load bindings from a key=value file, starting from defaults for any
    // missing entry. Duplicate keys are resolved deterministically: the
    // action earliest in enum order keeps the key; later ones fall back to
    // their default, or become unbound if that default is also taken.
    // Returns false when the file could not be read (defaults remain).
    bool load(const std::filesystem::path& path);

    // Persist all bindings as a key=value file. Returns false on I/O error.
    bool save(const std::filesystem::path& path) const;

   private:
    std::array<std::string, kActionCount> keys_;
};

}  // namespace llm_npc
