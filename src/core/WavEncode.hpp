#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace llm_npc {

// The fields of a canonical 44-byte RIFF/WAVE header, as read back by
// parseWavHeader so a test (or the cache) can check what a buffer claims to be.
struct WavInfo {
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t dataBytes = 0;
};

namespace detail {

inline void putLe16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

inline void putLe32(std::string& out, std::uint32_t v) {
    putLe16(out, static_cast<std::uint16_t>(v & 0xFFFF));
    putLe16(out, static_cast<std::uint16_t>((v >> 16) & 0xFFFF));
}

inline std::uint16_t getLe16(const std::string& in, std::size_t at) {
    return static_cast<std::uint16_t>(static_cast<unsigned char>(in[at]) |
                                      (static_cast<unsigned char>(in[at + 1]) << 8));
}

inline std::uint32_t getLe32(const std::string& in, std::size_t at) {
    return static_cast<std::uint32_t>(getLe16(in, at)) |
           (static_cast<std::uint32_t>(getLe16(in, at + 2)) << 16);
}

}  // namespace detail

// Wraps raw signed 16-bit little-endian PCM in a 44-byte WAV header so raylib's
// LoadWaveFromMemory(".wav", ...) can play it. Pure: no I/O, no dependencies.
inline std::string wavFromPcm16(const std::string& pcm, std::uint32_t sampleRate,
                                std::uint16_t channels = 1) {
    constexpr std::uint16_t kBits = 16;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * (kBits / 8));
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(pcm.size());

    std::string out;
    out.reserve(44 + pcm.size());
    out += "RIFF";
    detail::putLe32(out, 36 + dataBytes);
    out += "WAVE";
    out += "fmt ";
    detail::putLe32(out, 16);  // PCM fmt chunk size
    detail::putLe16(out, 1);   // audio format 1 = PCM
    detail::putLe16(out, channels);
    detail::putLe32(out, sampleRate);
    detail::putLe32(out, sampleRate * blockAlign);  // byte rate
    detail::putLe16(out, blockAlign);
    detail::putLe16(out, kBits);
    out += "data";
    detail::putLe32(out, dataBytes);
    out += pcm;
    return out;
}

// Reads back a header written by wavFromPcm16 (or any canonical PCM WAV whose
// fmt chunk comes first). nullopt for anything that is not such a file. A
// wav_* reply that carries extra chunks (LIST, fact) before "data" is not
// handled: the default output format is pcm_*, which never goes through here.
inline std::optional<WavInfo> parseWavHeader(const std::string& wav) {
    if (wav.size() < 44) return std::nullopt;
    if (wav.compare(0, 4, "RIFF") != 0 || wav.compare(8, 4, "WAVE") != 0 ||
        wav.compare(12, 4, "fmt ") != 0 || wav.compare(36, 4, "data") != 0) {
        return std::nullopt;
    }
    if (detail::getLe16(wav, 20) != 1) return std::nullopt;  // not plain PCM
    WavInfo info;
    info.channels = detail::getLe16(wav, 22);
    info.sampleRate = detail::getLe32(wav, 24);
    info.bitsPerSample = detail::getLe16(wav, 34);
    info.dataBytes = detail::getLe32(wav, 40);
    return info;
}

}  // namespace llm_npc
