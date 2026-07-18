#include "Menu.hpp"

#include <utility>

#include "InputMap.hpp"

namespace llm_npc {

namespace {

// Ids used by layout()/update() to identify what was clicked.
constexpr int kIdResume = 0;
constexpr int kIdControls = 1;
constexpr int kIdQuit = 2;
constexpr int kIdMultiplayer = 3;
constexpr int kIdBack = 100;
// Controls-page chips use ids [kIdChipBase, kIdChipBase + kActionCount).
constexpr int kIdChipBase = 200;
// Multiplayer-page widgets.
constexpr int kIdHost = 300;
constexpr int kIdAddress = 301;
constexpr int kIdJoin = 302;
constexpr int kIdLeave = 303;

constexpr float kToastSeconds = 3.f;

// Default TCP port suggested for hosting; also the port pre-filled in the
// join address. Arbitrary high port outside common registered ranges.
constexpr int kDefaultHostPort = 40605;

// Splits "ip:port" into its parts; false when the port is missing/garbage.
bool splitAddress(const std::string& address, std::string& host, int& port) {
    const auto colon = address.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= address.size()) return false;
    host = address.substr(0, colon);
    try {
        port = std::stoi(address.substr(colon + 1));
    } catch (...) {
        return false;
    }
    return port > 0 && port <= 65535;
}

// Draws `str` horizontally centered inside `rect`.
void drawCentered(const std::string& str, int size, const Rectangle& rect, Color color) {
    const int width = MeasureText(str.c_str(), size);
    DrawText(str.c_str(),
             static_cast<int>(rect.x + (rect.width - static_cast<float>(width)) / 2.f),
             static_cast<int>(rect.y + (rect.height - static_cast<float>(size)) / 2.f),
             size, color);
}

// Draws `str` centered on the whole screen width at height `y`.
void drawCenteredLine(const std::string& str, int size, float y, Color color) {
    const int width = MeasureText(str.c_str(), size);
    DrawText(str.c_str(), (GetScreenWidth() - width) / 2, static_cast<int>(y), size, color);
}

}  // namespace

Menu::Menu(KeyBindings& bindings, std::filesystem::path savePath)
    : bindings_(bindings), savePath_(std::move(savePath)) {}

void Menu::setMultiplayer(MultiplayerHooks hooks) { multiplayer_ = std::move(hooks); }

void Menu::open() {
    page_ = Page::Main;
    awaiting_.reset();
    editingAddress_ = false;
    toast_.clear();
    toastLeft_ = 0.f;
}

std::vector<Menu::Hit> Menu::layout() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    std::vector<Hit> hits;

    if (page_ == Page::Main) {
        const float x = (w - 320.f) * 0.5f;
        float y = h * 0.5f - 146.f;
        for (int id : {kIdResume, kIdControls, kIdMultiplayer, kIdQuit}) {
            hits.push_back({Rectangle{x, y, 320.f, 52.f}, id});
            y += 72.f;
        }
    } else if (page_ == Page::Multiplayer) {
        const float x = (w - 420.f) * 0.5f;
        float y = h * 0.30f;
        if (multiplayer_.active && multiplayer_.active()) {
            hits.push_back({Rectangle{x, y, 420.f, 52.f}, kIdLeave});
            y += 72.f;
        } else {
            hits.push_back({Rectangle{x, y, 420.f, 52.f}, kIdHost});
            y += 88.f;
            hits.push_back({Rectangle{x, y, 420.f, 44.f}, kIdAddress});
            y += 60.f;
            hits.push_back({Rectangle{x, y, 420.f, 52.f}, kIdJoin});
            y += 72.f;
        }
        hits.push_back({Rectangle{(w - 320.f) * 0.5f, y + 24.f, 320.f, 52.f}, kIdBack});
    } else {
        float y = h * 0.24f;
        for (std::size_t i = 0; i < kActionCount; ++i) {
            hits.push_back({Rectangle{w * 0.5f + 90.f, y, 180.f, 40.f},
                            kIdChipBase + static_cast<int>(i)});
            y += 52.f;
        }
        hits.push_back({Rectangle{(w - 320.f) * 0.5f, y + 28.f, 320.f, 52.f}, kIdBack});
    }
    return hits;
}

MenuResult Menu::update(float dt) {
    if (toastLeft_ > 0.f) {
        toastLeft_ -= dt;
        if (toastLeft_ <= 0.f) toast_.clear();
    }

    // Armed key capture eats the keyboard until a key lands or Escape.
    if (awaiting_) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            awaiting_.reset();
            showToast("Rebind cancelled");
            return MenuResult::None;
        }
        const int key = GetKeyPressed();
        if (key != 0) applyCapture(key);
        return MenuResult::None;
    }

    // Address editing: typed characters go into the field.
    if (editingAddress_) {
        int ch = GetCharPressed();
        while (ch != 0) {
            if (ch >= 32 && ch < 127 && joinAddress_.size() < 64) {
                joinAddress_.push_back(static_cast<char>(ch));
            }
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
            !joinAddress_.empty()) {
            joinAddress_.pop_back();
        }
        if (IsKeyPressed(KEY_ESCAPE)) editingAddress_ = false;
        if (IsKeyPressed(KEY_ENTER)) {
            editingAddress_ = false;
            attemptJoin();
        }
        // Clicking empty space also drops focus (fall through to clicks).
    }

    if (!editingAddress_ && IsKeyPressed(KEY_ESCAPE)) {
        if (page_ != Page::Main) {
            page_ = Page::Main;
            return MenuResult::None;
        }
        return MenuResult::Resume;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 click = GetMousePosition();
        bool hitSomething = false;
        for (const Hit& hit : layout()) {
            if (!CheckCollisionPointRec(click, hit.rect)) continue;
            hitSomething = true;
            if (hit.id == kIdResume) return MenuResult::Resume;
            if (hit.id == kIdQuit) return MenuResult::Quit;
            if (hit.id == kIdControls) {
                page_ = Page::Controls;
            } else if (hit.id == kIdMultiplayer) {
                page_ = Page::Multiplayer;
                editingAddress_ = false;
            } else if (hit.id == kIdBack) {
                awaiting_.reset();
                editingAddress_ = false;
                page_ = Page::Main;
            } else if (hit.id == kIdHost) {
                if (multiplayer_.onHost) {
                    const std::string error = multiplayer_.onHost(kDefaultHostPort);
                    showToast(error.empty() ? "Hosting started" : error);
                }
            } else if (hit.id == kIdAddress) {
                editingAddress_ = true;
            } else if (hit.id == kIdJoin) {
                editingAddress_ = false;
                attemptJoin();
            } else if (hit.id == kIdLeave) {
                if (multiplayer_.onLeave) multiplayer_.onLeave();
                showToast("Left the session");
            } else if (hit.id >= kIdChipBase &&
                       hit.id < kIdChipBase + static_cast<int>(kActionCount)) {
                awaiting_ = static_cast<Action>(hit.id - kIdChipBase);
                toast_.clear();
            }
            break;
        }
        // Clicking empty space cancels an armed capture or address edit.
        if (!hitSomething) {
            awaiting_.reset();
            editingAddress_ = false;
        }
    }

    return MenuResult::None;
}

void Menu::applyCapture(int key) {
    const std::string name = keyNameOf(key);
    if (name.empty()) {
        showToast("That key can't be bound");
        return;
    }
    const Action action = *awaiting_;
    awaiting_.reset();
    const std::optional<Action> displaced = bindings_.rebind(action, name);
    if (displaced) {
        showToast(std::string("Swapped keys with \"") + KeyBindings::actionLabel(*displaced) +
                  "\"");
    } else {
        showToast(std::string(KeyBindings::actionLabel(action)) + " is now " + name);
    }
    if (!bindings_.save(savePath_)) {
        showToast("Warning: could not save key bindings");
    }
}

void Menu::attemptJoin() {
    if (!multiplayer_.onJoin) return;
    std::string host;
    int port = 0;
    if (!splitAddress(joinAddress_, host, port)) {
        showToast("Address must look like 192.168.1.20:40605");
        return;
    }
    const std::string error = multiplayer_.onJoin(joinAddress_);
    showToast(error.empty() ? "Joined!" : error);
}

void Menu::showToast(std::string text) {
    toast_ = std::move(text);
    toastLeft_ = kToastSeconds;
}

void Menu::render() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());

    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{8, 10, 16, 170});

    const char* title = page_ == Page::Main       ? "Paused"
                        : page_ == Page::Controls ? "Controls"
                                                  : "Multiplayer";
    drawCenteredLine(title, 40, h * 0.12f, Color{235, 240, 250, 255});

    if (page_ == Page::Multiplayer) {
        const std::string status = multiplayer_.status ? multiplayer_.status() : "";
        if (!status.empty()) {
            drawCenteredLine(status, 20, h * 0.22f, Color{170, 200, 230, 255});
        }
    }
    if (page_ == Page::Controls) {
        drawCenteredLine("Click a key, then press the new key. Esc cancels.", 18,
                         h * 0.24f - 40.f, Color{170, 180, 200, 255});
    }

    const Vector2 mouse = GetMousePosition();
    for (const Hit& hit : layout()) {
        const bool hover = CheckCollisionPointRec(mouse, hit.rect);
        const bool isChip =
            hit.id >= kIdChipBase && hit.id < kIdChipBase + static_cast<int>(kActionCount);

        DrawRectangleRec(hit.rect, hover ? Color{60, 90, 140, 230} : Color{30, 40, 60, 215});
        DrawRectangleLinesEx(hit.rect, 2.f, Color{110, 160, 220, 200});

        std::string label;
        if (hit.id == kIdResume) label = "Resume";
        else if (hit.id == kIdControls) label = "Controls";
        else if (hit.id == kIdMultiplayer) label = "Multiplayer";
        else if (hit.id == kIdQuit) label = "Quit";
        else if (hit.id == kIdBack) label = "Back";
        else if (hit.id == kIdHost) label = "Host on port " + std::to_string(kDefaultHostPort);
        else if (hit.id == kIdJoin) label = "Join";
        else if (hit.id == kIdLeave) label = "Leave session";
        else if (hit.id == kIdAddress) {
            label = joinAddress_ + (editingAddress_ ? "_" : "");
            if (label.empty()) label = "(click to type ip:port)";
        } else if (isChip) {
            const Action action = static_cast<Action>(hit.id - kIdChipBase);
            const bool armed = awaiting_ && *awaiting_ == action;
            label = armed ? "press a key..." : bindings_.key(action);
            if (label.empty()) label = "(unbound)";

            DrawText(KeyBindings::actionLabel(action), static_cast<int>(w * 0.5f - 280.f),
                     static_cast<int>(hit.rect.y + 8.f), 22, Color{225, 230, 240, 255});
        }
        drawCentered(label, isChip ? 20 : 24, hit.rect, WHITE);

        if (hit.id == kIdAddress) {
            DrawText("Join address (ip:port) - click, type, Enter:",
                     static_cast<int>(hit.rect.x), static_cast<int>(hit.rect.y - 24.f), 16,
                     Color{170, 180, 200, 255});
        }
    }

    if (!toast_.empty()) {
        drawCenteredLine(toast_, 20, h * 0.88f, Color{255, 225, 130, 255});
    }
}

}  // namespace llm_npc
