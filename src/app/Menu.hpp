#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

#include "CharacterParts.hpp"
#include "KeyBindings.hpp"

namespace llm_npc {

// What the main loop should do after the menu handled this frame's input.
enum class MenuResult {
    None,    // keep showing the menu
    Resume,  // close the menu and return to the game
    Quit,    // close the window
};

// Mouse-driven pause menu: Resume / Controls / Multiplayer / Quit on the main
// page, a Controls page where clicking a key chip arms capture of the next
// key press, and a Multiplayer page for hosting or joining a session.
// Rebinds use KeyBindings swap semantics and are saved to disk immediately.
// raylib is polled, so update() handles input and returns the result each
// frame; render() draws.
class Menu {
   public:
    // Callbacks the Multiplayer page drives, injected by main.cpp so the
    // menu stays free of networking types. onHost/onJoin return "" on
    // success or a human-readable error shown as a toast.
    struct MultiplayerHooks {
        std::function<std::string(int port)> onHost;
        std::function<std::string(const std::string& address)> onJoin;
        std::function<void()> onLeave;
        std::function<std::string()> status;  // one-line session summary
        std::function<bool()> active;         // hosting or joined right now
    };

    // Callback the Creator page drives; injected by main.cpp so the menu
    // stays free of world/storage types. Returns "" on success (character
    // saved and spawned) or a human-readable error shown as a toast.
    struct CreatorHooks {
        std::function<std::string(const std::string& name, const std::string& backstory,
                                  const std::string& traits, const CharacterLook& look)>
            onCreate;
    };

    // One display row of the player's journal, pre-rendered by main.cpp
    // from the shared fact store so the menu never touches WorldState.
    struct JournalRow {
        std::string subject;      // normalized topic key ("bakery_fire")
        std::string content;      // what was said
        std::string attribution;  // "heard from Marge at 09:30"
        bool conflicting = false; // clashes with another row on the subject
    };
    struct JournalHooks {
        std::function<std::vector<JournalRow>()> entries;
    };

    // `bindings` is shared with the main loop; `savePath` is where every
    // accepted rebind is persisted.
    Menu(KeyBindings& bindings, std::filesystem::path savePath);

    // Installs the Multiplayer page's callbacks; without them the page
    // shows nothing actionable (solo-only build of the menu still works).
    void setMultiplayer(MultiplayerHooks hooks);

    // Installs the Creator page's save callback.
    void setCreator(CreatorHooks hooks);

    // The player's own avatar (issue #106): the SAME creator page opens in
    // avatar mode from "Edit My Avatar" — same picker, same Randomize, same
    // preview — but Save writes the avatar instead of spawning an NPC.
    // `current` seeds the draft; `onSave` returns "" or a toast-able error.
    struct AvatarHooks {
        std::function<CharacterLook()> current;
        std::function<std::string(const CharacterLook&)> onSave;
    };
    void setAvatar(AvatarHooks hooks);

    // Sandbox page (issue #112): lists saved maps; clicking one (or New)
    // hands the map's file stem ("" = create new) to main, which switches
    // into the editor. The menu never touches map files itself.
    struct SandboxHooks {
        std::function<std::vector<std::string>()> listMaps;
        std::function<void(const std::string& stemOrEmpty)> onOpen;
    };
    void setSandbox(SandboxHooks hooks);

    // Installs the Journal page's read hook (pure read path — the journal
    // owns no data and never writes).
    void setJournal(JournalHooks hooks);

    // The draft look while the Creator page is open, nullptr otherwise.
    // main.cpp renders it as an in-world preview in front of the camera that
    // the player rotates by dragging/arrow keys and that eases back to facing
    // the camera when idle (the overlay dims less on that page so it reads).
    const CharacterLook* creatorPreview() const {
        return page_ == Page::Creator ? &draftLook_ : nullptr;
    }

    // True when `p` lies over any interactive control on the current page (a
    // layout() hit-rect). main.cpp uses this so a left-drag that starts over
    // the creator preview rotates the figure, while a press over a control
    // stays a click — clicking a cycler/button never spins the preview.
    bool pointOverInteractive(Vector2 p) const;

    // Resets to the main page (called when the menu is opened).
    void open();

    // True while the Controls page is waiting for a key press to bind.
    bool capturingKey() const { return awaiting_.has_value(); }

    // Handles this frame's mouse/keyboard input and ages the toast.
    // Escape closes the menu from the main page, backs out of a sub-page,
    // or cancels an armed capture — in that priority order.
    MenuResult update(float dt);

    // Draws the menu over the dimmed frame.
    void render() const;

   private:
    enum class Page { Main, Controls, Multiplayer, Creator, Journal, Sandbox };

    // A clickable rectangle paired with what clicking it means.
    struct Hit {
        Rectangle rect;
        int id = 0;  // page-specific meaning, see layout helpers
    };

    KeyBindings& bindings_;
    std::filesystem::path savePath_;
    Page page_ = Page::Main;
    std::optional<Action> awaiting_;
    std::string toast_;
    float toastLeft_ = 0.f;

    MultiplayerHooks multiplayer_;
    std::string joinAddress_ = "127.0.0.1:40605";
    bool editingAddress_ = false;  // typed characters go into joinAddress_

    // ---- Creator page state ----
    CreatorHooks creator_;
    AvatarHooks avatar_;
    SandboxHooks sandbox_;
    std::vector<std::string> sandboxMaps_;  // stems, refreshed on page entry
    bool avatarMode_ = false;  // Creator page writes the avatar, not an NPC
    std::string creatorName_;
    std::string creatorBackstory_;
    std::string creatorTraits_;
    int editingField_ = 0;  // 0 none, 1 name, 2 backstory, 3 traits
    CharacterLook draftLook_ = randomizeLook(7);
    unsigned creatorSeed_ = 7;

    // Steps the draft's part for `category` by ±1 within the options
    // compatible with the current body style; cycling the body re-validates
    // the other categories.
    void cycleCreatorPart(PartCategory category, int direction);

    // Validates and fires onCreate; toasts the outcome and clears the form
    // on success.
    void attemptCreate();

    // ---- Journal page state ----
    JournalHooks journal_;
    int journalPage_ = 0;  // paging offset, kJournalRowsPerPage rows each

    // Clickable areas for the current page, derived from the window size so
    // render() and update() always agree.
    std::vector<Hit> layout() const;

    // Applies a captured raylib key code to the armed action; announces swaps.
    void applyCapture(int key);

    // Validates joinAddress_ and fires onJoin; toasts the outcome.
    void attemptJoin();

    // Shows a short status message near the bottom of the screen.
    void showToast(std::string text);
};

}  // namespace llm_npc
