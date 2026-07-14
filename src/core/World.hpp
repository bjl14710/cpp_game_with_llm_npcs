#pragma once

#include <random>
#include <vector>

#include "City.hpp"
#include "CombatEvents.hpp"
#include "Math.hpp"
#include "Npc.hpp"
#include "Player.hpp"
#include "WorldState.hpp"

namespace llm_npc {

// Simple projectile for ranged attacks (player or NPC). No physics — travels
// at constant speed on the XZ plane and expires after maxLifetime seconds.
struct Projectile {
    Vec3  position{};
    Vec3  direction{};   // normalised, full 3D (aim follows pitch — issue #91)
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

    // Swaps in a different city and clears every inhabitant — the seam
    // that makes town <-> sandbox-map switching possible. World cannot be
    // reassigned (Npc holds an LlmClient&), so contents swap IN PLACE.
    // The world bus (state_) deliberately survives: facts, knowledge and
    // the clock are session state, not map state. Callers must respawn
    // NPCs and rebuild their per-NPC bookkeeping afterwards.
    void loadCity(City city);

    // The world bus: shared facts every system reads instead of keeping a
    // private copy — the clock lives here (see WorldState).
    WorldState&       state()       { return state_; }
    const WorldState& state() const { return state_; }

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

    // Read-only view of in-flight projectiles. Lets tests inspect a shot's
    // origin and direction directly (the renderer does not draw them yet).
    const std::vector<Projectile>& projectiles() const { return projectiles_; }

    // --- Combat API ------------------------------------------------------

    // Called by main.cpp on left mouse button press (in Playing mode only).
    // Consumes attack cooldown and ammo, spawns projectile if ranged.
    // Returns false if the attack was suppressed (cooldown active, no ammo).
        bool playerAttack(const Vec3& aimDirection);

    // Switches the player's equipped weapon. Silently ignored if the player
    // has no ammo for the requested ranged weapon.
        void playerSwitchWeapon(WeaponKind kind);

    // Advances all combat simulation: projectile travel, flee movement,
    // hostile NPC AI, cooldown ticks, death checks. Returns all events that
    // fired this frame so main.cpp can route LLM triggers and effects.
        CombatFrameResult updateCombat(float dt);

   private:
    City   city_;
    WorldState state_;
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
