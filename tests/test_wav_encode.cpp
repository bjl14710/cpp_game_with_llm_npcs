// WavEncode: the 44-byte header that turns ElevenLabs raw PCM into something
// raylib's LoadWaveFromMemory(".wav") accepts.
#include <string>

#include "WavEncode.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("wavFromPcm16 writes a canonical 44-byte PCM header followed by the samples") {
    const std::string pcm = {'\x01', '\x02', '\x03', '\x04', '\x05', '\x06'};  // 3 samples
    const std::string wav = wavFromPcm16(pcm, 22050);

    REQUIRE(wav.size() == 44 + pcm.size());
    CHECK(wav.substr(0, 4) == "RIFF");
    CHECK(wav.substr(8, 4) == "WAVE");
    CHECK(wav.substr(12, 4) == "fmt ");
    CHECK(wav.substr(36, 4) == "data");
    CHECK(wav.substr(44) == pcm);

    // RIFF size = 36 + data bytes; fmt chunk size 16; format 1 (PCM).
    CHECK(static_cast<unsigned char>(wav[4]) == 42);
    CHECK(static_cast<unsigned char>(wav[16]) == 16);
    CHECK(static_cast<unsigned char>(wav[20]) == 1);

    const auto info = parseWavHeader(wav);
    REQUIRE(info.has_value());
    CHECK(info->sampleRate == 22050);
    CHECK(info->channels == 1);
    CHECK(info->bitsPerSample == 16);
    CHECK(info->dataBytes == 6);

    // Byte rate and block align for 16-bit mono: rate*2 and 2.
    CHECK(static_cast<unsigned char>(wav[32]) == 2);
    CHECK(static_cast<unsigned char>(wav[33]) == 0);
    const std::uint32_t byteRate = static_cast<unsigned char>(wav[28]) |
                                   (static_cast<unsigned char>(wav[29]) << 8) |
                                   (static_cast<unsigned char>(wav[30]) << 16) |
                                   (static_cast<unsigned char>(wav[31]) << 24);
    CHECK(byteRate == 44100);
}

TEST_CASE("the sample rate round-trips for every rate ElevenLabs offers") {
    for (const std::uint32_t rate : {8000u, 16000u, 22050u, 24000u, 32000u, 44100u, 48000u}) {
        const auto info = parseWavHeader(wavFromPcm16(std::string(4, '\0'), rate));
        REQUIRE(info.has_value());
        CHECK(info->sampleRate == rate);
    }
}

TEST_CASE("stereo is encoded with the right block align") {
    const auto info = parseWavHeader(wavFromPcm16(std::string(8, '\0'), 16000, 2));
    REQUIRE(info.has_value());
    CHECK(info->channels == 2);
    CHECK(info->dataBytes == 8);
}

TEST_CASE("empty PCM yields a header-only file") {
    const std::string wav = wavFromPcm16("", 22050);
    CHECK(wav.size() == 44);
    CHECK(parseWavHeader(wav)->dataBytes == 0);
}

TEST_CASE("parseWavHeader rejects anything that is not a canonical PCM WAV") {
    CHECK_FALSE(parseWavHeader("").has_value());
    CHECK_FALSE(parseWavHeader(std::string(43, 'x')).has_value());
    CHECK_FALSE(parseWavHeader("<html>not audio</html>" + std::string(40, ' ')).has_value());
    std::string notPcm = wavFromPcm16(std::string(4, '\0'), 22050);
    notPcm[20] = 3;  // IEEE float
    CHECK_FALSE(parseWavHeader(notPcm).has_value());
}
