#include "Menu.hpp"

#include <utility>

#include "InputMap.hpp"

namespace llm_npc {

namespace {

// Ids used by layout()/handleEvent() to identify what was clicked.
constexpr int kIdResume = 0;
constexpr int kIdControls = 1;
constexpr int kIdQuit = 2;
constexpr int kIdBack = 100;
// Controls-page chips use ids [kIdChipBase, kIdChipBase + kActionCount).
constexpr int kIdChipBase = 200;

constexpr float kToastSeconds = 3.f;

// Builds a ready-to-draw text object anchored at `pos`.
sf::Text makeText(const sf::Font& font, const std::string& str, unsigned size,
                  sf::Vector2f pos, sf::Color color) {
    sf::Text text(str, font, size);
    text.setPosition(pos);
    text.setFillColor(color);
    return text;
}

// Draws `str` horizontally centered inside `rect`.
void drawCentered(sf::RenderWindow& window, const sf::Font& font, const std::string& str,
                  unsigned size, const sf::FloatRect& rect, sf::Color color) {
    sf::Text text(str, font, size);
    const sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition(rect.left + (rect.width - bounds.width) * 0.5f - bounds.left,
                     rect.top + (rect.height - bounds.height) * 0.5f - bounds.top);
    text.setFillColor(color);
    window.draw(text);
}

}  // namespace

Menu::Menu(KeyBindings& bindings, std::filesystem::path savePath)
    : bindings_(bindings), savePath_(std::move(savePath)) {}

void Menu::open() {
    page_ = Page::Main;
    awaiting_.reset();
    toast_.clear();
    toastLeft_ = 0.f;
}

std::vector<Menu::Hit> Menu::layout(const sf::RenderWindow& window) const {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    std::vector<Hit> hits;

    if (page_ == Page::Main) {
        const sf::Vector2f size(320.f, 52.f);
        const float x = (w - size.x) * 0.5f;
        float y = h * 0.5f - 110.f;
        for (int id : {kIdResume, kIdControls, kIdQuit}) {
            hits.push_back({sf::FloatRect(x, y, size.x, size.y), id});
            y += 72.f;
        }
    } else {
        float y = h * 0.24f;
        for (std::size_t i = 0; i < kActionCount; ++i) {
            hits.push_back({sf::FloatRect(w * 0.5f + 90.f, y, 180.f, 40.f),
                            kIdChipBase + static_cast<int>(i)});
            y += 52.f;
        }
        hits.push_back({sf::FloatRect((w - 320.f) * 0.5f, y + 28.f, 320.f, 52.f), kIdBack});
    }
    return hits;
}

MenuResult Menu::handleEvent(const sf::Event& event, const sf::RenderWindow& window) {
    if (event.type == sf::Event::MouseMoved) {
        mouse_ = {static_cast<float>(event.mouseMove.x), static_cast<float>(event.mouseMove.y)};
        return MenuResult::None;
    }

    if (event.type == sf::Event::KeyPressed) {
        if (awaiting_) {
            if (event.key.code == sf::Keyboard::Escape) {
                awaiting_.reset();
                showToast("Rebind cancelled");
            } else {
                applyCapture(event.key.code);
            }
            return MenuResult::None;
        }
        if (event.key.code == sf::Keyboard::Escape) {
            if (page_ == Page::Controls) {
                page_ = Page::Main;
                return MenuResult::None;
            }
            return MenuResult::Resume;
        }
        return MenuResult::None;
    }

    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        const sf::Vector2f click(static_cast<float>(event.mouseButton.x),
                                 static_cast<float>(event.mouseButton.y));
        for (const Hit& hit : layout(window)) {
            if (!hit.rect.contains(click)) continue;
            if (hit.id == kIdResume) return MenuResult::Resume;
            if (hit.id == kIdQuit) return MenuResult::Quit;
            if (hit.id == kIdControls) {
                page_ = Page::Controls;
                return MenuResult::None;
            }
            if (hit.id == kIdBack) {
                awaiting_.reset();
                page_ = Page::Main;
                return MenuResult::None;
            }
            if (hit.id >= kIdChipBase && hit.id < kIdChipBase + static_cast<int>(kActionCount)) {
                awaiting_ = static_cast<Action>(hit.id - kIdChipBase);
                toast_.clear();
                return MenuResult::None;
            }
        }
        // Clicking empty space cancels an armed capture.
        awaiting_.reset();
        return MenuResult::None;
    }

    return MenuResult::None;
}

void Menu::applyCapture(sf::Keyboard::Key key) {
    const std::string name = keyNameOf(key);
    if (name.empty()) {
        showToast("That key can't be bound");
        return;
    }
    const Action action = *awaiting_;
    awaiting_.reset();
    const std::optional<Action> displaced = bindings_.rebind(action, name);
    if (displaced) {
        showToast(std::string("Swapped keys with \"") + KeyBindings::actionLabel(*displaced) + "\"");
    } else {
        showToast(std::string(KeyBindings::actionLabel(action)) + " is now " + name);
    }
    if (!bindings_.save(savePath_)) {
        showToast("Warning: could not save key bindings");
    }
}

void Menu::showToast(std::string text) {
    toast_ = std::move(text);
    toastLeft_ = kToastSeconds;
}

void Menu::update(float dt) {
    if (toastLeft_ > 0.f) {
        toastLeft_ -= dt;
        if (toastLeft_ <= 0.f) toast_.clear();
    }
}

void Menu::render(sf::RenderWindow& window, const sf::Font& font) const {
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);

    sf::RectangleShape dim(sf::Vector2f(w, h));
    dim.setFillColor(sf::Color(8, 10, 16, 170));
    window.draw(dim);

    const char* title = page_ == Page::Main ? "Paused" : "Controls";
    sf::Text titleText = makeText(font, title, 40, {0.f, h * 0.12f}, sf::Color(235, 240, 250));
    titleText.setPosition((w - titleText.getLocalBounds().width) * 0.5f, h * 0.12f);
    window.draw(titleText);

    for (const Hit& hit : layout(window)) {
        const bool hover = hit.rect.contains(mouse_);
        const bool isChip = hit.id >= kIdChipBase && hit.id < kIdChipBase + static_cast<int>(kActionCount);

        sf::RectangleShape box(sf::Vector2f(hit.rect.width, hit.rect.height));
        box.setPosition(hit.rect.left, hit.rect.top);
        box.setFillColor(hover ? sf::Color(60, 90, 140, 230) : sf::Color(30, 40, 60, 215));
        box.setOutlineThickness(2.f);
        box.setOutlineColor(sf::Color(110, 160, 220, 200));
        window.draw(box);

        std::string label;
        if (hit.id == kIdResume) label = "Resume";
        else if (hit.id == kIdControls) label = "Controls";
        else if (hit.id == kIdQuit) label = "Quit";
        else if (hit.id == kIdBack) label = "Back";
        else if (isChip) {
            const Action action = static_cast<Action>(hit.id - kIdChipBase);
            const bool armed = awaiting_ && *awaiting_ == action;
            label = armed ? "press a key..." : bindings_.key(action);
            if (label.empty()) label = "(unbound)";

            const sf::Text rowLabel = makeText(font, KeyBindings::actionLabel(action), 22,
                                               {w * 0.5f - 280.f, hit.rect.top + 6.f},
                                               sf::Color(225, 230, 240));
            window.draw(rowLabel);
        }
        drawCentered(window, font, label, isChip ? 20 : 24, hit.rect, sf::Color::White);
    }

    if (page_ == Page::Controls) {
        const sf::Text hint = makeText(font, "Click a key, then press the new key. Esc cancels.", 18,
                                       {0.f, 0.f}, sf::Color(170, 180, 200));
        sf::Text centered = hint;
        centered.setPosition((w - hint.getLocalBounds().width) * 0.5f, h * 0.24f - 40.f);
        window.draw(centered);
    }

    if (!toast_.empty()) {
        sf::Text toast = makeText(font, toast_, 20, {0.f, 0.f}, sf::Color(255, 225, 130));
        toast.setPosition((w - toast.getLocalBounds().width) * 0.5f, h * 0.88f);
        window.draw(toast);
    }
}

}  // namespace llm_npc
