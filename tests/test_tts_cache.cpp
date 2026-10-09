// TtsCache: hit/miss, the four-input key, and LRU eviction under the cap.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "TtsCache.hpp"
#include "doctest.h"

using namespace llm_npc;
namespace fs = std::filesystem;

namespace {

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("llm_npc_tts_cache_" + std::to_string(std::rand()));
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
};

}  // namespace

TEST_CASE("the key changes when any of text, voice, model or format changes") {
    const std::string base = TtsCache::key("hello", "voice-a", "eleven_flash_v2_5", "pcm_22050");
    CHECK(base.size() == 16);
    CHECK(base == TtsCache::key("hello", "voice-a", "eleven_flash_v2_5", "pcm_22050"));
    CHECK(base != TtsCache::key("hello!", "voice-a", "eleven_flash_v2_5", "pcm_22050"));
    CHECK(base != TtsCache::key("hello", "voice-b", "eleven_flash_v2_5", "pcm_22050"));
    CHECK(base != TtsCache::key("hello", "voice-a", "eleven_multilingual_v2", "pcm_22050"));
    CHECK(base != TtsCache::key("hello", "voice-a", "eleven_flash_v2_5", "pcm_24000"));
    // Field boundaries are part of the hash: shifting a character across one
    // is a different key.
    CHECK(TtsCache::key("ab", "c", "", "") != TtsCache::key("a", "bc", "", ""));
    for (const char c : base) CHECK(std::string("0123456789abcdef").find(c) != std::string::npos);
}

TEST_CASE("a miss is nullopt and a put is read back byte for byte") {
    TempDir tmp;
    TtsCache cache(tmp.path / "nested" / "dir", 1024 * 1024);
    const std::string key = TtsCache::key("x", "v", "m", "f");
    CHECK_FALSE(cache.get(key).has_value());

    std::string bytes = "RIFF";
    bytes.push_back('\0');
    bytes += "binary\xff\xfe";
    REQUIRE(cache.put(key, bytes));
    const auto hit = cache.get(key);
    REQUIRE(hit.has_value());
    CHECK(*hit == bytes);
    CHECK(cache.sizeBytes() == bytes.size());
    CHECK_FALSE(fs::exists(tmp.path / "nested" / "dir" / (key + ".part")));
}

TEST_CASE("the least recently used entries are evicted once the cap is exceeded") {
    TempDir tmp;
    TtsCache cache(tmp.path, 250);  // room for two 100-byte entries, not three
    const std::string a = TtsCache::key("a", "v", "m", "f");
    const std::string b = TtsCache::key("b", "v", "m", "f");
    const std::string c = TtsCache::key("c", "v", "m", "f");
    const std::string hundred(100, 'z');

    REQUIRE(cache.put(a, hundred));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(cache.put(b, hundred));
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    // Touch a so b is the oldest.
    REQUIRE(cache.get(a).has_value());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    REQUIRE(cache.put(c, hundred));

    CHECK(cache.sizeBytes() <= 250);
    CHECK(cache.get(a).has_value());
    CHECK_FALSE(cache.get(b).has_value());
    CHECK(cache.get(c).has_value());
}

TEST_CASE("an unwritable directory makes put fail without throwing") {
    // A directory under a regular file cannot be created by anyone, root
    // included, so this fails for the right reason on every machine.
    TempDir tmp;
    const fs::path blocker = tmp.path / "not-a-dir";
    { std::ofstream(blocker) << "file"; }
    TtsCache cache(blocker / "x", 1024);
    CHECK_FALSE(cache.put("k", "bytes"));
    CHECK_FALSE(cache.get("k").has_value());
    CHECK(cache.sizeBytes() == 0);
}

TEST_CASE("remove deletes an entry and reports whether there was one") {
    TempDir tmp;
    TtsCache cache(tmp.path, 1024);
    REQUIRE(cache.put("k", "bytes"));
    CHECK(cache.remove("k"));
    CHECK_FALSE(cache.get("k").has_value());
    CHECK_FALSE(cache.remove("k"));
}
