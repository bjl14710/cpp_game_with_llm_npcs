#pragma once

namespace llm_npc {

// Pure day/night curves — core so they are unit-testable; the renderer
// maps them to raylib colors and shader uniforms. Everything is a function
// of the ONE world clock's time-of-day; no consumer runs its own timer.
//
// The day is four bands with linear blends across dawn and dusk:
//   night  [21:00 .. 05:00)
//   dawn   [05:00 .. 07:00)   night -> day
//   day    [07:00 .. 18:00)
//   dusk   [18:00 .. 21:00)   day -> warm -> night (warmth peaks mid-dusk)

// Normalized RGB, 0..1 (the app layer scales to 0..255).
struct SkyColor {
    float r, g, b;
};

namespace daynight {

constexpr float kDawnStart = 5.f, kDawnEnd = 7.f;
constexpr float kDuskStart = 18.f, kDuskEnd = 21.f;

constexpr SkyColor kDaySky{135.f / 255.f, 190.f / 255.f, 235.f / 255.f};
constexpr SkyColor kNightSky{16.f / 255.f, 22.f / 255.f, 44.f / 255.f};
constexpr SkyColor kDuskSky{222.f / 255.f, 136.f / 255.f, 84.f / 255.f};

inline SkyColor lerp(const SkyColor& a, const SkyColor& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

}  // namespace daynight

// Sky (and fog) color for a time of day in hours [0, 24).
inline SkyColor skyColorAt(float hours) {
    using namespace daynight;
    if (hours >= kDawnStart && hours < kDawnEnd) {
        return lerp(kNightSky, kDaySky, (hours - kDawnStart) / (kDawnEnd - kDawnStart));
    }
    if (hours >= kDawnEnd && hours < kDuskStart) return kDaySky;
    if (hours >= kDuskStart && hours < kDuskEnd) {
        // Day -> warm sunset -> night, warmth peaking mid-dusk.
        const float t = (hours - kDuskStart) / (kDuskEnd - kDuskStart);
        return t < 0.5f ? lerp(kDaySky, kDuskSky, t * 2.f)
                        : lerp(kDuskSky, kNightSky, (t - 0.5f) * 2.f);
    }
    return kNightSky;
}

// Scene brightness multiplier for a time of day: 1 in daylight, 0.35 at
// night (dim but readable), blended across the same dawn/dusk windows so
// light never jumps while the sky fades.
inline float lightLevelAt(float hours) {
    using namespace daynight;
    constexpr float kDay = 1.f, kNight = 0.35f;
    if (hours >= kDawnStart && hours < kDawnEnd) {
        return kNight + (kDay - kNight) * (hours - kDawnStart) / (kDawnEnd - kDawnStart);
    }
    if (hours >= kDawnEnd && hours < kDuskStart) return kDay;
    if (hours >= kDuskStart && hours < kDuskEnd) {
        return kDay + (kNight - kDay) * (hours - kDuskStart) / (kDuskEnd - kDuskStart);
    }
    return kNight;
}

}  // namespace llm_npc
