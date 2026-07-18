#pragma once

#include <SFML/Graphics.hpp>

#include <filesystem>
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

// Mouse-driven pause menu: Resume / Controls / Quit on the main page, and a
// Controls page where clicking a key chip arms capture of the next key press.
// Rebinds use KeyBindings swap semantics and are saved to disk immediately.
class Menu {
   public:
    // `bindings` is shared with the main loop; `savePath` is where every
    // accepted rebind is persisted.
    Menu(KeyBindings& bindings, std::filesystem::path savePath);

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
    enum class Page { Main, Controls };

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

    // Clickable areas for the current page, derived from the window size so
    // render() and handleEvent() always agree.
    std::vector<Hit> layout(const sf::RenderWindow& window) const;

    // Applies a captured key press to the armed action; announces swaps.
    void applyCapture(sf::Keyboard::Key key);

    // Shows a short status message under the controls list.
    void showToast(std::string text);
};

}  // namespace llm_npc
