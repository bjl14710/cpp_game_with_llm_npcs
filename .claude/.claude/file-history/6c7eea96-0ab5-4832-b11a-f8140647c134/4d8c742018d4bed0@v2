// Tests for KeyBindings: defaults, rebinding with swap, persistence, dedupe.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "KeyBindings.hpp"
#include "doctest.h"

namespace fs = std::filesystem;
using llm_npc::Action;
using llm_npc::KeyBindings;

namespace {

// Temp file path helper; the file is removed on destruction.
struct TempFile {
    fs::path path;
    TempFile() {
        path = fs::temp_directory_path() /
               ("llm_npc_kb_" + std::to_string(std::rand()) + ".cfg");
    }
    ~TempFile() { std::error_code ec; fs::remove(path, ec); }
    void write(const std::string& content) const {
        std::ofstream out(path);
        out << content;
    }
};

}  // namespace

TEST_CASE("defaults are WASD + T + Escape") {
    KeyBindings kb = KeyBindings::defaults();
    CHECK(kb.key(Action::MoveForward) == "W");
    CHECK(kb.key(Action::MoveBackward) == "S");
    CHECK(kb.key(Action::StrafeLeft) == "A");
    CHECK(kb.key(Action::StrafeRight) == "D");
    CHECK(kb.key(Action::Talk) == "T");
    CHECK(kb.key(Action::OpenMenu) == "Escape");
}

TEST_CASE("rebind to a free key returns no displaced action") {
    KeyBindings kb = KeyBindings::defaults();
    auto displaced = kb.rebind(Action::Talk, "G");
    CHECK_FALSE(displaced.has_value());
    CHECK(kb.key(Action::Talk) == "G");
    CHECK(kb.actionForKey("G") == Action::Talk);
}

TEST_CASE("rebind to a taken key swaps the two actions") {
    KeyBindings kb = KeyBindings::defaults();
    auto displaced = kb.rebind(Action::Talk, "W");  // W belongs to MoveForward
    REQUIRE(displaced.has_value());
    CHECK(*displaced == Action::MoveForward);
    CHECK(kb.key(Action::Talk) == "W");
    CHECK(kb.key(Action::MoveForward) == "T");  // inherited Talk's old key
}

TEST_CASE("rebind is case-insensitive for single letters and no-ops on same key") {
    KeyBindings kb = KeyBindings::defaults();
    CHECK_FALSE(kb.rebind(Action::Talk, "t").has_value());  // same key, lowercase
    CHECK(kb.key(Action::Talk) == "T");
    kb.rebind(Action::Talk, "g");
    CHECK(kb.key(Action::Talk) == "G");
}

TEST_CASE("save then load round-trips all bindings") {
    TempFile tmp;
    KeyBindings kb = KeyBindings::defaults();
    kb.rebind(Action::Talk, "F");
    kb.rebind(Action::OpenMenu, "M");
    REQUIRE(kb.save(tmp.path));

    KeyBindings loaded;
    REQUIRE(loaded.load(tmp.path));
    CHECK(loaded.key(Action::Talk) == "F");
    CHECK(loaded.key(Action::OpenMenu) == "M");
    CHECK(loaded.key(Action::MoveForward) == "W");
}

TEST_CASE("load keeps defaults for missing keys and missing file") {
    TempFile tmp;
    tmp.write("talk = Y\n");
    KeyBindings kb;
    REQUIRE(kb.load(tmp.path));
    CHECK(kb.key(Action::Talk) == "Y");
    CHECK(kb.key(Action::MoveForward) == "W");

    KeyBindings missing;
    CHECK_FALSE(missing.load("/nonexistent/kb.cfg"));
    CHECK(missing.key(Action::MoveForward) == "W");  // defaults intact
}

TEST_CASE("load resolves duplicate keys deterministically") {
    TempFile tmp;
    // Both forward and talk claim W; forward is earlier in enum order.
    tmp.write("move_forward = W\ntalk = W\n");
    KeyBindings kb;
    REQUIRE(kb.load(tmp.path));
    CHECK(kb.key(Action::MoveForward) == "W");
    CHECK(kb.key(Action::Talk) == "T");  // fell back to its default
}
