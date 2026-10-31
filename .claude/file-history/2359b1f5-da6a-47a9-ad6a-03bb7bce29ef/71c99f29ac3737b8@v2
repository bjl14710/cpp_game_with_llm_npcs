#pragma once

#include <cstddef>
#include <vector>

namespace llm_npc {

// Lightweight POD event structs emitted by World::updateCombat each frame.
// main.cpp iterates the returned vectors and routes LLM triggers / audio /
// screen effects. No heap allocation, no virtual dispatch.

// The player's melee swing connected with an NPC.
struct MeleeHitEvent {
    std::size_t npcIndex;   // index into World::npcs()
    int         damage;
};

// The player fired a ranged weapon. May or may not have hit an NPC.
struct ShotFiredEvent {
    bool        hitNpc;
    std::size_t npcIndex;   // only meaningful when hitNpc == true
    int         damage;     // only meaningful when hitNpc == true
};

// An NPC took damage (from any source). main.cpp uses this to fire the LLM
// combat-reaction prompt on the damaged NPC and nearby armed NPCs.
struct NpcDamagedEvent {
    std::size_t npcIndex;
    int         damage;
    bool        killedByHit;  // true if this hit dropped the NPC to 0 HP
};

// An armed (Hostile) NPC fired at the player.
struct NpcShotPlayerEvent {
    std::size_t npcIndex;
    bool        landed;   // false = missed (accuracy roll failed)
    int         damage;   // only meaningful when landed == true
};

// Bundle returned by World::updateCombat so main.cpp only needs one call.
struct CombatFrameResult {
    std::vector<MeleeHitEvent>      meleeHits;
    std::vector<ShotFiredEvent>     shotsFired;
    std::vector<NpcDamagedEvent>    npcsDamaged;
    std::vector<NpcShotPlayerEvent> npcShots;
};

}  // namespace llm_npc
