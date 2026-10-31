#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace llm_npc {

// On-disk cache of synthesized lines, so a repeated line costs no API call and
// every player hears the identical take. One file per entry under `dir`, named
// by the key; least recently used files are evicted once the directory exceeds
// `maxBytes`. All methods are safe to call from one worker thread; the cache
// keeps no in-memory index, so a second process sharing the directory is fine.
class TtsCache {
   public:
    TtsCache(std::filesystem::path dir, std::uint64_t maxBytes);

    // Hex FNV-1a 64 over every input that changes the audio. Pure.
    static std::string key(const std::string& text, const std::string& voiceId,
                           const std::string& modelId, const std::string& outputFormat);

    // The cached bytes for `key`, marking the entry as just used; nullopt on a
    // miss or an unreadable file.
    std::optional<std::string> get(const std::string& key);

    // Stores `bytes` under `key` and evicts the least recently used entries
    // until the directory fits the cap. Returns false if the file could not be
    // written (the line still plays; it just is not cached).
    bool put(const std::string& key, const std::string& bytes);

    // Deletes the entry for `key` (a corrupt file the client refused to play).
    // Returns false when there was nothing to delete.
    bool remove(const std::string& key);

    // Sum of the sizes of every entry on disk.
    std::uint64_t sizeBytes() const;

    const std::filesystem::path& dir() const { return dir_; }

   private:
    std::filesystem::path pathFor(const std::string& key) const;
    void evictToFit();

    std::filesystem::path dir_;
    std::uint64_t maxBytes_;
};

}  // namespace llm_npc
