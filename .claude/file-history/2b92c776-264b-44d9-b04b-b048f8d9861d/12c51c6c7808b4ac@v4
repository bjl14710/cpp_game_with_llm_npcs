#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "KeyBindings.hpp"

namespace llm_npc {

// What the main loop should do after the menu handled an event.
enum class MenuResult {
    None,    // keep showing the menu
    Resume,  // close the menu and return to the game
    Quit,    // close the window
};

// Mouse-driven pause menu: Resume / Controls / Multiplayer / Quit on the main
// page, a Controls page where clicking a key chip arms capture of the next
// key press, and a Multiplayer page for hosting or joining a session.
// Rebinds use KeyBindings swap semantics and are saved to disk immediately.
class Menu {
   public:
    // Callbacks the Multiplayer page drives, injected by main.cpp so the
    // menu stays free of networking types (same shape as the key-binding
    // injection). onHost/onJoin return "" on success or a human-readable
    // error shown as a toast.
    struct MultiplayerHooks {
        std::function<std::string(int port)> onHost;
        std::function<std::string(const std::string& address)> onJoin;
        std::function<void()> onLeave;
        std::function<std::string()> status;  // one-line session summary
        std::function<bool()> active;         // hosting or joined right now
    };

    // `bindings` is shared with the main loop; `savePath` is where every
    // accepted rebind is persisted.
    Menu(KeyBindings& bindings, std::filesystem::path savePath);

    // Installs the Multiplayer page's callbacks; without them the page
    // shows nothing actionable (solo-only build of the menu still works).
    void setMultiplayer(MultiplayerHooks hooks);

    // Resets to the main page (called when the menu is opened).
    void open();

    // True while the Controls page is waiting for a key press to bind.
    bool capturingKey() const { return awaiting_.has_value(); }

    // Routes one SFML event (mouse move/click, key press during capture).
    // Escape closes the menu from the main page, backs out of the Controls
    // page, or cancels an armed capture — in that priority order.
    MenuResult handleEvent(const sf::Event& event, const sf::RenderWindow& window);

    // Ages the transient toast message.
    void update(float dt);

    // Draws the menu; call between pushGLStates/popGLStates.
    void render(sf::RenderWindow& window, const sf::Font& font) const;

   private:
    enum class Page { Main, Controls, Multiplayer };

    // A clickable rectangle paired with what clicking it means.
    struct Hit {
        sf::FloatRect rect;
        int id = 0;  // page-specific meaning, see layout helpers
    };

    KeyBindings& bindings_;
    std::filesystem::path savePath_;
    Page page_ = Page::Main;
    std::optional<Action> awaiting_;
    sf::Vector2f mouse_{};
    std::string toast_;
    float toastLeft_ = 0.f;

    MultiplayerHooks multiplayer_;
    std::string joinAddress_ = "127.0.0.1:40605";
    bool editingAddress_ = false;  // typed characters go into joinAddress_

    // Clickable areas for the current page, derived from the window size so
    // render() and handleEvent() always agree.
    std::vector<Hit> layout(const sf::RenderWindow& window) const;

    // Applies a captured key press to the armed action; announces swaps.
    void applyCapture(sf::Keyboard::Key key);

    // Validates joinAddress_ and fires onJoin; toasts the outcome.
    void attemptJoin();

    // Shows a short status message under the controls list.
    void showToast(std::string text);
};

}  // namespace llm_npc
