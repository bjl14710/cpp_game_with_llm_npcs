#include "TtsCache.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

namespace llm_npc {

namespace fs = std::filesystem;

namespace {

constexpr const char* kExt = ".wav";

void fnv1a(std::uint64_t& hash, const std::string& s) {
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    for (const char c : s) {
        hash ^= static_cast<unsigned char>(c);
        hash *= kPrime;
    }
    // A separator that cannot occur in valid UTF-8, so "ab"+"c" and "a"+"bc"
    // hash differently.
    hash ^= 0xFF;
    hash *= kPrime;
}

}  // namespace

TtsCache::TtsCache(fs::path dir, std::uint64_t maxBytes)
    : dir_(std::move(dir)), maxBytes_(maxBytes) {
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) std::cerr << "[llm_npc] tts: cannot create cache dir " << dir_ << ": " << ec.message() << "\n";
}

std::string TtsCache::key(const std::string& text, const std::string& voiceId,
                          const std::string& modelId, const std::string& outputFormat) {
    std::uint64_t hash = 14695981039346656037ULL;  // FNV-1a 64 offset basis
    fnv1a(hash, text);
    fnv1a(hash, voiceId);
    fnv1a(hash, modelId);
    fnv1a(hash, outputFormat);
    std::string hex(16, '0');
    for (int i = 15; i >= 0; --i) {
        hex[static_cast<std::size_t>(i)] = "0123456789abcdef"[hash & 0xF];
        hash >>= 4;
    }
    return hex;
}

fs::path TtsCache::pathFor(const std::string& key) const { return dir_ / (key + kExt); }

std::optional<std::string> TtsCache::get(const std::string& key) {
    const fs::path path = pathFor(key);
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return std::nullopt;
    // The mtime is the LRU clock; a hit refreshes it.
    std::error_code ec;
    fs::last_write_time(path, fs::file_time_type::clock::now(), ec);
    return bytes;
}

bool TtsCache::put(const std::string& key, const std::string& bytes) {
    const fs::path path = pathFor(key);
    // Write to a sibling and rename, so a crash mid-write never leaves a
    // half file that get() would hand to the audio device.
    const fs::path tmp = dir_ / (key + ".part");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    evictToFit();
    return true;
}

bool TtsCache::remove(const std::string& key) {
    std::error_code ec;
    return fs::remove(pathFor(key), ec);
}

std::uint64_t TtsCache::sizeBytes() const {
    std::uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (entry.path().extension() != kExt) continue;
        total += entry.file_size(ec);
    }
    return total;
}

void TtsCache::evictToFit() {
    struct Entry {
        fs::path path;
        fs::file_time_type used;
        std::uint64_t size;
    };
    std::vector<Entry> entries;
    std::uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir_, ec)) {
        if (entry.path().extension() != kExt) continue;
        const std::uint64_t size = entry.file_size(ec);
        entries.push_back({entry.path(), entry.last_write_time(ec), size});
        total += size;
    }
    if (total <= maxBytes_) return;
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.used < b.used; });
    for (const Entry& entry : entries) {
        if (total <= maxBytes_) break;
        if (fs::remove(entry.path, ec)) total -= entry.size;
    }
}

}  // namespace llm_npc
