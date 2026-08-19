#include "World.hpp"

#include <algorithm>
#include <random>

namespace llm_npc {

World::World(City city)
    : city_(std::move(city)),
      rng_(std::random_device{}()) {}

bool World::playerAttack(const Vec3& aimDirection) {
    if (player_.attackCooldown > 0.f) return false;

    const WeaponDef& def = weaponDef(player_.weapon);

    if (player_.weapon == WeaponKind::Fist) {
        const int target = nearestNpcWithin(player_.position, def.range);
        player_.attackCooldown    = def.fireCooldown;
        player_.attackAnimFraction = 1.0f;
        if (target < 0) return true;  // swing missed — cooldown still consumed
        npcs_[static_cast<std::size_t>(target)].takeDamage(def.damage);
        return true;
    }

    // Pistol: check ammo, spawn a projectile.
    auto& ammoSlot = player_.ammo[static_cast<std::uint8_t>(player_.weapon)];
    if (ammoSlot <= 0) return false;

    --ammoSlot;
    player_.attackCooldown    = def.fireCooldown;
    player_.attackAnimFraction = 1.0f;

    // Fire along the full 3D aim (issue #91): keep the y component instead of
    // flattening it, and spawn from eye height so the shot leaves from the
    // point the player is looking through. tickProjectiles integrates the
    // direction in 3D and the [0,kNpcHeight] band test still catches hits.
    const Vec3 dir = normalize(aimDirection);
    Projectile p;
    p.position   = player_.position + Vec3{0.f, kEyeHeight, 0.f};
    p.direction  = dir;
    p.speed      = 40.f;
    p.damage     = def.damage;
    p.fromPlayer = true;
    projectiles_.push_back(p);
    return true;
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
    tickProjectiles(dt, result);
    tickNpcAi(dt, result);
    return result;
}

void World::tickProjectiles(float dt, CombatFrameResult& out) {
    static constexpr float kNpcRadius = 0.4f;
    static constexpr float kNpcHeight = 1.8f;

    for (auto& proj : projectiles_) {
        proj.position += proj.direction * (proj.speed * dt);
        proj.lifetime += dt;
    }

    auto it = projectiles_.begin();
    while (it != projectiles_.end()) {
        // Retire on age, or once a downward shot has passed below the ground —
        // now that aim follows pitch (#91), a missed downward shot would
        // otherwise keep travelling underground until its lifetime expired.
        if (it->lifetime >= it->maxLifetime ||
            (it->position.y < 0.f && it->direction.y < 0.f)) {
            it = projectiles_.erase(it);
            continue;
        }

        bool hit = false;
        for (std::size_t i = 0; i < npcs_.size(); ++i) {
            Npc& npc = npcs_[i];
            if (npc.combatState() == NpcState::Dead) continue;
            if (!it->fromPlayer) continue;  // NPC projectiles hit player only

            const float xzDist = distanceXZ(it->position, npc.position());
            if (xzDist > kNpcRadius) continue;
            if (it->position.y < 0.f || it->position.y > kNpcHeight) continue;

            const bool killed = (npc.hp() - it->damage <= 0);
            npc.takeDamage(it->damage);
            out.npcsDamaged.push_back({i, it->damage, killed});
            out.shotsFired.push_back({true, i, it->damage});
            hit = true;
            break;
        }

        if (hit) {
            it = projectiles_.erase(it);
        } else {
            ++it;
        }
    }
}

void World::tickNpcAi(float dt, CombatFrameResult& out) {
    for (std::size_t i = 0; i < npcs_.size(); ++i) {
        Npc& npc = npcs_[i];
        switch (npc.combatState()) {
            case NpcState::Fleeing: {
                const Vec3 away = normalize(npc.position() - player_.position);
                const Vec3 target = npc.position() + away * (kFleeSpeed * dt);
                npc.position() = city_.resolveMovement(npc.position(), target, 0.4f);
                break;
            }
            case NpcState::Hostile: {
                const float dist = distanceXZ(npc.position(), player_.position);
                const Vec3 dir   = normalize(player_.position - npc.position());
                if (dist > kHostilePreferRange + 1.f) {
                    const Vec3 target = npc.position() + dir * (kFleeSpeed * dt);
                    npc.position() = city_.resolveMovement(npc.position(), target, 0.4f);
                } else if (dist < kHostilePreferRange - 1.f) {
                    const Vec3 target = npc.position() - dir * (kFleeSpeed * dt);
                    npc.position() = city_.resolveMovement(npc.position(), target, 0.4f);
                }
                if (npc.fireCooldown <= 0.f) {
                    npc.fireCooldown = kNpcFireCooldownMin +
                        std::uniform_real_distribution<float>(
                            0.f, kNpcFireCooldownMax - kNpcFireCooldownMin)(rng_);
                    const bool landed =
                        std::uniform_real_distribution<float>(0.f, 1.f)(rng_) < kNpcHitChance;
                    const int dmg = landed ? weaponDef(WeaponKind::Pistol).damage : 0;
                    out.npcShots.push_back({i, landed, dmg});
                    if (landed) {
                        player_.hp = std::max(0, player_.hp - dmg);
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}

void World::tickCooldowns(float dt) {
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
