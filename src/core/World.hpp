#pragma once

#include <random>
#include <vector>

#include "City.hpp"
#include "CombatEvents.hpp"
#include "Math.hpp"
#include "Npc.hpp"
#include "Player.hpp"

namespace llm_npc {

// Simple projectile for ranged attacks (player or NPC). No physics — travels
// at constant speed on the XZ plane and expires after maxLifetime seconds.
struct Projectile {
    Vec3  position{};
    Vec3  direction{};   // normalised, XZ only
    float speed     = 0.f;
    float lifetime  = 0.f;
    float maxLifetime = 2.f;
    bool  fromPlayer = true;   // false → NPC shot, damages player
    int   damage     = 0;
    std::size_t ownerNpcIndex = 0; // only meaningful when fromPlayer==false
};

// The complete game world: the city geometry plus every NPC living in it.
// SFML-free so proximity and placement logic stays unit-testable.
class World {
   public:
    explicit World(City city);

    const City& city() const { return city_; }

    // Adds an NPC (already placed via setPlacement).
    void addNpc(Npc npc) { npcs_.push_back(std::move(npc)); }

    std::vector<Npc>& npcs() { return npcs_; }
    const std::vector<Npc>& npcs() const { return npcs_; }

    // Index of the NPC closest to `pos` on the ground plane, provided they
    // are within `radius`; -1 when nobody is in range. Drives the
    // "[T] Talk to <name>" prompt and chat targeting.
    int nearestNpcWithin(const Vec3& pos, float radius) const;

    // --- Player access ---------------------------------------------------

    Player&       player()       { return player_; }
    const Player& player() const { return player_; }

    // --- Combat API ------------------------------------------------------

    // Called by main.cpp on left mouse button press (in Playing mode only).
    // Consumes attack cooldown and ammo, spawns projectile if ranged.
    // Returns false if the attack was suppressed (cooldown active, no ammo).
    // TODO(combat): implement in World.cpp
    bool playerAttack(const Vec3& aimDirection);

    // Switches the player's equipped weapon. Silently ignored if the player
    // has no ammo for the requested ranged weapon.
    // TODO(combat): implement in World.cpp
    void playerSwitchWeapon(WeaponKind kind);

    // Advances all combat simulation: projectile travel, flee movement,
    // hostile NPC AI, cooldown ticks, death checks. Returns all events that
    // fired this frame so main.cpp can route LLM triggers and effects.
    // TODO(combat): implement in World.cpp
    CombatFrameResult updateCombat(float dt);

   private:
    City   city_;
    Player player_;
    std::vector<Npc>        npcs_;
    std::vector<Projectile> projectiles_;
    std::mt19937 rng_;  // seeded once in constructor; used for accuracy rolls

    // NPC flee / hostile movement constants.
    static constexpr float kFleeSpeed          = 6.0f;   // units/s
    static constexpr float kHostilePreferRange = 8.0f;   // preferred fire distance
    static constexpr float kNpcFireCooldownMin = 1.2f;
    static constexpr float kNpcFireCooldownMax = 2.0f;
    static constexpr float kNpcHitChance       = 0.70f;  // base cop accuracy

    // Hearing radius: armed NPCs within this range of a combat event react.
    static constexpr float kCombatHearingRadius = 20.0f;

    // Helpers used by updateCombat.
    // TODO(combat): implement these in World.cpp
    void tickProjectiles(float dt, CombatFrameResult& out);
    void tickNpcAi(float dt, CombatFrameResult& out);
    void tickCooldowns(float dt);
};

}  // namespace llm_npc
