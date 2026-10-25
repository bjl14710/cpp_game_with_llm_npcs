#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "DayNight.hpp"
#include "Math.hpp"

namespace llm_npc {

// The Neo-Noir night look as pure numbers — core so the curves are
// unit-testable; the renderer feeds them to shader uniforms. Every tunable
// is a blend from its daytime identity (no effect) to its full-night value
// by nightFactorAt(hours), so the whole look rides the ONE world clock and
// daytime renders exactly as before.
struct NoirLook {
    float night = 0.f;            // the 0..1 factor everything below scales by
    float desaturation = 0.f;     // 0 = full color .. 0.92 = near-monochrome
    float contrast = 0.f;         // S-curve mix: 0 = identity .. 0.55 = punchy
    float vignette = 0.f;         // corner darkening strength
    float grain = 0.f;            // animated film-grain amplitude
    float fogDensityScale = 1.f;  // multiplies the base fog density
    SkyColor sky{};               // sky clear + fog color (noir near-black at night)
    SkyColor lampColor{};         // sodium lamp tint (constant hue)
    float lampIntensity = 0.f;    // lamp pool brightness; 0 = lamps off
    float lampRadius = 0.f;       // pool falloff radius in world units
    float wetSpecular = 0.f;      // street reflection strength (night x wetness)
    float rain = 0.f;             // rain density, 0..1
};

namespace noir {

// Full-night targets. Daytime values are the struct defaults above.
constexpr float kDesaturation = 0.92f;
constexpr float kContrast = 0.55f;
constexpr float kVignette = 0.55f;
constexpr float kGrain = 0.06f;
constexpr float kFogDensityScale = 1.8f;
constexpr float kLampIntensity = 1.6f;
constexpr float kLampRadius = 11.f;
constexpr float kWetSpecular = 0.9f;
constexpr float kRain = 1.f;

// Reference palette: near-black blue sky #07090d, sodium lamp #ffd9a0.
constexpr SkyColor kSky{7.f / 255.f, 9.f / 255.f, 13.f / 255.f};
constexpr SkyColor kLamp{255.f / 255.f, 217.f / 255.f, 160.f / 255.f};

// Most lamps the shader takes per frame (its uniform array size).
constexpr std::size_t kMaxLamps = 16;

}  // namespace noir

// The noir look blended for a time of day in hours [0, 24). `enabled` false
// (the noir_night = off toggle) pins the factor to 0 at every hour, which
// yields today's plain look: identity grade, base fog, the unmodified
// skyColorAt sky, and no lamps, reflections, or rain.
inline NoirLook noirLookAt(float hours, bool enabled = true) {
    const float n = enabled ? nightFactorAt(hours) : 0.f;
    NoirLook look;
    look.night = n;
    look.desaturation = noir::kDesaturation * n;
    look.contrast = noir::kContrast * n;
    look.vignette = noir::kVignette * n;
    look.grain = noir::kGrain * n;
    look.fogDensityScale = 1.f + (noir::kFogDensityScale - 1.f) * n;
    look.sky = daynight::lerp(skyColorAt(hours), noir::kSky, n);
    look.lampColor = noir::kLamp;
    look.lampIntensity = noir::kLampIntensity * n;
    look.lampRadius = noir::kLampRadius;
    look.wetSpecular = noir::kWetSpecular * n;
    look.rain = noir::kRain * n;
    return look;
}

// The (at most `cap`) lamp positions nearest `eye`, nearest first — the
// per-frame CPU pick that fits a whole city of lamps into the shader's
// fixed-size uniform array. Distance is full 3D (lamp heads sit overhead).
inline std::vector<Vec3> nearestLamps(const std::vector<Vec3>& lamps, const Vec3& eye,
                                      std::size_t cap = noir::kMaxLamps) {
    std::vector<Vec3> picked = lamps;
    const auto closer = [&eye](const Vec3& a, const Vec3& b) {
        return dot(a - eye, a - eye) < dot(b - eye, b - eye);
    };
    const std::size_t n = std::min(cap, picked.size());
    std::partial_sort(picked.begin(), picked.begin() + static_cast<std::ptrdiff_t>(n),
                      picked.end(), closer);
    picked.resize(n);
    return picked;
}

}  // namespace llm_npc
