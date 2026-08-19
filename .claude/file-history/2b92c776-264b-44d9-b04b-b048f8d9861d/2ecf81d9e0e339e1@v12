#include "Menu.hpp"

#include <algorithm>

#include <utility>

#include "Config.hpp"  // trim
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
// Creator-page widgets. Field ids map to editingField_ values 1-3; the
// part cyclers encode category and direction in their id.
constexpr int kIdCreator = 4;
constexpr int kIdJournal = 5;
constexpr int kIdAvatar = 6;
constexpr int kIdSandbox = 7;
// Sandbox page: New button + one row per saved map (capped by layout).
constexpr int kIdMapNew = 600;
constexpr int kIdGenField = 601;
constexpr int kIdGenGo = 602;
constexpr int kIdMapBase = 610;
constexpr int kMaxMapRows = 7;
// Journal-page widgets.
constexpr int kIdJournalPrev = 500;
constexpr int kIdJournalNext = 501;
constexpr int kJournalRowsPerPage = 7;
constexpr int kIdFieldBase = 400;      // +1 name, +2 backstory, +3 traits
constexpr int kIdPartPrev = 410;       // +category
constexpr int kIdPartNext = 420;       // +category
constexpr int kIdPalette = 430;
constexpr int kIdRandomize = 431;
constexpr int kIdCreate = 432;
constexpr int kIdTrait = 433;

// Display name for a catalog part id: the text after the category prefix
// ("hair_bowl" → "bowl"), title-ish enough for a picker chip.
std::string partLabel(const std::string& id) {
    const auto underscore = id.find('_');
    return underscore == std::string::npos ? id : id.substr(underscore + 1);
}

const char* categoryLabel(PartCategory category) {
    switch (category) {
        case PartCategory::Body: return "Body";
        case PartCategory::Head: return "Head";
        case PartCategory::Eyes: return "Eyes";
        case PartCategory::Hair: return "Hair";
        case PartCategory::Mouth: return "Mouth";
    }
    return "?";
}

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

void Menu::setCreator(CreatorHooks hooks) { creator_ = std::move(hooks); }

void Menu::setAvatar(AvatarHooks hooks) { avatar_ = std::move(hooks); }

void Menu::setSandbox(SandboxHooks hooks) { sandbox_ = std::move(hooks); }

void Menu::setTraitChoices(std::vector<std::string> ids) {
    traitChoices_ = std::move(ids);
    creatorTraitIndex_ = 0;
}

void Menu::setJournal(JournalHooks hooks) { journal_ = std::move(hooks); }

void Menu::open() {
    page_ = Page::Main;
    awaiting_.reset();
    editingAddress_ = false;
    editingField_ = 0;  // draft look and typed fields survive a reopen
    toast_.clear();
    toastLeft_ = 0.f;
}

std::vector<Menu::Hit> Menu::layout() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    std::vector<Hit> hits;

    if (page_ == Page::Main) {
        const float x = (w - 320.f) * 0.5f;
        float y = h * 0.5f - 290.f;
        for (int id : {kIdResume, kIdControls, kIdMultiplayer, kIdCreator,
                       kIdAvatar, kIdJournal, kIdSandbox, kIdQuit}) {
            hits.push_back({Rectangle{x, y, 320.f, 52.f}, id});
            y += 72.f;
        }
    } else if (page_ == Page::Journal) {
        // Rows draw in render(); only paging + back are clickable.
        const float y = h * 0.86f;
        hits.push_back({Rectangle{w * 0.20f, y, 120.f, 44.f}, kIdJournalPrev});
        hits.push_back({Rectangle{w * 0.80f - 120.f, y, 120.f, 44.f}, kIdJournalNext});
        hits.push_back({Rectangle{(w - 320.f) * 0.5f, y, 320.f, 44.f}, kIdBack});
    } else if (page_ == Page::Creator) {
        // Left column: persona text fields (label drawn above each box).
        const float fieldX = w * 0.07f;
        float y = h * 0.26f;
        if (!avatarMode_) {  // the avatar has no persona; look-only editing
            for (int f = 1; f <= 3; ++f) {
                hits.push_back({Rectangle{fieldX, y, w * 0.34f, 44.f}, kIdFieldBase + f});
                y += 92.f;
            }
        }
        // Right column: a [<] label [>] row per category, then palette,
        // then the action buttons. The preview renders in-world between
        // the columns (main.cpp draws it while this page is open).
        const float rowX = w * 0.62f;
        const float rowW = w * 0.31f;
        y = h * 0.26f;
        for (int c = 0; c < kPartCategoryCount; ++c) {
            hits.push_back({Rectangle{rowX, y, 44.f, 40.f}, kIdPartPrev + c});
            hits.push_back({Rectangle{rowX + rowW - 44.f, y, 44.f, 40.f}, kIdPartNext + c});
            y += 52.f;
        }
        hits.push_back({Rectangle{rowX, y, rowW, 40.f}, kIdPalette});
        y += 52.f;
        if (!avatarMode_ && !traitChoices_.empty()) {
            // Structured personality (issue #117) — avatars have none.
            hits.push_back({Rectangle{rowX, y, rowW, 40.f}, kIdTrait});
        }
        y += 56.f;
        hits.push_back({Rectangle{rowX, y, rowW * 0.47f, 48.f}, kIdRandomize});
        hits.push_back(
            {Rectangle{rowX + rowW * 0.53f, y, rowW * 0.47f, 48.f}, kIdCreate});
        hits.push_back({Rectangle{(w - 320.f) * 0.5f, h * 0.86f, 320.f, 48.f}, kIdBack});
    } else if (page_ == Page::Sandbox) {
        const float x = (w - 420.f) * 0.5f;
        float y = h * 0.18f;
        hits.push_back({Rectangle{x, y, 420.f, 48.f}, kIdMapNew});
        y += 60.f;
        // Describe-a-map row (issue #129): text field + Generate button.
        hits.push_back({Rectangle{x, y, 300.f, 44.f}, kIdGenField});
        hits.push_back({Rectangle{x + 310.f, y, 110.f, 44.f}, kIdGenGo});
        y += 64.f;
        const int rows = std::min<int>(kMaxMapRows, static_cast<int>(sandboxMaps_.size()));
        for (int i = 0; i < rows; ++i) {
            hits.push_back({Rectangle{x, y, 420.f, 44.f}, kIdMapBase + i});
            y += 56.f;
        }
        hits.push_back({Rectangle{(w - 320.f) * 0.5f, h * 0.86f, 320.f, 44.f}, kIdBack});
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

    // Creator field editing: typed characters go into the focused field.
    if (editingField_ != 0) {
        std::string& field = editingField_ == 1   ? creatorName_
                             : editingField_ == 2 ? creatorBackstory_
                                                  : creatorTraits_;
        const std::size_t cap = editingField_ == 1 ? 32 : 160;
        int ch = GetCharPressed();
        while (ch != 0) {
            if (ch >= 32 && ch < 127 && field.size() < cap) {
                field.push_back(static_cast<char>(ch));
            }
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
            !field.empty()) {
            field.pop_back();
        }
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            editingField_ = 0;
            return MenuResult::None;  // the Escape ends the edit, not the page
        }
        // Clicking elsewhere also drops focus (fall through to clicks).
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

    if (editingGen_) {
        int ch = GetCharPressed();
        while (ch != 0) {
            if (ch >= 32 && ch < 127 && genDescription_.size() < 200) {
                genDescription_.push_back(static_cast<char>(ch));
            }
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
            !genDescription_.empty()) {
            genDescription_.pop_back();
        }
        if (IsKeyPressed(KEY_ESCAPE)) editingGen_ = false;
        if (IsKeyPressed(KEY_ENTER)) {
            editingGen_ = false;
            if (sandbox_.onGenerate && !trim(genDescription_).empty()) {
                showToast(sandbox_.onGenerate(trim(genDescription_)));
            }
        }
    }

    if (!editingAddress_ && !editingGen_ && IsKeyPressed(KEY_ESCAPE)) {
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
            } else if (hit.id == kIdCreator) {
                page_ = Page::Creator;
                avatarMode_ = false;
                editingField_ = 0;
            } else if (hit.id == kIdAvatar) {
                page_ = Page::Creator;
                avatarMode_ = true;
                editingField_ = 0;
                if (avatar_.current) draftLook_ = avatar_.current();
            } else if (hit.id == kIdJournal) {
                page_ = Page::Journal;
                journalPage_ = 0;
            } else if (hit.id == kIdSandbox) {
                page_ = Page::Sandbox;
                sandboxMaps_ = sandbox_.listMaps ? sandbox_.listMaps()
                                                 : std::vector<std::string>{};
            } else if (hit.id == kIdMapNew) {
                if (sandbox_.onOpen) sandbox_.onOpen("");
            } else if (hit.id == kIdGenField) {
                editingGen_ = true;
            } else if (hit.id == kIdGenGo) {
                editingGen_ = false;
                if (sandbox_.onGenerate && !trim(genDescription_).empty()) {
                    showToast(sandbox_.onGenerate(trim(genDescription_)));
                }
            } else if (hit.id >= kIdMapBase && hit.id < kIdMapBase + kMaxMapRows) {
                const std::size_t index =
                    static_cast<std::size_t>(hit.id - kIdMapBase);
                if (sandbox_.onOpen && index < sandboxMaps_.size()) {
                    sandbox_.onOpen(sandboxMaps_[index]);
                }
            } else if (hit.id == kIdJournalPrev) {
                if (journalPage_ > 0) --journalPage_;
            } else if (hit.id == kIdJournalNext) {
                const int rows = journal_.entries
                                     ? static_cast<int>(journal_.entries().size())
                                     : 0;
                if ((journalPage_ + 1) * kJournalRowsPerPage < rows) ++journalPage_;
            } else if (hit.id == kIdBack) {
                awaiting_.reset();
                editingAddress_ = false;
                editingField_ = 0;
                page_ = Page::Main;
            } else if (hit.id >= kIdFieldBase + 1 && hit.id <= kIdFieldBase + 3) {
                editingField_ = hit.id - kIdFieldBase;
            } else if (hit.id >= kIdPartPrev && hit.id < kIdPartPrev + kPartCategoryCount) {
                editingField_ = 0;
                cycleCreatorPart(static_cast<PartCategory>(hit.id - kIdPartPrev), -1);
            } else if (hit.id >= kIdPartNext && hit.id < kIdPartNext + kPartCategoryCount) {
                editingField_ = 0;
                cycleCreatorPart(static_cast<PartCategory>(hit.id - kIdPartNext), +1);
            } else if (hit.id == kIdPalette) {
                editingField_ = 0;
                const auto& palettes = paletteCatalog();
                std::size_t index = 0;
                for (std::size_t i = 0; i < palettes.size(); ++i) {
                    if (palettes[i].id == draftLook_.paletteId) index = i;
                }
                draftLook_.paletteId = palettes[(index + 1) % palettes.size()].id;
            } else if (hit.id == kIdTrait) {
                editingField_ = 0;
                creatorTraitIndex_ = (creatorTraitIndex_ + 1) %
                                     (static_cast<int>(traitChoices_.size()) + 1);
            } else if (hit.id == kIdRandomize) {
                editingField_ = 0;
                creatorSeed_ = creatorSeed_ * 7919u + 13u;
                draftLook_ = randomizeLook(creatorSeed_);
            } else if (hit.id == kIdCreate) {
                editingField_ = 0;
                if (avatarMode_) {
                    const std::string error =
                        avatar_.onSave ? avatar_.onSave(draftLook_) : "no avatar hook";
                    showToast(error.empty() ? "Avatar saved" : error);
                } else {
                    attemptCreate();
                }
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
        // Clicking empty space cancels an armed capture or field edit.
        if (!hitSomething) {
            awaiting_.reset();
            editingAddress_ = false;
            editingField_ = 0;
        }
    }

    return MenuResult::None;
}

bool Menu::pointOverInteractive(Vector2 p) const {
    // True over any clickable control on the current page. main.cpp uses this
    // so a left-drag that starts over the creator preview (empty space) rotates
    // the figure, while a press over a cycler/field/button stays a click.
    for (const Hit& h : layout()) {
        if (CheckCollisionPointRec(p, h.rect)) return true;
    }
    return false;
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

void Menu::cycleCreatorPart(PartCategory category, int direction) {
    // Options are gated by the BODY's style family (the body is the style
    // anchor); cycling the body itself offers every body, then re-validates
    // the other categories against the new family.
    const PartDef* body = findPart(draftLook_.part(PartCategory::Body));
    const std::string gate =
        category == PartCategory::Body ? "any" : (body ? body->styleTag : "any");
    const auto options = partsForCategory(category, gate);
    if (options.empty()) return;

    std::size_t index = 0;
    for (std::size_t i = 0; i < options.size(); ++i) {
        if (options[i]->id == draftLook_.part(category)) index = i;
    }
    index = (index + options.size() + static_cast<std::size_t>(direction)) %
            options.size();
    draftLook_.part(category) = options[index]->id;

    if (category == PartCategory::Body) {
        const PartDef* newBody = options[index];
        for (const CategorySpec& spec : categorySpecs()) {
            if (spec.category == PartCategory::Body) continue;
            const PartDef* current = findPart(draftLook_.part(spec.category));
            if (current && styleCompatible(*newBody, *current)) continue;
            const auto valid = partsForCategory(spec.category, newBody->styleTag);
            if (!valid.empty()) draftLook_.part(spec.category) = valid.front()->id;
        }
    }
}

void Menu::attemptCreate() {
    if (!creator_.onCreate) return;
    if (trim(creatorName_).empty()) {
        showToast("Name required");
        return;
    }
    const std::string traitId =
        creatorTraitIndex_ == 0
            ? std::string()
            : traitChoices_[static_cast<std::size_t>(creatorTraitIndex_ - 1)];
    const std::string error =
        creator_.onCreate(trim(creatorName_), trim(creatorBackstory_),
                          trim(creatorTraits_), draftLook_, traitId);
    if (!error.empty()) {
        showToast(error);
        return;
    }
    showToast(trim(creatorName_) + " has moved into town!");
    creatorName_.clear();
    creatorBackstory_.clear();
    creatorTraits_.clear();
    creatorTraitIndex_ = 0;
    creatorSeed_ = creatorSeed_ * 7919u + 13u;
    draftLook_ = randomizeLook(creatorSeed_);
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

    // The Creator page dims less: its live preview is a real in-world
    // render behind the overlay and must stay readable.
    const unsigned char dim = page_ == Page::Creator ? 90 : 170;
    DrawRectangle(0, 0, static_cast<int>(w), static_cast<int>(h), Color{8, 10, 16, dim});

    const char* title = page_ == Page::Main       ? "Paused"
                        : page_ == Page::Controls ? "Controls"
                        : page_ == Page::Creator  ? "Create a Character"
                        : page_ == Page::Journal  ? "Journal"
                                                  : "Multiplayer";
    drawCenteredLine(title, 40, h * 0.12f, Color{235, 240, 250, 255});

    if (page_ == Page::Journal) {
        const std::vector<JournalRow> rows =
            journal_.entries ? journal_.entries() : std::vector<JournalRow>{};
        if (rows.empty()) {
            drawCenteredLine("You haven't been told anything yet.", 22, h * 0.4f,
                             Color{170, 180, 200, 255});
        }
        const int first = journalPage_ * kJournalRowsPerPage;
        float y = h * 0.22f;
        std::string lastSubject;
        for (int i = first;
             i < static_cast<int>(rows.size()) && i < first + kJournalRowsPerPage;
             ++i) {
            const JournalRow& row = rows[static_cast<std::size_t>(i)];
            const float x = w * 0.14f;
            if (row.subject != lastSubject) {  // one header per topic group
                lastSubject = row.subject;
                DrawText(row.subject.c_str(), static_cast<int>(x),
                         static_cast<int>(y), 20, Color{130, 190, 250, 255});
                y += 28.f;
            }
            const Color ink = row.conflicting ? Color{255, 170, 130, 255}
                                              : Color{225, 230, 240, 255};
            std::string line = row.content + "  (" + row.attribution + ")";
            if (row.conflicting) line += "  [conflicting accounts]";
            DrawText(line.c_str(), static_cast<int>(x + 18.f), static_cast<int>(y),
                     18, ink);
            y += 30.f;
        }
        if (static_cast<int>(rows.size()) > kJournalRowsPerPage) {
            drawCenteredLine("page " + std::to_string(journalPage_ + 1), 16,
                             h * 0.80f, Color{140, 150, 170, 255});
        }
    }

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
        else if (hit.id == kIdCreator) label = "Create Character";
        else if (hit.id == kIdAvatar) label = "Edit My Avatar";
        else if (hit.id == kIdSandbox) label = "Sandbox Maps";
        else if (hit.id == kIdMapNew) label = "+ New Map";
        else if (hit.id == kIdGenField) {
            label = genDescription_ + (editingGen_ ? "_" : "");
            if (label.empty()) label = "(describe a map to generate)";
        } else if (hit.id == kIdGenGo) label = "Generate";
        else if (hit.id >= kIdMapBase && hit.id < kIdMapBase + kMaxMapRows) {
            const std::size_t index = static_cast<std::size_t>(hit.id - kIdMapBase);
            label = index < sandboxMaps_.size() ? sandboxMaps_[index] : "?";
        }
        else if (hit.id == kIdJournal) label = "Journal";
        else if (hit.id == kIdJournalPrev) label = "< Prev";
        else if (hit.id == kIdJournalNext) label = "Next >";
        else if (hit.id == kIdQuit) label = "Quit";
        else if (hit.id == kIdBack) label = "Back";
        else if (hit.id >= kIdFieldBase + 1 && hit.id <= kIdFieldBase + 3) {
            const int f = hit.id - kIdFieldBase;
            const std::string& value = f == 1 ? creatorName_
                                     : f == 2 ? creatorBackstory_
                                              : creatorTraits_;
            label = value + (editingField_ == f ? "_" : "");
            if (label.empty()) label = "(click to type)";
            const char* caption = f == 1   ? "Name (required):"
                                  : f == 2 ? "Backstory:"
                                           : "Traits (comma-separated):";
            DrawText(caption, static_cast<int>(hit.rect.x),
                     static_cast<int>(hit.rect.y - 24.f), 16, Color{170, 180, 200, 255});
        } else if (hit.id >= kIdPartPrev && hit.id < kIdPartPrev + kPartCategoryCount) {
            label = "<";
            // The row's category and current pick draw once, between the
            // two arrow buttons (this is the prev arrow; the next arrow is
            // 44 px inside the row's right edge — see layout()).
            const auto category = static_cast<PartCategory>(hit.id - kIdPartPrev);
            DrawText(categoryLabel(category), static_cast<int>(hit.rect.x),
                     static_cast<int>(hit.rect.y - 20.f), 16, Color{170, 180, 200, 255});
            const Rectangle between{hit.rect.x + 44.f, hit.rect.y, w * 0.31f - 88.f,
                                    hit.rect.height};
            drawCentered(partLabel(draftLook_.part(category)), 20, between, WHITE);
        } else if (hit.id >= kIdPartNext && hit.id < kIdPartNext + kPartCategoryCount) {
            label = ">";
        } else if (hit.id == kIdPalette) {
            label = "Palette: " + draftLook_.paletteId;
        } else if (hit.id == kIdTrait) {
            label = "Trait: " + (creatorTraitIndex_ == 0
                                     ? std::string("none")
                                     : traitChoices_[static_cast<std::size_t>(
                                           creatorTraitIndex_ - 1)]);
        } else if (hit.id == kIdRandomize) label = "Randomize";
        else if (hit.id == kIdCreate) label = avatarMode_ ? "Save Avatar" : "Save & Spawn";
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
