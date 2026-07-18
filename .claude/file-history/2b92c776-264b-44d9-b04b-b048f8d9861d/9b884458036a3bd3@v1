// Tests for the key=value config reader and LLM config loading.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "Config.hpp"
#include "doctest.h"

namespace fs = std::filesystem;

namespace {

// Creates a temp directory unique to this test binary run and writes a file
// into it; removed when the helper goes out of scope.
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("llm_npc_test_" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
    void write(const std::string& name, const std::string& content) const {
        std::ofstream out(path / name);
        out << content;
    }
};

}  // namespace

TEST_CASE("trim strips surrounding whitespace only") {
    CHECK(llm_npc::trim("  a b \t\n") == "a b");
    CHECK(llm_npc::trim("") == "");
    CHECK(llm_npc::trim(" \t ") == "");
}

TEST_CASE("readKv parses values and ignores comments and blanks") {
    TempDir tmp;
    tmp.write("a.cfg",
              "# full-line comment\n"
              "host = localhost  # trailing comment\n"
              "\n"
              "port=11434\n"
              "broken line without equals\n");
    auto kv = llm_npc::readKv(tmp.path / "a.cfg");
    CHECK(kv.size() == 2);
    CHECK(kv["host"] == "localhost");
    CHECK(kv["port"] == "11434");
}

TEST_CASE("readKv returns empty map for a missing file") {
    CHECK(llm_npc::readKv("/nonexistent/path/llm.cfg").empty());
}

TEST_CASE("loadLlmConfig overrides defaults from llm.cfg") {
    TempDir tmp;
    tmp.write("llm.cfg",
              "host = example.org\n"
              "port = 9999\n"
              "model = tiny:1b\n"
              "temperature = 0.25\n"
              "request_timeout_s = 7\n");
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.host == "example.org");
    CHECK(cfg.port == 9999);
    CHECK(cfg.model == "tiny:1b");
    CHECK(cfg.temperature == doctest::Approx(0.25));
    CHECK(cfg.requestTimeoutSeconds == 7);
}

TEST_CASE("loadLlmConfig keeps defaults when llm.cfg is absent") {
    TempDir tmp;  // empty dir
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.host == "localhost");
    CHECK(cfg.port == 11434);
    CHECK_FALSE(cfg.model.empty());
}
