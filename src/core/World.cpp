#include "World.hpp"

#include <algorithm>
#include <random>

namespace llm_npc {

World::World(City city)
    : city_(std::move(city)),
      rng_(std::random_device{}()) {}

bool World::playerAttack(const Vec3& /*aimDirection*/) {
    // TODO(combat): implement player attack
    // Steps:
    //   1. Check player_.attackCooldown > 0 → return false.
    //   2. For Fist: find nearest NPC within weaponDef(Fist).range, call
    //      npc.takeDamage(weaponDef(Fist).damage), set cooldown, trigger anim.
    //   3. For Pistol: check ammo > 0, spawn a Projectile in aimDirection,
    //      decrement ammo, set cooldown.
    //   4. Return true if attack was performed.
    return false;
}

void World::playerSwitchWeapon(WeaponKind kind) {
    if (weaponDef(kind).ammoMax > 0) {
        const auto it = player_.ammo.find(static_cast<std::uint8_t>(kind));
        if (it == player_.ammo.end() || it->second == 0) return;
    }
    player_.weapon = kind;
}

CombatFrameResult World::updateCombat(float dt) {
    CombatFrameResult result;
    tickCooldowns(dt);
    // TODO(combat): uncomment as each subsystem is implemented:
    // tickProjectiles(dt, result);
    // tickNpcAi(dt, result);
    return result;
}

void World::tickProjectiles(float /*dt*/, CombatFrameResult& /*out*/) {
    // TODO(combat): advance each projectile along its direction * speed * dt.
    // Check collision with NPC bounding cylinders (radius ~0.4, height ~1.8).
    // On hit: call npc.takeDamage(), push NpcDamagedEvent + ShotFiredEvent.
    // Remove projectiles whose lifetime >= maxLifetime.
}

void World::tickNpcAi(float /*dt*/, CombatFrameResult& /*out*/) {
    // TODO(combat): per-NPC state-machine update.
    // Fleeing: move away from player_.position using city_.resolveMovement.
    // Hostile: if within preferred range, fire (roll kNpcHitChance, push
    //          NpcShotPlayerEvent, apply damage to player if landed).
    //          Otherwise advance toward player.
}

void World::tickCooldowns(float dt) {
    // TODO(combat): tick player and NPC fire cooldowns.
    player_.attackCooldown = std::max(0.f, player_.attackCooldown - dt);
    player_.attackAnimFraction = std::max(0.f, player_.attackAnimFraction - dt / 0.15f);
    for (auto& npc : npcs_) {
        npc.fireCooldown = std::max(0.f, npc.fireCooldown - dt);
    }
}

int World::nearestNpcWithin(const Vec3& pos, float radius) const {
    int best = -1;
    float bestDist = radius;
    for (std::size_t i = 0; i < npcs_.size(); ++i) {
        const float dist = distanceXZ(pos, npcs_[i].position());
        if (dist <= bestDist) {
            bestDist = dist;
            best = static_cast<int>(i);
        }
    }
    return best;
}

}  // namespace llm_npc
