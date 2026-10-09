#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "raylib.h"

#include "TtsClient.hpp"

namespace llm_npc {

// NPC voice playback (plan: elevenlabs-tts-backend). Owns the TtsClient and the
// raylib audio device; the device is opened only when config/tts.cfg names a
// provider and a key was found, so the default build never touches audio.
// Construct after InitWindow. raylib's audio device is independent of the
// window, so destroying this after CloseWindow (as main does) is fine.
class Speech {
   public:
    explicit Speech(const std::filesystem::path& projectRoot);
    ~Speech();

    Speech(const Speech&) = delete;
    Speech& operator=(const Speech&) = delete;

    // Queues `text` for `npcId` (the persona name). A no-op for NPCs without a
    // voices.cfg entry and whenever voices are off.
    void speak(const std::string& npcId, const std::string& text);

    // Once per frame: starts playback of lines that finished synthesizing and
    // unloads sounds that have finished playing.
    void update();

    // True when a provider is configured, a key was found and a voice map exists.
    bool enabled() const { return client_ && client_->enabled(); }

   private:
    std::unique_ptr<TtsClient> client_;
    bool openedAudio_ = false;
    std::vector<Sound> playing_;
};

}  // namespace llm_npc
