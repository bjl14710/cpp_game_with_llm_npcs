// Tests for the key=value config reader and LLM config loading.
#include <cstdio>
#include <cstdlib>
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
    unsetenv("OPENROUTER_API_KEY");  // don't let the ambient env leak in
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.host == "localhost");
    CHECK(cfg.port == 11434);
    CHECK_FALSE(cfg.model.empty());
    CHECK(cfg.provider == "ollama");
    CHECK(cfg.baseUrl.empty());
    CHECK(cfg.apiKeyEnv == "OPENROUTER_API_KEY");
    CHECK(cfg.apiKey.empty());
}

TEST_CASE("loadLlmConfig reads provider/base_url/api_key_env") {
    TempDir tmp;
    tmp.write("llm.cfg",
              "provider = openrouter\n"
              "base_url = https://openrouter.ai/api/v1\n"
              "api_key_env = LLM_NPC_TEST_KEY\n"
              "model = anthropic/claude-haiku-4.5\n");
    unsetenv("LLM_NPC_TEST_KEY");
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.provider == "openrouter");
    CHECK(cfg.baseUrl == "https://openrouter.ai/api/v1");
    CHECK(cfg.apiKeyEnv == "LLM_NPC_TEST_KEY");
    CHECK(cfg.model == "anthropic/claude-haiku-4.5");
}

TEST_CASE("loadLlmConfig resolves the API key from the env var, which wins") {
    TempDir tmp;
    tmp.write("llm.cfg", "api_key_env = LLM_NPC_TEST_KEY\n");
    tmp.write("secrets.cfg", "api_key = from-secrets\n");
    setenv("LLM_NPC_TEST_KEY", "from-env", /*overwrite=*/1);
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.apiKey == "from-env");
    unsetenv("LLM_NPC_TEST_KEY");
}

TEST_CASE("loadLlmConfig falls back to secrets.cfg when the env var is unset") {
    TempDir tmp;
    tmp.write("llm.cfg", "api_key_env = LLM_NPC_TEST_KEY\n");
    tmp.write("secrets.cfg", "api_key = from-secrets\n");
    unsetenv("LLM_NPC_TEST_KEY");
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.apiKey == "from-secrets");
}

TEST_CASE("loadLlmConfig reads the think toggle and defaults to omitted") {
    TempDir tmp;
    tmp.write("llm.cfg", "model = qwen3:8b\nthink = false\n");
    auto cfg = llm_npc::loadLlmConfig(tmp.path);
    CHECK(cfg.think == "false");

    TempDir empty;
    CHECK(llm_npc::loadLlmConfig(empty.path).think.empty());
}

TEST_CASE("setKvValue rewrites the matching line and preserves the rest") {
    TempDir tmp;
    tmp.write("llm.cfg",
              "# heading comment\n"
              "host = localhost\n"
              "model = old:tag\n"
              "keep_alive = 10m\n");
    REQUIRE(llm_npc::setKvValue(tmp.path / "llm.cfg", "model", "new:tag"));
    auto kv = llm_npc::readKv(tmp.path / "llm.cfg");
    CHECK(kv["model"] == "new:tag");
    CHECK(kv["host"] == "localhost");  // other keys untouched
    CHECK(kv["keep_alive"] == "10m");
    // The comment survives (readKv strips comments, so check the raw text).
    CHECK(llm_npc::slurp(tmp.path / "llm.cfg").find("# heading comment") != std::string::npos);
}

TEST_CASE("setKvValue appends a key that isn't present yet") {
    TempDir tmp;
    tmp.write("llm.cfg", "host = localhost\n");
    REQUIRE(llm_npc::setKvValue(tmp.path / "llm.cfg", "model", "tiny:1b"));
    auto kv = llm_npc::readKv(tmp.path / "llm.cfg");
    CHECK(kv["host"] == "localhost");
    CHECK(kv["model"] == "tiny:1b");
}

TEST_CASE("setKvValue ignores commented and longer-prefixed keys") {
    TempDir tmp;
    tmp.write("llm.cfg",
              "# model = commented\n"
              "model_extra = leaveme\n"
              "model = real\n");
    REQUIRE(llm_npc::setKvValue(tmp.path / "llm.cfg", "model", "changed"));
    auto kv = llm_npc::readKv(tmp.path / "llm.cfg");
    CHECK(kv["model"] == "changed");
    CHECK(kv["model_extra"] == "leaveme");  // prefix collision not touched
    CHECK(llm_npc::slurp(tmp.path / "llm.cfg").find("# model = commented") != std::string::npos);
}

TEST_CASE("parseOnOff accepts the usual spellings and falls back otherwise") {
    using llm_npc::parseOnOff;
    for (const char* yes : {"on", "ON", " true ", "Yes", "1"}) {
        CAPTURE(yes);
        CHECK(parseOnOff(yes, false));
    }
    for (const char* no : {"off", "Off", "false", "NO", "0"}) {
        CAPTURE(no);
        CHECK_FALSE(parseOnOff(no, true));
    }
    CHECK(parseOnOff("maybe", true));
    CHECK_FALSE(parseOnOff("", false));
}

TEST_CASE("loadGraphicsConfig defaults both noir switches on") {
    TempDir tmp;  // no graphics.cfg at all
    const llm_npc::GraphicsConfig cfg = llm_npc::loadGraphicsConfig(tmp.path);
    CHECK(cfg.noirNight);
    CHECK(cfg.rain);
}

TEST_CASE("loadGraphicsConfig reads noir_night and rain") {
    TempDir tmp;
    tmp.write("graphics.cfg",
              "# visuals\n"
              "noir_night = off\n"
              "rain = on  # drizzle\n");
    llm_npc::GraphicsConfig cfg = llm_npc::loadGraphicsConfig(tmp.path);
    CHECK_FALSE(cfg.noirNight);
    CHECK(cfg.rain);

    // A persisted menu toggle round-trips through setKvValue.
    REQUIRE(llm_npc::setKvValue(tmp.path / "graphics.cfg", "rain", "off"));
    cfg = llm_npc::loadGraphicsConfig(tmp.path);
    CHECK_FALSE(cfg.rain);
    CHECK_FALSE(cfg.noirNight);

    // Garbage keeps the default rather than silently disabling the look.
    tmp.write("graphics.cfg", "noir_night = sometimes\n");
    CHECK(llm_npc::loadGraphicsConfig(tmp.path).noirNight);
}

TEST_CASE("loadLlmConfig keeps the default for a non-numeric value instead of throwing") {
    const llm_npc::LlmConfig defaults;
    for (const char* bad : {"port = abc", "port =", "port = 12x", "port = 99999999999"}) {
        CAPTURE(bad);
        TempDir tmp;
        tmp.write("llm.cfg", std::string(bad) + "\n");
        llm_npc::LlmConfig cfg;
        CHECK_NOTHROW(cfg = llm_npc::loadLlmConfig(tmp.path));
        CHECK(cfg.port == defaults.port);
    }

    TempDir tmp;
    tmp.write("llm.cfg",
              "line_bank_threshold = high\n"
              "line_bank_cps = fast\n"
              "request_timeout_s = nan\n"
              "temperature = nan\n"
              "port = 8080\n");
    llm_npc::LlmConfig cfg;
    REQUIRE_NOTHROW(cfg = llm_npc::loadLlmConfig(tmp.path));
    CHECK(cfg.lineBankThreshold == doctest::Approx(defaults.lineBankThreshold));
    CHECK(cfg.lineBankCps == defaults.lineBankCps);
    CHECK(cfg.requestTimeoutSeconds == defaults.requestTimeoutSeconds);
    CHECK(cfg.temperature == doctest::Approx(defaults.temperature));
    CHECK(cfg.port == 8080);  // a good neighbour is still applied
}

TEST_CASE("readKv ignores a leading UTF-8 BOM") {
    TempDir tmp;
    tmp.write("a.cfg", "\xEF\xBB\xBF" "host = example\nport = 1\n");
    auto kv = llm_npc::readKv(tmp.path / "a.cfg");
    CHECK(kv.size() == 2);
    CHECK(kv["host"] == "example");
    CHECK(kv["port"] == "1");
}

TEST_CASE("readKv keeps a '#' inside a value but still strips comments") {
    TempDir tmp;
    tmp.write("a.cfg",
              "api_key = sk-abc#def\n"
              "x = 1 # note\n"
              "y = 2\t# tabbed note\n"
              "#z = 3\n");
    auto kv = llm_npc::readKv(tmp.path / "a.cfg");
    CHECK(kv["api_key"] == "sk-abc#def");
    CHECK(kv["x"] == "1");
    CHECK(kv["y"] == "2");
    CHECK(kv.count("z") == 0);
}

TEST_CASE("setKvValue refuses values with line breaks and leaves the file alone") {
    const std::string original = "host = localhost\nmodel = old\n";
    for (const char* hostile : {"evil\nbase_url = http://attacker", "evil\rx", "a\r\nb"}) {
        CAPTURE(hostile);
        TempDir tmp;
        tmp.write("llm.cfg", original);
        CHECK_FALSE(llm_npc::setKvValue(tmp.path / "llm.cfg", "model", hostile));
        CHECK(llm_npc::slurp(tmp.path / "llm.cfg") == original);
        CHECK_FALSE(llm_npc::setKvValue(tmp.path / "llm.cfg", "mo\ndel", "ok"));
        CHECK(llm_npc::slurp(tmp.path / "llm.cfg") == original);
    }
}

TEST_CASE("setKvValue leaves no temp file behind and fails cleanly when it cannot write") {
    TempDir tmp;
    tmp.write("llm.cfg", "model = old\n");
    REQUIRE(llm_npc::setKvValue(tmp.path / "llm.cfg", "model", "new"));
    int files = 0;
    for (const auto& e : fs::directory_iterator(tmp.path)) {
        (void)e;
        ++files;
    }
    CHECK(files == 1);

    // A directory that does not exist: the temp file cannot be created.
    CHECK_FALSE(llm_npc::setKvValue(tmp.path / "missing_dir" / "llm.cfg", "model", "x"));
}
