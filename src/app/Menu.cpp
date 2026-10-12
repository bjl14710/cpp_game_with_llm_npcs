#include "Menu.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Config.hpp"  // trim
#include "InputMap.hpp"
#include "Theme.hpp"

namespace llm_npc {

namespace {

// Ids used by layout()/activate() to identify what was clicked.
constexpr int kIdResume = 0;
constexpr int kIdControls = 1;
constexpr int kIdQuit = 2;
constexpr int kIdMultiplayer = 3;
constexpr int kIdCreator = 4;
constexpr int kIdJournal = 5;
constexpr int kIdAvatar = 6;
constexpr int kIdSandbox = 7;
constexpr int kIdModels = 8;
constexpr int kIdNewMystery = 9;
constexpr int kIdMatchSettings = 10;
constexpr int kIdCredits = 11;
constexpr int kIdBack = 100;
// Controls-page chips use ids [kIdChipBase, kIdChipBase + kActionCount).
constexpr int kIdChipBase = 200;
// Multiplayer-page widgets.
constexpr int kIdHost = 300;
constexpr int kIdAddress = 301;
constexpr int kIdJoin = 302;
constexpr int kIdLeave = 303;
// Creator-page widgets. Field ids map to editingField_ values 1-3; the
// part cyclers encode category and direction in their id; palette swatches
// and trait chips are one id per choice.
constexpr int kIdFieldBase = 400;  // +1 name, +2 backstory, +3 traits
constexpr int kIdPartPrev = 410;   // +category
constexpr int kIdPartNext = 420;   // +category
constexpr int kIdRandomize = 431;
constexpr int kIdCreate = 432;
constexpr int kIdPaletteBase = 440;  // +palette index
constexpr int kMaxPalettes = 16;
constexpr int kIdTraitBase = 460;  // +0 none, +i the i-th trait choice
constexpr int kMaxTraitChips = 16;
// Journal-page widgets.
constexpr int kIdJournalPrev = 500;
constexpr int kIdJournalNext = 501;
constexpr int kIdJournalSubjectBase = 510;  // +index into the subject list
constexpr int kMaxJournalSubjects = 14;
// Sandbox page: New button, generate field + button, one row per saved map.
constexpr int kIdMapNew = 600;
constexpr int kIdGenField = 601;
constexpr int kIdGenGo = 602;
constexpr int kIdMapBase = 610;
constexpr int kMaxMapRows = 7;
constexpr int kIdModelBase = 700;
constexpr int kMaxModelRows = 8;
// Match-settings page: two values, each with a decrement and an increment.
constexpr int kIdDaysDown = 800;
constexpr int kIdDaysUp = 801;
constexpr int kIdLengthDown = 802;
constexpr int kIdLengthUp = 803;
// Bounds for the two settings. A day limit of 1 is a legitimate one-day
// mystery; 7 is where a session stops being one sitting. Day length is
// bounded below by "long enough to walk across downtown and hold two
// conversations" and above by the same sitting argument.
constexpr int kDayLimitMin = 1;
constexpr int kDayLimitMax = 7;
constexpr int kDayMinutesMin = 2;
constexpr int kDayMinutesMax = 20;

// Credits-page widgets: the two tier cards and one Buy button per amount.
constexpr int kIdTierFree = 900;
constexpr int kIdTierPaid = 901;
constexpr int kIdBuyBase = 910;  // +index into kBuyAmountsUsd
constexpr int kBuyAmountsUsd[] = {5, 10, 20};

// ---- geometry shared by layout() and render() -----------------------------
constexpr float kPanelWidth = 400.f;  // main page: the left panel
constexpr float kPad = 36.f;          // sub-pages: outer padding
constexpr float kContentTop = 150.f;  // sub-pages: first row below the header
constexpr float kBackWidth = 128.f;
constexpr float kRowWidth = 552.f;  // list rows on Sandbox / Model
constexpr float kListRow = 46.f;
constexpr float kListStep = 50.f;
constexpr float kCardHeight = 96.f;  // journal cards
constexpr float kCardGap = 12.f;
constexpr float kSubjectWidth = 240.f;
constexpr float kButtonH = 36.f;

// The main page, in display order. A `group` starts a new section.
struct MainEntry {
    int id;
    const char* label;
    const char* group;
};
constexpr MainEntry kMainEntries[] = {
    {kIdResume, "Resume", "Play"},
    {kIdNewMystery, "New Mystery", nullptr},
    {kIdMatchSettings, "Match Settings", nullptr},
    {kIdJournal, "Journal", "You"},
    {kIdCreator, "Create Character", nullptr},
    {kIdAvatar, "Edit My Avatar", nullptr},
    {kIdMultiplayer, "Multiplayer", "World"},
    {kIdSandbox, "Sandbox Maps", nullptr},
    {kIdControls, "Controls", "System"},
    {kIdModels, "LLM Model", nullptr},
    {kIdCredits, "AI: Free / Paid", nullptr},
    {kIdQuit, "Quit", nullptr},
};
constexpr int kMainCount = static_cast<int>(std::size(kMainEntries));
constexpr float kGroupHeader = 26.f;  // room above a group's first row
constexpr float kGroupGap = 12.f;

// Row rectangles for the main page, in kMainEntries order. Fitted to the
// window height so adding an entry crowds the list rather than pushing Quit
// off-screen (which an earlier fixed-step layout did, unnoticed).
std::vector<Rectangle> mainRowRects(float h) {
    const float top = 176.f;
    const float bottom = h - 64.f;
    int groups = 0;
    for (const MainEntry& e : kMainEntries) groups += e.group != nullptr;
    const float fixed = groups * kGroupHeader + (groups - 1) * kGroupGap;
    const float rowH = std::clamp((bottom - top - fixed) / kMainCount, 22.f, 32.f);
    std::vector<Rectangle> rects;
    float y = top;
    for (int i = 0; i < kMainCount; ++i) {
        if (kMainEntries[i].group) {
            if (i > 0) y += kGroupGap;
            y += kGroupHeader;
        }
        rects.push_back(Rectangle{28.f, y, kPanelWidth - 56.f, rowH});
        y += rowH;
    }
    return rects;
}

Rectangle backRect(float w) { return Rectangle{w - kPad - kBackWidth, 48.f, kBackWidth, 32.f}; }

// Everything the Creator page positions, so hits and drawing cannot drift.
struct CreatorLayout {
    Rectangle fields[3];           // name, backstory, traits (not avatar mode)
    Rectangle partPrev[kPartCategoryCount];
    Rectangle partNext[kPartCategoryCount];
    Rectangle partRow[kPartCategoryCount];
    float paletteHeaderY = 0.f;
    std::vector<Vector2> swatches;  // one per palette
    float traitHeaderY = 0.f;
    std::vector<Rectangle> traitChips;  // [0] = none
    Rectangle randomize;
    Rectangle create;
    float rightX = 0.f;
    float rightW = 364.f;
};

CreatorLayout creatorLayout(float w, float h, bool avatarMode,
                            const std::vector<std::string>& traitChoices) {
    CreatorLayout l;
    float y = kContentTop + 20.f;
    for (int f = 0; f < 3; ++f) {
        l.fields[f] = Rectangle{kPad, y, 360.f, 40.f};
        y += 78.f;
    }
    l.rightX = w - kPad - l.rightW;
    const float rx = l.rightX;
    y = kContentTop + 22.f;
    for (int c = 0; c < kPartCategoryCount; ++c) {
        l.partRow[c] = Rectangle{rx, y, l.rightW, 36.f};
        l.partPrev[c] = Rectangle{rx + 72.f, y + 2.f, 32.f, 32.f};
        l.partNext[c] = Rectangle{rx + l.rightW - 32.f, y + 2.f, 32.f, 32.f};
        y += 40.f;
    }
    l.paletteHeaderY = y + 8.f;
    y += 30.f;
    const auto& palettes = paletteCatalog();
    const int perRow = static_cast<int>(l.rightW / 36.f);
    for (std::size_t i = 0; i < palettes.size() && i < kMaxPalettes; ++i) {
        const int col = static_cast<int>(i) % perRow;
        const int row = static_cast<int>(i) / perRow;
        l.swatches.push_back(Vector2{rx + 14.f + col * 36.f, y + 14.f + row * 36.f});
    }
    const int paletteRows =
        (static_cast<int>(std::min<std::size_t>(palettes.size(), kMaxPalettes)) + perRow - 1) /
        perRow;
    y += paletteRows * 36.f + 8.f;
    if (!avatarMode && !traitChoices.empty()) {
        l.traitHeaderY = y;
        y += 22.f;
        float cx = rx;
        const auto place = [&](const std::string& label) {
            const float cw = ui::chipWidth(label);
            if (cx + cw > rx + l.rightW && cx > rx) {
                cx = rx;
                y += 28.f;
            }
            l.traitChips.push_back(Rectangle{cx, y, cw, 22.f});
            cx += cw + 6.f;
        };
        place("None");
        for (std::size_t i = 0; i < traitChoices.size() && i + 1 < kMaxTraitChips; ++i) {
            place(traitChoices[i]);
        }
    }
    l.create = Rectangle{w - kPad - 150.f, h - kPad - kButtonH, 150.f, kButtonH};
    l.randomize = Rectangle{l.create.x - 12.f - 120.f, l.create.y, 120.f, kButtonH};
    (void)h;
    return l;
}

// Journal: the subjects in order of first appearance, with how many rows
// each has.
struct SubjectGroup {
    std::string subject;
    int count = 0;
};
std::vector<SubjectGroup> groupSubjects(const std::vector<Menu::JournalRow>& rows) {
    std::vector<SubjectGroup> groups;
    for (const Menu::JournalRow& row : rows) {
        auto it = std::find_if(groups.begin(), groups.end(), [&](const SubjectGroup& g) {
            return g.subject == row.subject;
        });
        if (it == groups.end()) groups.push_back({row.subject, 1});
        else ++it->count;
    }
    return groups;
}

// Topic keys are normalised ("bakery_fire"); the page shows them as words.
std::string subjectLabel(std::string subject) {
    for (char& c : subject) {
        if (c == '_') c = ' ';
    }
    return subject;
}

int journalCardsPerPage(float h) {
    return std::max(1, static_cast<int>((h - kContentTop - 96.f) / (kCardHeight + kCardGap)));
}

int journalSubjectRows(float h) {
    return std::clamp(static_cast<int>((h - kContentTop - 60.f) / 34.f), 1, kMaxJournalSubjects);
}

// Rows a list page can show below kContentTop + `offset` and above the
// bottom padding.
int listRowsThatFit(float h, float offset) {
    return std::max(0, static_cast<int>((h - kContentTop - offset - 48.f) / kListStep));
}

// Display name for a catalog part id: the text after the category prefix
// ("hair_bowl" -> "bowl"), title-ish enough for a picker row.
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

// Appends printable ASCII from this frame's typed characters to `field`,
// up to `cap`, and applies Backspace.
void typeInto(std::string& field, std::size_t cap) {
    int ch = GetCharPressed();
    while (ch != 0) {
        if (ch >= 32 && ch < 127 && field.size() < cap) field.push_back(static_cast<char>(ch));
        ch = GetCharPressed();
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !field.empty()) {
        field.pop_back();
    }
}

bool hovered(const Rectangle& r) { return CheckCollisionPointRec(GetMousePosition(), r); }

Color divider() { return ui::alpha(ui::kText, 0.16f); }

}  // namespace

Menu::Menu(KeyBindings& bindings, std::filesystem::path savePath)
    : bindings_(bindings), savePath_(std::move(savePath)) {}

void Menu::setMultiplayer(MultiplayerHooks hooks) { multiplayer_ = std::move(hooks); }

void Menu::setCreator(CreatorHooks hooks) { creator_ = std::move(hooks); }

void Menu::setAvatar(AvatarHooks hooks) { avatar_ = std::move(hooks); }

void Menu::setSandbox(SandboxHooks hooks) { sandbox_ = std::move(hooks); }

void Menu::setModels(ModelHooks hooks) { models_ = std::move(hooks); }

void Menu::setCredits(CreditsHooks hooks) { credits_ = std::move(hooks); }

void Menu::setMatchSettings(MatchHooks hooks) { matchSettings_ = std::move(hooks); }

void Menu::setHeader(std::string kicker, std::string sub) {
    headerKicker_ = std::move(kicker);
    headerSub_ = std::move(sub);
}

void Menu::nudgeMatchSetting(int id) {
    if (!matchSettings_.onChange || !matchSettings_.dayLimit ||
        !matchSettings_.dayMinutes) {
        return;
    }
    int days = matchSettings_.dayLimit();
    int minutes = matchSettings_.dayMinutes();
    if (id == kIdDaysDown) --days;
    else if (id == kIdDaysUp) ++days;
    else if (id == kIdLengthDown) --minutes;
    else if (id == kIdLengthUp) ++minutes;
    // Clamped rather than refused: a click at either end is a no-op instead
    // of a toast saying so, which would be noise for the one input a player
    // can see the result of directly. MatchRules clamps again on
    // construction; this bound is about what the page will offer, not about
    // what the clock will accept.
    days = std::clamp(days, kDayLimitMin, kDayLimitMax);
    minutes = std::clamp(minutes, kDayMinutesMin, kDayMinutesMax);
    matchSettings_.onChange(days, minutes);
    // Say it once per edit rather than only on the page's static line: a
    // player changing settings DURING a match is exactly the person who will
    // otherwise assume it took effect and wonder why the day did not change.
    if (matchSettings_.active && matchSettings_.active()) {
        showToast("Saved - applies to the next mystery, not this one.");
    }
}

void Menu::setTraitChoices(std::vector<std::string> ids) {
    traitChoices_ = std::move(ids);
    creatorTraitIndex_ = 0;
}

void Menu::setJournal(JournalHooks hooks) { journal_ = std::move(hooks); }

void Menu::open() {
    page_ = Page::Main;
    selected_ = 0;
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
        const std::vector<Rectangle> rects = mainRowRects(h);
        for (int i = 0; i < kMainCount; ++i) hits.push_back({rects[i], kMainEntries[i].id});
        return hits;
    }

    hits.push_back({backRect(w), kIdBack});

    if (page_ == Page::Journal) {
        const std::vector<JournalRow> rows =
            journal_.entries ? journal_.entries() : std::vector<JournalRow>{};
        const std::vector<SubjectGroup> subjects = groupSubjects(rows);
        const int shown = std::min(static_cast<int>(subjects.size()), journalSubjectRows(h));
        for (int i = 0; i < shown; ++i) {
            hits.push_back({Rectangle{kPad, kContentTop + i * 34.f, kSubjectWidth, 34.f},
                            kIdJournalSubjectBase + i});
        }
        const float y = h - kPad - kButtonH;
        hits.push_back({Rectangle{w - kPad - 208.f, y, 100.f, kButtonH}, kIdJournalPrev});
        hits.push_back({Rectangle{w - kPad - 100.f, y, 100.f, kButtonH}, kIdJournalNext});
    } else if (page_ == Page::Creator) {
        const CreatorLayout l = creatorLayout(w, h, avatarMode_, traitChoices_);
        if (!avatarMode_) {  // the avatar has no persona; look-only editing
            for (int f = 0; f < 3; ++f) hits.push_back({l.fields[f], kIdFieldBase + 1 + f});
        }
        for (int c = 0; c < kPartCategoryCount; ++c) {
            hits.push_back({l.partPrev[c], kIdPartPrev + c});
            hits.push_back({l.partNext[c], kIdPartNext + c});
        }
        for (std::size_t i = 0; i < l.swatches.size(); ++i) {
            hits.push_back({Rectangle{l.swatches[i].x - 16.f, l.swatches[i].y - 16.f, 32.f, 32.f},
                            kIdPaletteBase + static_cast<int>(i)});
        }
        for (std::size_t i = 0; i < l.traitChips.size(); ++i) {
            hits.push_back({l.traitChips[i], kIdTraitBase + static_cast<int>(i)});
        }
        hits.push_back({l.randomize, kIdRandomize});
        hits.push_back({l.create, kIdCreate});
    } else if (page_ == Page::Sandbox) {
        float y = kContentTop;
        hits.push_back({Rectangle{kPad, y, 420.f, 40.f}, kIdGenField});
        hits.push_back({Rectangle{kPad + 432.f, y, 120.f, 40.f}, kIdGenGo});
        y += 56.f;
        hits.push_back({Rectangle{kPad, y, kRowWidth, kListRow}, kIdMapNew});
        y += kListStep + 4.f;
        const int rows = std::min({kMaxMapRows, static_cast<int>(sandboxMaps_.size()),
                                   listRowsThatFit(h, y - kContentTop)});
        for (int i = 0; i < rows; ++i) {
            hits.push_back({Rectangle{kPad, y, kRowWidth, kListRow}, kIdMapBase + i});
            y += kListStep;
        }
    } else if (page_ == Page::Match) {
        // Two rows, each "label ... - value +": the minus and plus are the
        // clickable parts and render() draws the value between them.
        float y = kContentTop + 8.f;
        for (const std::pair<int, int> pair :
             {std::pair<int, int>{kIdDaysDown, kIdDaysUp},
              std::pair<int, int>{kIdLengthDown, kIdLengthUp}}) {
            hits.push_back({Rectangle{kPad + 480.f - 24.f - 144.f, y + 18.f, 36.f, 36.f},
                            pair.first});
            hits.push_back({Rectangle{kPad + 480.f - 24.f - 36.f, y + 18.f, 36.f, 36.f},
                            pair.second});
            y += 88.f;
        }
    } else if (page_ == Page::Credits) {
        // Two tier cards side by side, then a row of Buy buttons under the
        // balance line that render() draws between them.
        const float y = kContentTop + 8.f;
        hits.push_back({Rectangle{kPad, y, 340.f, 96.f}, kIdTierFree});
        hits.push_back({Rectangle{kPad + 356.f, y, 340.f, 96.f}, kIdTierPaid});
        const float buyY = y + 96.f + 56.f;
        for (int i = 0; i < static_cast<int>(std::size(kBuyAmountsUsd)); ++i) {
            hits.push_back({Rectangle{kPad + 152.f * static_cast<float>(i), buyY, 140.f, kButtonH},
                            kIdBuyBase + i});
        }
    } else if (page_ == Page::Model) {
        float y = kContentTop;
        const int rows = std::min({kMaxModelRows, static_cast<int>(modelList_.size()),
                                   listRowsThatFit(h, 0.f)});
        for (int i = 0; i < rows; ++i) {
            hits.push_back({Rectangle{kPad, y, kRowWidth, kListRow}, kIdModelBase + i});
            y += kListStep;
        }
    } else if (page_ == Page::Multiplayer) {
        const float cardW = std::min(360.f, (w - 2.f * kPad - 24.f) * 0.5f);
        const float y = kContentTop + 8.f;
        if (multiplayer_.active && multiplayer_.active()) {
            hits.push_back({Rectangle{kPad + 24.f, y + 160.f - 24.f - kButtonH, cardW - 48.f,
                                      kButtonH},
                            kIdLeave});
        } else {
            constexpr float cardH = 236.f;
            const float jx = kPad + cardW + 24.f;
            hits.push_back({Rectangle{kPad + 24.f, y + cardH - 24.f - kButtonH, cardW - 48.f,
                                      kButtonH},
                            kIdHost});
            hits.push_back({Rectangle{jx + 24.f, y + 100.f, cardW - 48.f, 40.f}, kIdAddress});
            hits.push_back({Rectangle{jx + 24.f, y + cardH - 24.f - kButtonH, cardW - 48.f,
                                      kButtonH},
                            kIdJoin});
        }
    } else {  // Controls
        float y = kContentTop;
        for (std::size_t i = 0; i < kActionCount; ++i) {
            hits.push_back({Rectangle{kPad + 300.f, y, 160.f, 32.f},
                            kIdChipBase + static_cast<int>(i)});
            y += 44.f;
        }
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
        typeInto(field, editingField_ == 1 ? 32 : 160);
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
            editingField_ = 0;
            return MenuResult::None;  // the Escape ends the edit, not the page
        }
        // Clicking elsewhere also drops focus (fall through to clicks).
    }

    // Address editing: typed characters go into the field.
    if (editingAddress_) {
        typeInto(joinAddress_, 64);
        if (IsKeyPressed(KEY_ESCAPE)) editingAddress_ = false;
        if (IsKeyPressed(KEY_ENTER)) {
            editingAddress_ = false;
            attemptJoin();
        }
        // Clicking empty space also drops focus (fall through to clicks).
    }

    if (editingGen_) {
        typeInto(genDescription_, 200);
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

    // Keyboard on the main page: Up/Down move the selection, Enter activates.
    // Hover moves it too, but only while the mouse is actually moving, so a
    // mouse parked over a row does not pin the arrow keys.
    if (page_ == Page::Main) {
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
            selected_ = (selected_ + 1) % kMainCount;
        }
        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
            selected_ = (selected_ + kMainCount - 1) % kMainCount;
        }
        const Vector2 delta = GetMouseDelta();
        if (delta.x != 0.f || delta.y != 0.f) {
            const std::vector<Rectangle> rects =
                mainRowRects(static_cast<float>(GetScreenHeight()));
            for (int i = 0; i < kMainCount; ++i) {
                if (CheckCollisionPointRec(GetMousePosition(), rects[i])) selected_ = i;
            }
        }
        if (IsKeyPressed(KEY_ENTER)) return activate(kMainEntries[selected_].id);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Vector2 click = GetMousePosition();
        for (const Hit& hit : layout()) {
            if (CheckCollisionPointRec(click, hit.rect)) return activate(hit.id);
        }
        // Clicking empty space cancels an armed capture or field edit.
        awaiting_.reset();
        editingAddress_ = false;
        editingGen_ = false;
        editingField_ = 0;
    }

    return MenuResult::None;
}

MenuResult Menu::activate(int id) {
    if (id == kIdResume) return MenuResult::Resume;
    if (id == kIdNewMystery) {
        // Refuse rather than clobber, and say so. The row stays visible and
        // clickable on purpose: hiding it would leave a player wondering
        // where it went, and a toast explains.
        if (matchActive_) {
            showToast("A mystery is already running.");
            return MenuResult::None;
        }
        return MenuResult::NewMystery;
    }
    if (id == kIdQuit) return MenuResult::Quit;
    if (id == kIdControls) {
        page_ = Page::Controls;
    } else if (id == kIdMultiplayer) {
        page_ = Page::Multiplayer;
        editingAddress_ = false;
    } else if (id == kIdCreator) {
        page_ = Page::Creator;
        avatarMode_ = false;
        editingField_ = 0;
    } else if (id == kIdAvatar) {
        page_ = Page::Creator;
        avatarMode_ = true;
        editingField_ = 0;
        if (avatar_.current) draftLook_ = avatar_.current();
    } else if (id == kIdJournal) {
        page_ = Page::Journal;
        journalSubject_ = 0;
        journalPage_ = 0;
    } else if (id == kIdSandbox) {
        page_ = Page::Sandbox;
        sandboxMaps_ = sandbox_.listMaps ? sandbox_.listMaps() : std::vector<std::string>{};
    } else if (id == kIdMatchSettings) {
        page_ = Page::Match;
    } else if (id == kIdDaysDown || id == kIdDaysUp || id == kIdLengthDown ||
               id == kIdLengthUp) {
        nudgeMatchSetting(id);
    } else if (id == kIdCredits) {
        page_ = Page::Credits;
    } else if (id == kIdTierFree || id == kIdTierPaid) {
        if (credits_.onSetPaid) {
            const bool paid = id == kIdTierPaid;
            const std::string error = credits_.onSetPaid(paid);
            showToast(!error.empty() ? "Paid unavailable: " + error
                      : paid         ? "Paid: NPCs now answer with Claude Haiku"
                                     : "Free: NPCs now answer with the local model");
        }
    } else if (id >= kIdBuyBase &&
               id < kIdBuyBase + static_cast<int>(std::size(kBuyAmountsUsd))) {
        if (credits_.onBuy) showToast(credits_.onBuy(kBuyAmountsUsd[id - kIdBuyBase]));
    } else if (id == kIdModels) {
        page_ = Page::Model;
        modelList_ = models_.listModels ? models_.listModels() : std::vector<std::string>{};
        if (modelList_.empty()) showToast("No models found (is Ollama running?)");
    } else if (id >= kIdModelBase && id < kIdModelBase + kMaxModelRows) {
        const auto index = static_cast<std::size_t>(id - kIdModelBase);
        if (models_.onSelect && index < modelList_.size()) {
            const std::string error = models_.onSelect(modelList_[index]);
            showToast(error.empty() ? "Model switched to " + modelList_[index] +
                                          " (next replies use it)"
                                    : error);
        }
    } else if (id == kIdMapNew) {
        if (sandbox_.onOpen) sandbox_.onOpen("");
    } else if (id == kIdGenField) {
        editingGen_ = true;
    } else if (id == kIdGenGo) {
        editingGen_ = false;
        if (sandbox_.onGenerate && !trim(genDescription_).empty()) {
            showToast(sandbox_.onGenerate(trim(genDescription_)));
        }
    } else if (id >= kIdMapBase && id < kIdMapBase + kMaxMapRows) {
        const auto index = static_cast<std::size_t>(id - kIdMapBase);
        if (sandbox_.onOpen && index < sandboxMaps_.size()) sandbox_.onOpen(sandboxMaps_[index]);
    } else if (id >= kIdJournalSubjectBase && id < kIdJournalSubjectBase + kMaxJournalSubjects) {
        journalSubject_ = id - kIdJournalSubjectBase;
        journalPage_ = 0;
    } else if (id == kIdJournalPrev) {
        if (journalPage_ > 0) --journalPage_;
    } else if (id == kIdJournalNext) {
        const std::vector<JournalRow> rows =
            journal_.entries ? journal_.entries() : std::vector<JournalRow>{};
        const std::vector<SubjectGroup> subjects = groupSubjects(rows);
        const int count =
            journalSubject_ < static_cast<int>(subjects.size())
                ? subjects[static_cast<std::size_t>(journalSubject_)].count
                : 0;
        const int perPage = journalCardsPerPage(static_cast<float>(GetScreenHeight()));
        if ((journalPage_ + 1) * perPage < count) ++journalPage_;
    } else if (id == kIdBack) {
        awaiting_.reset();
        editingAddress_ = false;
        editingGen_ = false;
        editingField_ = 0;
        page_ = Page::Main;
    } else if (id >= kIdFieldBase + 1 && id <= kIdFieldBase + 3) {
        editingField_ = id - kIdFieldBase;
    } else if (id >= kIdPartPrev && id < kIdPartPrev + kPartCategoryCount) {
        editingField_ = 0;
        cycleCreatorPart(static_cast<PartCategory>(id - kIdPartPrev), -1);
    } else if (id >= kIdPartNext && id < kIdPartNext + kPartCategoryCount) {
        editingField_ = 0;
        cycleCreatorPart(static_cast<PartCategory>(id - kIdPartNext), +1);
    } else if (id >= kIdPaletteBase && id < kIdPaletteBase + kMaxPalettes) {
        editingField_ = 0;
        const auto& palettes = paletteCatalog();
        const auto index = static_cast<std::size_t>(id - kIdPaletteBase);
        if (index < palettes.size()) draftLook_.paletteId = palettes[index].id;
    } else if (id >= kIdTraitBase && id < kIdTraitBase + kMaxTraitChips) {
        editingField_ = 0;
        const int index = id - kIdTraitBase;
        if (index <= static_cast<int>(traitChoices_.size())) creatorTraitIndex_ = index;
    } else if (id == kIdRandomize) {
        editingField_ = 0;
        creatorSeed_ = creatorSeed_ * 7919u + 13u;
        draftLook_ = randomizeLook(creatorSeed_);
    } else if (id == kIdCreate) {
        editingField_ = 0;
        if (avatarMode_) {
            const std::string error =
                avatar_.onSave ? avatar_.onSave(draftLook_) : "no avatar hook";
            showToast(error.empty() ? "Avatar saved" : error);
        } else {
            attemptCreate();
        }
    } else if (id == kIdHost) {
        if (multiplayer_.onHost) {
            const std::string error = multiplayer_.onHost(kDefaultHostPort);
            showToast(error.empty() ? "Hosting started" : error);
        }
    } else if (id == kIdAddress) {
        editingAddress_ = true;
    } else if (id == kIdJoin) {
        editingAddress_ = false;
        attemptJoin();
    } else if (id == kIdLeave) {
        if (multiplayer_.onLeave) multiplayer_.onLeave();
        showToast("Left the session");
    } else if (id >= kIdChipBase && id < kIdChipBase + static_cast<int>(kActionCount)) {
        awaiting_ = static_cast<Action>(id - kIdChipBase);
        toast_.clear();
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

void Menu::renderPageHeader(const std::string& kicker, const std::string& title,
                            const std::string& sub) const {
    const float w = static_cast<float>(GetScreenWidth());
    ui::kicker(kicker, kPad, 40.f);
    ui::text(ui::Face::Display, title, kPad, 56.f, 34.f, ui::kText);
    ui::text(ui::Face::Ui, sub, kPad, 100.f, 13.f, ui::kNeutral400);
    ui::ruleH(kPad, 126.f, w - 2.f * kPad, divider());
    const Rectangle back = backRect(w);
    ui::button(back, "<- Back  (Esc)", ui::ButtonKind::Ghost, hovered(back));
}

void Menu::render() const {
    using ui::Face;
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());

    if (page_ == Page::Main) {
        ui::panel(Rectangle{0.f, 0.f, w, h}, 0.55f);
        ui::panel(Rectangle{0.f, 0.f, kPanelWidth, h}, 0.94f);
        ui::ruleV(kPanelWidth, 0.f, h, divider());
        ui::kicker(headerKicker_.empty() ? "Paused" : headerKicker_, 40.f, 44.f);
        ui::text(Face::Display, "Paused", 40.f, 60.f, 46.f, ui::kText);
        if (!headerSub_.empty()) ui::text(Face::Ui, headerSub_, 40.f, 118.f, 13.f, ui::kNeutral400);
        ui::ruleH(40.f, 150.f, kPanelWidth - 80.f, divider());

        const std::vector<Rectangle> rects = mainRowRects(h);
        for (int i = 0; i < kMainCount; ++i) {
            const MainEntry& e = kMainEntries[i];
            const Rectangle& r = rects[static_cast<std::size_t>(i)];
            if (e.group) ui::sectionHeader(e.group, r.x + 16.f, r.y - kGroupHeader + 6.f);
            const bool on = i == selected_;
            if (on) ui::selection(r);
            const float textY = r.y + (r.height - 15.f) * 0.5f;
            ui::text(Face::Ui, e.label, r.x + 16.f, textY, 15.f, on ? ui::kText : ui::kNeutral300);
            if (e.id == kIdNewMystery && matchActive_) {
                ui::tag("Running", r.x + r.width - 12.f - ui::tagWidth("Running"), r.y + (r.height - 18.f) * 0.5f);
            }
        }

        float x = 40.f;
        const float fy = h - 44.f;
        x += ui::keycap("Up/Down", x, fy) + 6.f;
        ui::text(Face::Ui, "move", x, fy + 4.f, 11.f, ui::kNeutral500);
        x += ui::measure(Face::Ui, "move", 11.f) + 14.f;
        x += ui::keycap("Enter", x, fy) + 6.f;
        ui::text(Face::Ui, "select", x, fy + 4.f, 11.f, ui::kNeutral500);
        x += ui::measure(Face::Ui, "select", 11.f) + 14.f;
        x += ui::keycap("Esc", x, fy) + 6.f;
        ui::text(Face::Ui, "back", x, fy + 4.f, 11.f, ui::kNeutral500);
        if (!toast_.empty()) ui::toast(toast_, h - 84.f);
        return;
    }

    // The Creator page dims least: its live preview is a real in-world
    // render behind the overlay and must stay readable between the columns.
    ui::panel(Rectangle{0.f, 0.f, w, h}, page_ == Page::Creator ? 0.35f : 0.86f);

    if (page_ == Page::Controls) {
        renderPageHeader("System", "Controls",
                         "Click a key, then press the new key. Esc cancels.");
        for (const Hit& hit : layout()) {
            if (hit.id < kIdChipBase || hit.id >= kIdChipBase + static_cast<int>(kActionCount)) continue;
            const Action action = static_cast<Action>(hit.id - kIdChipBase);
            const bool armed = awaiting_ && *awaiting_ == action;
            std::string label = armed ? "press a key..." : bindings_.key(action);
            if (label.empty()) label = "(unbound)";
            ui::text(Face::Ui, KeyBindings::actionLabel(action), kPad,
                     hit.rect.y + (hit.rect.height - 14.f) * 0.5f, 14.f, ui::kText);
            ui::button(hit.rect, label, armed ? ui::ButtonKind::Primary : ui::ButtonKind::Secondary,
                       hovered(hit.rect));
        }
    } else if (page_ == Page::Multiplayer) {
        const bool active = multiplayer_.active && multiplayer_.active();
        const std::string status = multiplayer_.status ? multiplayer_.status() : "";
        renderPageHeader("World", "Multiplayer",
                         status.empty() ? "Play the same city with friends over LAN." : status);
        const float cardW = std::min(360.f, (w - 2.f * kPad - 24.f) * 0.5f);
        const float y = kContentTop + 8.f;
        if (active) {
            const Rectangle c{kPad, y, cardW, 160.f};
            ui::card(c);
            ui::text(Face::Display, "Connected", c.x + 24.f, c.y + 20.f, 24.f, ui::kText);
            ui::text(Face::Ui, "Leaving returns you to your own city.", c.x + 24.f, c.y + 58.f,
                     13.f, ui::kNeutral400);
        } else {
            constexpr float cardH = 236.f;
            const Rectangle hc{kPad, y, cardW, cardH};
            const Rectangle jc{kPad + cardW + 24.f, y, cardW, cardH};
            ui::card(hc);
            ui::text(Face::Display, "Host", hc.x + 24.f, hc.y + 20.f, 24.f, ui::kText);
            ui::text(Face::Ui, "Run the city for up to three friends.", hc.x + 24.f, hc.y + 58.f,
                     13.f, ui::kNeutral400);
            ui::text(Face::Ui, "Port " + std::to_string(kDefaultHostPort), hc.x + 24.f,
                     hc.y + 82.f, 12.f, ui::kNeutral500);
            ui::card(jc);
            ui::text(Face::Display, "Join", jc.x + 24.f, jc.y + 20.f, 24.f, ui::kText);
            ui::text(Face::Ui, "Walk into a friend's city.", jc.x + 24.f, jc.y + 58.f, 13.f,
                     ui::kNeutral400);
            ui::sectionHeader("Host address", jc.x + 24.f, jc.y + 84.f);
            ui::text(Face::Ui, "LAN, or a Tailscale / ZeroTier address.", jc.x + 24.f,
                     jc.y + 148.f, 11.f, ui::kNeutral500);
        }
        for (const Hit& hit : layout()) {
            if (hit.id == kIdHost) {
                ui::button(hit.rect, "Host a session", ui::ButtonKind::Primary, hovered(hit.rect));
            } else if (hit.id == kIdJoin) {
                ui::button(hit.rect, "Join", ui::ButtonKind::Secondary, hovered(hit.rect));
            } else if (hit.id == kIdLeave) {
                ui::button(hit.rect, "Leave session", ui::ButtonKind::Secondary, hovered(hit.rect));
            } else if (hit.id == kIdAddress) {
                ui::input(hit.rect, joinAddress_, "192.168.1.20:40605", editingAddress_);
            }
        }
    } else if (page_ == Page::Creator) {
        renderPageHeader("You", avatarMode_ ? "Edit My Avatar" : "Create a Character",
                         avatarMode_ ? "Drag the figure to turn it. Changes apply when you save."
                                     : "Name them, give them a past, pick a look. They move in when you save.");
        const CreatorLayout l = creatorLayout(w, h, avatarMode_, traitChoices_);
        if (!avatarMode_) {
            ui::panel(Rectangle{kPad - 16.f, kContentTop - 12.f, 392.f, 3.f * 78.f + 24.f}, 0.72f);
            const char* captions[3] = {"Name (required)", "Backstory", "Traits (comma-separated)"};
            const char* placeholders[3] = {"Who are they?", "Where have they been?",
                                           "e.g. curious, blunt, forgetful"};
            for (int f = 0; f < 3; ++f) {
                const std::string& value = f == 0 ? creatorName_ : f == 1 ? creatorBackstory_ : creatorTraits_;
                ui::sectionHeader(captions[f], l.fields[f].x, l.fields[f].y - 18.f);
                ui::input(l.fields[f], value, placeholders[f], editingField_ == f + 1);
            }
        }
        const float panelBottom = (l.traitChips.empty() ? l.paletteHeaderY + 30.f + 36.f * 2.f
                                                        : l.traitChips.back().y + 22.f) + 16.f;
        ui::panel(Rectangle{l.rightX - 16.f, kContentTop - 12.f, l.rightW + 32.f,
                            panelBottom - kContentTop + 12.f},
                  0.72f);
        ui::sectionHeader("Look", l.rightX, kContentTop);
        for (int c = 0; c < kPartCategoryCount; ++c) {
            const auto category = static_cast<PartCategory>(c);
            const Rectangle& row = l.partRow[c];
            ui::text(Face::Ui, categoryLabel(category), row.x, row.y + 11.f, 13.f, ui::kNeutral400);
            ui::button(l.partPrev[c], "<", ui::ButtonKind::Secondary, hovered(l.partPrev[c]));
            ui::button(l.partNext[c], ">", ui::ButtonKind::Secondary, hovered(l.partNext[c]));
            const float midX = (l.partPrev[c].x + l.partPrev[c].width + l.partNext[c].x) * 0.5f;
            ui::textCentered(Face::Ui, partLabel(draftLook_.part(category)), midX, row.y + 10.f,
                             14.f, ui::kText);
        }
        ui::sectionHeader("Palette", l.rightX, l.paletteHeaderY);
        const auto& palettes = paletteCatalog();
        for (std::size_t i = 0; i < l.swatches.size(); ++i) {
            const PartPalette& p = palettes[i];
            const Color fill{p.outfit[0], p.outfit[1], p.outfit[2], 255};
            const bool on = p.id == draftLook_.paletteId;
            if (on) DrawCircleV(l.swatches[i], 16.f, ui::kAccent);
            DrawCircleV(l.swatches[i], 14.f, fill);
            DrawCircleV(Vector2{l.swatches[i].x - 4.f, l.swatches[i].y - 4.f}, 5.f,
                        Color{p.hair[0], p.hair[1], p.hair[2], 255});
        }
        if (!l.traitChips.empty()) {
            ui::sectionHeader("Trait", l.rightX, l.traitHeaderY);
            for (std::size_t i = 0; i < l.traitChips.size(); ++i) {
                const std::string label = i == 0 ? "None" : traitChoices_[i - 1];
                ui::chip(label, l.traitChips[i].x, l.traitChips[i].y,
                         static_cast<int>(i) == creatorTraitIndex_);
            }
        }
        ui::button(l.randomize, "Randomize", ui::ButtonKind::Secondary, hovered(l.randomize));
        ui::button(l.create, avatarMode_ ? "Save Avatar" : "Save & Spawn", ui::ButtonKind::Primary,
                   hovered(l.create));
    } else if (page_ == Page::Journal) {
        const std::vector<JournalRow> rows =
            journal_.entries ? journal_.entries() : std::vector<JournalRow>{};
        const std::vector<SubjectGroup> subjects = groupSubjects(rows);
        renderPageHeader("Journal - " + std::to_string(rows.size()) +
                             (rows.size() == 1 ? " entry" : " entries"),
                         "Journal",
                         "What people told you, by subject. Accounts that clash are flagged.");
        if (subjects.empty()) {
            ui::textCentered(Face::Ui, "You haven't been told anything yet.", w * 0.5f, h * 0.45f,
                             15.f, ui::kNeutral400);
        } else {
            const int selected =
                std::clamp(journalSubject_, 0, static_cast<int>(subjects.size()) - 1);
            const int shown = std::min(static_cast<int>(subjects.size()), journalSubjectRows(h));
            for (int i = 0; i < shown; ++i) {
                const Rectangle r{kPad, kContentTop + i * 34.f, kSubjectWidth, 34.f};
                const SubjectGroup& g = subjects[static_cast<std::size_t>(i)];
                const bool on = i == selected;
                if (on) ui::selection(r);
                else if (hovered(r)) DrawRectangleRec(r, ui::alpha(ui::kAccent, 0.06f));
                const std::string count = "x" + std::to_string(g.count);
                const float countW = ui::measure(Face::Ui, count, 11.f);
                ui::text(Face::Ui,
                         ui::ellipsize(Face::Ui, subjectLabel(g.subject), 14.f,
                                       kSubjectWidth - 32.f - countW),
                         r.x + 12.f, r.y + 10.f, 14.f, on ? ui::kText : ui::kNeutral300);
                ui::text(Face::Ui, count, r.x + r.width - 12.f - countW, r.y + 12.f, 11.f,
                         ui::kNeutral500);
            }
            ui::ruleV(kPad + kSubjectWidth + 12.f, kContentTop, h - kContentTop - kPad - 48.f,
                      divider());

            const float cx = kPad + kSubjectWidth + 24.f;
            const float cw = w - cx - kPad;
            const std::string& subject = subjects[static_cast<std::size_t>(selected)].subject;
            std::vector<const JournalRow*> mine;
            for (const JournalRow& row : rows) {
                if (row.subject == subject) mine.push_back(&row);
            }
            const int perPage = journalCardsPerPage(h);
            const int pages = std::max(1, (static_cast<int>(mine.size()) + perPage - 1) / perPage);
            const int page = std::clamp(journalPage_, 0, pages - 1);
            float y = kContentTop;
            for (int i = page * perPage;
                 i < static_cast<int>(mine.size()) && i < (page + 1) * perPage; ++i) {
                const JournalRow& row = *mine[static_cast<std::size_t>(i)];
                const Rectangle c{cx, y, cw, kCardHeight};
                ui::card(c);
                if (row.conflicting) {
                    DrawRectangleRoundedLinesEx(c, (ui::kRadius * 2.f) / kCardHeight, 6, 1.f,
                                                ui::kAccent700);
                }
                float textW = cw - 32.f;
                if (row.conflicting) {
                    const float tw = ui::tagWidth("Conflicting accounts");
                    ui::tag("Conflicting accounts", c.x + cw - 16.f - tw, c.y + 14.f);
                    textW -= tw + 12.f;
                }
                std::vector<std::string> lines =
                    ui::wrap(Face::Ui, "\"" + row.content + "\"", 15.f, textW);
                if (lines.size() > 2) {
                    lines.resize(2);
                    lines[1] = ui::ellipsize(Face::Ui, lines[1] + " ...", 15.f, textW);
                }
                for (std::size_t li = 0; li < lines.size(); ++li) {
                    ui::text(Face::Ui, lines[li], c.x + 16.f, c.y + 14.f + li * 20.f, 15.f,
                             ui::kText);
                }
                ui::text(Face::Ui, row.attribution, c.x + 16.f, c.y + kCardHeight - 26.f, 12.f,
                         ui::kNeutral500);
                y += kCardHeight + kCardGap;
            }
            if (pages > 1) {
                ui::text(Face::Ui,
                         "Page " + std::to_string(page + 1) + " of " + std::to_string(pages), cx,
                         h - kPad - kButtonH + 11.f, 12.f, ui::kNeutral500);
                for (const Hit& hit : layout()) {
                    if (hit.id == kIdJournalPrev) {
                        ui::button(hit.rect, "< Prev", ui::ButtonKind::Secondary, hovered(hit.rect),
                                   page > 0);
                    } else if (hit.id == kIdJournalNext) {
                        ui::button(hit.rect, "Next >", ui::ButtonKind::Secondary, hovered(hit.rect),
                                   page + 1 < pages);
                    }
                }
            }
        }
    } else if (page_ == Page::Sandbox) {
        renderPageHeader("Sandbox - " + std::to_string(sandboxMaps_.size()) +
                             (sandboxMaps_.size() == 1 ? " saved map" : " saved maps"),
                         "Sandbox Maps",
                         "Open a saved map in the editor, start a blank one, or describe one.");
        if (sandboxMaps_.empty()) {
            ui::text(Face::Ui, "No saved maps yet.", kPad + 16.f, kContentTop + 56.f + kListStep + 18.f,
                     13.f, ui::kNeutral500);
        }
        for (const Hit& hit : layout()) {
            if (hit.id == kIdGenField) {
                ui::input(hit.rect, genDescription_, "Describe a map to generate...", editingGen_);
            } else if (hit.id == kIdGenGo) {
                ui::button(hit.rect, "Generate", ui::ButtonKind::Primary, hovered(hit.rect));
            } else if (hit.id == kIdMapNew) {
                if (hovered(hit.rect)) DrawRectangleRec(hit.rect, ui::alpha(ui::kAccent, 0.08f));
                ui::text(Face::Ui, "+ New map", hit.rect.x + 16.f, hit.rect.y + 15.f, 15.f,
                         ui::kAccent300);
                ui::ruleH(hit.rect.x, hit.rect.y + hit.rect.height + 2.f, hit.rect.width, divider());
            } else if (hit.id >= kIdMapBase && hit.id < kIdMapBase + kMaxMapRows) {
                const auto index = static_cast<std::size_t>(hit.id - kIdMapBase);
                if (hovered(hit.rect)) DrawRectangleRec(hit.rect, ui::alpha(ui::kAccent, 0.08f));
                ui::text(Face::Display, index < sandboxMaps_.size() ? sandboxMaps_[index] : "?",
                         hit.rect.x + 16.f, hit.rect.y + 12.f, 19.f, ui::kText);
                ui::textRight(Face::Ui, "Open >", hit.rect.x + hit.rect.width - 12.f,
                              hit.rect.y + 17.f, 12.f, ui::kNeutral500);
                ui::ruleH(hit.rect.x, hit.rect.y + hit.rect.height + 2.f, hit.rect.width, divider());
            }
        }
    } else if (page_ == Page::Model) {
        renderPageHeader("Language model", "LLM Model",
                         "Installed models. Click one to switch; the next replies use it.");
        const std::string current = models_.currentModel ? models_.currentModel() : "";
        if (modelList_.empty()) {
            ui::text(Face::Ui, "No models found - is the backend running?", kPad + 16.f,
                     kContentTop + 16.f, 13.f, ui::kNeutral500);
        }
        for (const Hit& hit : layout()) {
            if (hit.id < kIdModelBase || hit.id >= kIdModelBase + kMaxModelRows) continue;
            const auto index = static_cast<std::size_t>(hit.id - kIdModelBase);
            const std::string name = index < modelList_.size() ? modelList_[index] : "?";
            const bool on = name == current;
            if (hovered(hit.rect)) DrawRectangleRec(hit.rect, ui::alpha(ui::kAccent, 0.08f));
            ui::glowDot(Vector2{hit.rect.x + 20.f, hit.rect.y + hit.rect.height * 0.5f}, on);
            ui::text(Face::Ui, name, hit.rect.x + 40.f, hit.rect.y + 15.f, 15.f,
                     on ? ui::kText : ui::kNeutral300);
            if (on) {
                ui::tag("Current", hit.rect.x + hit.rect.width - 12.f - ui::tagWidth("Current"),
                        hit.rect.y + 14.f);
            }
            ui::ruleH(hit.rect.x, hit.rect.y + hit.rect.height + 2.f, hit.rect.width, divider());
        }
    } else if (page_ == Page::Credits) {
        renderPageHeader("System", "AI: Free / Paid",
                         "Free runs NPCs on the local model. Paid uses Claude Haiku and "
                         "spends credits per reply.");
        const bool paid = credits_.paid && credits_.paid();
        const std::string unavailable =
            credits_.paidUnavailable ? credits_.paidUnavailable() : std::string();
        const float y = kContentTop + 8.f;
        for (const Hit& hit : layout()) {
            if (hit.id == kIdTierFree || hit.id == kIdTierPaid) {
                const bool isPaidCard = hit.id == kIdTierPaid;
                const bool on = isPaidCard == paid;
                ui::card(hit.rect);
                if (hovered(hit.rect)) DrawRectangleRec(hit.rect, ui::alpha(ui::kAccent, 0.08f));
                ui::glowDot(Vector2{hit.rect.x + 24.f, hit.rect.y + 32.f}, on);
                ui::text(Face::Display, isPaidCard ? "Paid - Claude Haiku" : "Free - local model",
                         hit.rect.x + 44.f, hit.rect.y + 20.f, 20.f,
                         on ? ui::kText : ui::kNeutral300);
                const char* blurb = !isPaidCard           ? "Ollama on this machine. No cost."
                                    : unavailable.empty() ? "Charged per reply from your balance."
                                                          : "Not available right now:";
                ui::text(Face::Ui, blurb, hit.rect.x + 44.f, hit.rect.y + 52.f, 13.f,
                         ui::kNeutral500);
                if (isPaidCard && !unavailable.empty()) {
                    ui::text(Face::Ui, unavailable, hit.rect.x + 44.f, hit.rect.y + 70.f, 12.f,
                             ui::kNeutral300);
                }
                if (on) {
                    ui::tag("Current",
                            hit.rect.x + hit.rect.width - 12.f - ui::tagWidth("Current"),
                            hit.rect.y + hit.rect.height - 30.f);
                }
            } else if (hit.id >= kIdBuyBase &&
                       hit.id < kIdBuyBase + static_cast<int>(std::size(kBuyAmountsUsd))) {
                ui::button(hit.rect,
                           "Buy $" + std::to_string(kBuyAmountsUsd[hit.id - kIdBuyBase]),
                           ui::ButtonKind::Secondary, hovered(hit.rect));
            }
        }
        const float balanceY = y + 96.f + 18.f;
        ui::text(Face::Display, "Balance  " + (credits_.balance ? credits_.balance() : "?"),
                 kPad, balanceY, 20.f, ui::kText);
        float lineY = balanceY + 38.f + kButtonH + 12.f;
        ui::text(Face::Ui, "Test credits - no money is charged.", kPad, lineY, 13.f,
                 ui::kNeutral500);
        lineY += 30.f;
        const std::vector<std::string> lines =
            credits_.details ? credits_.details() : std::vector<std::string>{};
        // Tab-separated cells line up in fixed columns; the UI font is
        // proportional, so padding with spaces would not.
        constexpr float kColumnX[] = {0.f, 200.f, 340.f};
        for (const std::string& line : lines) {
            std::size_t start = 0;
            for (const float columnX : kColumnX) {
                const std::size_t tab = line.find('\t', start);
                ui::text(Face::Ui, line.substr(start, tab - start), kPad + columnX, lineY, 13.f,
                         ui::kNeutral300);
                if (tab == std::string::npos) break;
                start = tab + 1;
            }
            lineY += 20.f;
        }
    } else if (page_ == Page::Match) {
        renderPageHeader("Play", "Match Settings",
                         "Applies to the NEXT mystery, not one already running.");
        const int days = matchSettings_.dayLimit ? matchSettings_.dayLimit() : 0;
        const int minutes = matchSettings_.dayMinutes ? matchSettings_.dayMinutes() : 0;
        const char* labels[2] = {"Days", "Day length"};
        const std::string values[2] = {std::to_string(days),
                                       std::to_string(minutes) + " min"};
        float y = kContentTop + 8.f;
        for (int i = 0; i < 2; ++i) {
            const Rectangle c{kPad, y, 480.f, 72.f};
            ui::card(c);
            ui::text(Face::Display, labels[i], c.x + 24.f, c.y + 22.f, 22.f, ui::kText);
            ui::textCentered(Face::Ui, values[i], c.x + 480.f - 24.f - 72.f, c.y + 29.f, 15.f,
                             ui::kText);
            y += 88.f;
        }
        for (const Hit& hit : layout()) {
            if (hit.id == kIdDaysDown || hit.id == kIdLengthDown) {
                ui::button(hit.rect, "-", ui::ButtonKind::Secondary, hovered(hit.rect));
            } else if (hit.id == kIdDaysUp || hit.id == kIdLengthUp) {
                ui::button(hit.rect, "+", ui::ButtonKind::Secondary, hovered(hit.rect));
            }
        }
        ui::text(Face::Ui, "Dusk falls as the day runs out - the sky is the timer.", kPad,
                 y + 8.f, 13.f, ui::kNeutral500);
    }

    if (!toast_.empty()) ui::toast(toast_, h - 84.f);
}

std::optional<Menu::Page> Menu::pageFromName(const std::string& name) {
    // One row per Page. A page added without a row here simply cannot be
    // captured, which is the failure this whole issue is about, so the list
    // lives next to the enum rather than somewhere a reader has to go find.
    if (name == "main") return Page::Main;
    if (name == "controls") return Page::Controls;
    if (name == "multiplayer") return Page::Multiplayer;
    if (name == "creator") return Page::Creator;
    if (name == "journal") return Page::Journal;
    if (name == "sandbox") return Page::Sandbox;
    if (name == "model") return Page::Model;
    if (name == "match") return Page::Match;
    if (name == "credits") return Page::Credits;
    return std::nullopt;
}

}  // namespace llm_npc
