#include "Speech.hpp"

#include <iostream>
#include <utility>

#include "TtsBackend.hpp"
#include "TtsConfig.hpp"

namespace llm_npc {

Speech::Speech(const std::filesystem::path& projectRoot) {
    const std::filesystem::path configDir = projectRoot / "config";
    TtsConfig config = loadTtsConfig(configDir);
    std::unique_ptr<TtsBackend> backend = makeTtsBackend(config);
    if (!backend) return;  // provider none or no key: already logged, stay silent

    VoiceMap voices = loadVoiceMap(configDir);
    if (voices.empty()) {
        std::cerr << "[llm_npc] tts: provider = " << config.provider
                  << " but config/voices.cfg names no NPC; NPC voices off\n";
        return;
    }
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        std::cerr << "[llm_npc] tts: no audio device; NPC voices off\n";
        return;
    }
    openedAudio_ = true;
    client_ = std::make_unique<TtsClient>(std::move(config), std::move(voices), std::move(backend));
}

Speech::~Speech() {
    // The worker joins before the sounds and device go, so no result lands on
    // a closed device.
    client_.reset();
    for (const Sound& sound : playing_) UnloadSound(sound);
    if (openedAudio_) CloseAudioDevice();
}

void Speech::speak(const std::string& npcId, const std::string& text) {
    if (client_) client_->submit(npcId, text);
}

void Speech::update() {
    if (!client_) return;
    for (SpeechResult& result : client_->drainReady()) {
        if (!result.ok) continue;  // already logged by the backend; silence
        const Wave wave = LoadWaveFromMemory(
            ".wav", reinterpret_cast<const unsigned char*>(result.wav.data()),
            static_cast<int>(result.wav.size()));
        if (wave.data == nullptr) {
            std::cerr << "[llm_npc] tts: raylib could not decode the audio for " << result.npcId
                      << "\n";
            continue;
        }
        const Sound sound = LoadSoundFromWave(wave);
        UnloadWave(wave);  // LoadSoundFromWave copies the samples
        PlaySound(sound);
        playing_.push_back(sound);
    }
    // Unload before erasing: remove_if would leave the tail unspecified, so the
    // finished sounds are released in place and the vector compacted after.
    std::vector<Sound> stillPlaying;
    stillPlaying.reserve(playing_.size());
    for (const Sound& sound : playing_) {
        if (IsSoundPlaying(sound)) {
            stillPlaying.push_back(sound);
        } else {
            UnloadSound(sound);
        }
    }
    playing_.swap(stillPlaying);
}

}  // namespace llm_npc
