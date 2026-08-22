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
    None,        // keep showing the menu
    Resume,      // close the menu and return to the game
    NewMystery,  // start a detective match, then return to the game
    Quit,        // close the window
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
                                  const std::string& traits, const CharacterLook& look,
                                  const std::string& traitId)>
            onCreate;  // traitId "" = no structured trait picked
    };

    // The structured-trait choices the creator's trait row cycles (issue
    // #117); installed once by main from the loaded library. One trait per
    // created character in the v1 UI — the file format supports three.
    void setTraitChoices(std::vector<std::string> ids);

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

    // Whether a detective match is currently running. Pushed in by main.cpp
    // each frame -- a bool, so the menu still knows nothing about MatchClock
    // or any world type. Used only to refuse a second New Mystery: starting
    // one mid-match would throw away the day, the phase and the elapsed time
    // for a misclick one row above Resume.
    void setMatchActive(bool active) { matchActive_ = active; }

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
        // LLM generation (issue #129): the typed description; main runs
        // the async generate-validate-retry chain and opens the result.
        std::function<std::string(const std::string& description)> onGenerate;
    };
    void setSandbox(SandboxHooks hooks);

    // Model page (ported from the pluggable-llm branch onto the raylib
    // menu): lists the backend's installed models; clicking one switches
    // the live client and persists the choice. Hooks keep the menu free of
    // LlmClient types.
    struct ModelHooks {
        std::function<std::vector<std::string>()> listModels;
        std::function<std::string()> currentModel;
        std::function<std::string(const std::string& model)> onSelect;
    };
    void setModels(ModelHooks hooks);

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

    // The menu's pages, by the name --menu takes on the command line.
    //
    // Public only so a smoke run can open one. Every page but Main is
    // reachable in play by clicking, which a headless --frames capture cannot
    // do — so none of them had a visual-QA path, and that is precisely how a
    // full-screen menu shipped drawn over the map editor (#215) and how every
    // UI capture in this project's history turned out to be missing its UI
    // (#216).
    enum class Page { Main, Controls, Multiplayer, Creator, Journal, Sandbox, Model };

    // Opens `page` directly. Intended for --menu; play reaches pages by
    // clicking, and nothing about that path changes.
    void showPage(Page page) { page_ = page; }

    // The Page for a command-line name ("journal", "controls", ...), or
    // nullopt. Kept beside the enum so a page added without a name is a
    // visible omission rather than a silent one.
    static std::optional<Page> pageFromName(const std::string& name);

   private:

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
    std::string genDescription_;   // sandbox "Generate..." field (issue #129)
    bool editingGen_ = false;

    // ---- Creator page state ----
    CreatorHooks creator_;
    AvatarHooks avatar_;
    SandboxHooks sandbox_;
    ModelHooks models_;
    std::vector<std::string> modelList_;  // refreshed on page entry
    std::vector<std::string> sandboxMaps_;  // stems, refreshed on page entry
    bool avatarMode_ = false;  // Creator page writes the avatar, not an NPC
    bool matchActive_ = false;  // a match is running; New Mystery refuses
    std::string creatorName_;
    std::vector<std::string> traitChoices_;  // cycled by the trait row
    int creatorTraitIndex_ = 0;              // 0 = none; else 1-based into choices
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
