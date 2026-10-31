#include "Traffic.hpp"

#include <algorithm>
#include <cmath>

namespace llm_npc {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kLaneOffset = 3.0f;   // lane center's offset from the avenue line
constexpr float kCarHalfLen = 2.2f;   // half the car's length (along travel axis)
constexpr float kCarHalfWid = 1.0f;   // half the car's width
constexpr float kLookAhead = 8.0f;    // how far ahead a car watches for obstacles
constexpr float kLateral = 1.9f;      // lateral half-width of the look-ahead box
constexpr float kAccel = 10.0f;       // acceleration toward target speed
constexpr float kDecel = 22.0f;       // braking deceleration

// The avenue centerlines of the 5x5 grid (see City::makeDowntown).
constexpr float kGrid[] = {-96.f, -32.f, 32.f, 96.f};
constexpr int kGridN = static_cast<int>(sizeof(kGrid) / sizeof(kGrid[0]));

}  // namespace

float Traffic::rand01() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return static_cast<float>((rng_ >> 8) & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

void Traffic::refreshPose(Vehicle& v) const {
    if (v.axis == 0) {  // north-south street: x fixed in the lane, z = pos
        v.x = v.line + static_cast<float>(v.dir) * kLaneOffset;
        v.z = v.pos;
        v.headingDeg = v.dir > 0 ? 0.f : 180.f;
    } else {  // east-west street: z fixed in the lane, x = pos
        v.x = v.pos;
        v.z = v.line - static_cast<float>(v.dir) * kLaneOffset;
        v.headingDeg = v.dir > 0 ? 90.f : 270.f;
    }
}

Traffic::Traffic(float halfSize, std::uint32_t seed, int count)
    : halfSize_(halfSize), rng_(seed ? seed : 1u) {
    for (int i = 0; i < count; ++i) {
        Vehicle v;
        v.axis = rand01() < 0.5f ? 0 : 1;
        v.line = kGrid[static_cast<int>(rand01() * kGridN) % kGridN];
        v.dir = rand01() < 0.5f ? 1 : -1;
        v.pos = (rand01() * 2.f - 1.f) * halfSize;
        v.maxSpeed = 7.f + rand01() * 5.f;
        v.speed = v.maxSpeed;
        v.colorId = static_cast<int>(rand01() * 997.f);
        refreshPose(v);
        vehicles_.push_back(v);
    }
}

void Traffic::update(float dt, const Vec3& playerPos, const std::vector<Vec3>& people) {
    const float wrap = halfSize_ + 4.f;
    // Closest an obstacle's center may get ahead of a car's center before the
    // car stops dead, so the body (front bumper at +kCarHalfLen) never overlaps.
    const float kHardStop = kCarHalfLen + 1.2f;

    for (Vehicle& v : vehicles_) {
        const float h = v.headingDeg * kPi / 180.f;
        const float fs = std::sin(h);
        const float fc = std::cos(h);
        // Forward distance to (ox, oz) in this car's frame when it sits inside
        // the look-ahead box; returns false (and a huge distance) otherwise.
        const auto forwardTo = [&](float ox, float oz, float maxLat, float& outFwd) {
            const float dx = ox - v.x;
            const float dz = oz - v.z;
            const float forward = dx * fs + dz * fc;
            const float lateral = dx * fc - dz * fs;
            outFwd = forward;
            return forward > 0.3f && forward < kLookAhead && std::fabs(lateral) < maxLat;
        };

        float target = v.maxSpeed;
        float nearestYield = 1e9f;  // closest obstacle this car is yielding to
        float fwd = 0.f;

        // The player always has priority: brake for them.
        if (forwardTo(playerPos.x, playerPos.z, kLateral + 0.4f, fwd)) {
            target = 0.f;
            nearestYield = std::min(nearestYield, fwd);
        }
        // Pedestrians get the same priority: cars stop for them, not through them.
        for (const Vec3& q : people) {
            if (forwardTo(q.x, q.z, kLateral + 0.2f, fwd)) {
                target = 0.f;
                nearestYield = std::min(nearestYield, fwd);
            }
        }
        // Yield to cars ahead: a follower yields to the car it trails, and at a
        // crossing north-south traffic (axis 0) yields to east-west (axis 1).
        for (const Vehicle& o : vehicles_) {
            if (&o == &v) continue;
            if (!forwardTo(o.x, o.z, kLateral, fwd)) continue;
            const bool following = o.axis == v.axis;
            if (following || v.axis == 0) {
                target = 0.f;
                nearestYield = std::min(nearestYield, fwd);
            }
        }

        if (v.speed < target) {
            v.speed = std::min(target, v.speed + kAccel * dt);
        } else {
            v.speed = std::max(target, v.speed - kDecel * dt);
        }
        // Emergency stop: never creep into an obstacle we're yielding to.
        if (nearestYield < kHardStop) v.speed = 0.f;

        v.pos += static_cast<float>(v.dir) * v.speed * dt;
        if (v.pos > wrap) v.pos = -wrap;
        else if (v.pos < -wrap) v.pos = wrap;
        refreshPose(v);
    }
}

const Vehicle* Traffic::vehicleAt(float x, float z, float radius) const {
    for (const Vehicle& v : vehicles_) {
        // Cars are grid-aligned, so the body box is axis-aligned: length runs
        // along Z for a north-south car, along X for an east-west car.
        const float hx = v.axis == 0 ? kCarHalfWid : kCarHalfLen;
        const float hz = v.axis == 0 ? kCarHalfLen : kCarHalfWid;
        const float nx = clampf(x, v.x - hx, v.x + hx);
        const float nz = clampf(z, v.z - hz, v.z + hz);
        const float dx = x - nx;
        const float dz = z - nz;
        if (dx * dx + dz * dz < radius * radius) return &v;
    }
    return nullptr;
}

bool Traffic::circleHitsVehicle(float x, float z, float radius) const {
    return vehicleAt(x, z, radius) != nullptr;
}

}  // namespace llm_npc
