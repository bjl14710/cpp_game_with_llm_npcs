// Tests for the noir look curves and the per-frame nearest-lamp pick.
#include <cmath>

#include "NoirLook.hpp"
#include "doctest.h"

using namespace llm_npc;

TEST_CASE("noir look: noon is the identity look") {
    // Daytime must render exactly as before the noir work: every effect off.
    const NoirLook noon = noirLookAt(12.f);
    CHECK(noon.night == 0.f);
    CHECK(noon.desaturation == 0.f);
    CHECK(noon.contrast == 0.f);
    CHECK(noon.vignette == 0.f);
    CHECK(noon.grain == 0.f);
    CHECK(noon.fogDensityScale == doctest::Approx(1.f));
    CHECK(noon.ambientScale == 1.f);
    CHECK(noon.lampIntensity == 0.f);
    CHECK(noon.wetSpecular == 0.f);
    CHECK(noon.rain == 0.f);
    const SkyColor plain = skyColorAt(12.f);
    CHECK(noon.sky.r == doctest::Approx(plain.r));
    CHECK(noon.sky.g == doctest::Approx(plain.g));
    CHECK(noon.sky.b == doctest::Approx(plain.b));
}

TEST_CASE("noir look: mid-dusk is half-way, midnight is full noir") {
    const NoirLook dusk = noirLookAt(19.5f);
    CHECK(dusk.night == doctest::Approx(0.5f));
    CHECK(dusk.desaturation == doctest::Approx(0.46f));
    CHECK(dusk.fogDensityScale == doctest::Approx(1.4f));
    CHECK(dusk.rain == doctest::Approx(0.5f));
    CHECK(dusk.lampIntensity > 0.f);

    const NoirLook midnight = noirLookAt(0.f);
    CHECK(midnight.night == doctest::Approx(1.f));
    CHECK(midnight.desaturation == doctest::Approx(0.92f));
    CHECK(midnight.fogDensityScale == doctest::Approx(1.8f));
    // Ambient dims between the pools so the lamps carry the scene, but the
    // city never goes fully black (lightLevelAt's 0.35 floor still applies).
    CHECK(midnight.ambientScale < dusk.ambientScale);
    CHECK(midnight.ambientScale >= 0.5f);
    CHECK(midnight.vignette > dusk.vignette);
    CHECK(midnight.grain > dusk.grain);
    CHECK(midnight.wetSpecular > dusk.wetSpecular);
    CHECK(midnight.rain == doctest::Approx(1.f));
    // The reference near-black blue sky (#07090d): blue leads, all dark.
    CHECK(midnight.sky.r == doctest::Approx(7.f / 255.f));
    CHECK(midnight.sky.b > midnight.sky.r);
    CHECK(midnight.sky.b < 0.06f);
    // Warm sodium lamps: red > green > blue.
    CHECK(midnight.lampColor.r > midnight.lampColor.g);
    CHECK(midnight.lampColor.g > midnight.lampColor.b);
}

TEST_CASE("noir look: disabled is today's night exactly") {
    for (const float h : {0.f, 19.5f, 23.f}) {
        CAPTURE(h);
        const NoirLook off = noirLookAt(h, false);
        CHECK(off.night == 0.f);
        CHECK(off.desaturation == 0.f);
        CHECK(off.fogDensityScale == doctest::Approx(1.f));
        CHECK(off.ambientScale == 1.f);
        CHECK(off.lampIntensity == 0.f);
        CHECK(off.rain == 0.f);
        const SkyColor plain = skyColorAt(h);
        CHECK(off.sky.r == doctest::Approx(plain.r));
        CHECK(off.sky.b == doctest::Approx(plain.b));
    }
}

TEST_CASE("noir look blends without popping across dusk") {
    // Small hour steps produce small changes: no frame-to-frame jumps.
    for (float h = 17.5f; h < 21.5f; h += 0.05f) {
        CAPTURE(h);
        const NoirLook a = noirLookAt(h);
        const NoirLook b = noirLookAt(h + 0.05f);
        CHECK(std::abs(a.desaturation - b.desaturation) < 0.03f);
        CHECK(std::abs(a.fogDensityScale - b.fogDensityScale) < 0.03f);
        CHECK(std::abs(a.sky.r - b.sky.r) < 0.05f);
    }
}

TEST_CASE("nearestLamps keeps the closest few, nearest first") {
    const std::vector<Vec3> lamps = {
        {10.f, 4.f, 0.f}, {1.f, 4.f, 0.f}, {50.f, 4.f, 0.f}, {5.f, 4.f, 0.f}};
    const Vec3 eye{0.f, 1.7f, 0.f};
    const std::vector<Vec3> two = nearestLamps(lamps, eye, 2);
    REQUIRE(two.size() == 2);
    CHECK(two[0].x == 1.f);
    CHECK(two[1].x == 5.f);
    // Fewer lamps than the cap: all of them, still sorted.
    const std::vector<Vec3> all = nearestLamps(lamps, eye, 16);
    REQUIRE(all.size() == 4);
    CHECK(all[3].x == 50.f);
    CHECK(nearestLamps({}, eye).empty());
}

TEST_CASE("nearestLamps never exceeds the shader cap") {
    std::vector<Vec3> many;
    for (int i = 0; i < 40; ++i) many.push_back({static_cast<float>(i), 4.f, 0.f});
    CHECK(nearestLamps(many, Vec3{}).size() == noir::kMaxLamps);
}
