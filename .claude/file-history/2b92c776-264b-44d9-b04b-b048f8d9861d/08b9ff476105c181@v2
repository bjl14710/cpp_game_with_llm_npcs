#pragma once

#include <cstdint>

#include "Persona.hpp"

namespace llm_npc {

// Builds a randomized pedestrian persona from `seed`, deterministically: the
// same seed always yields the same person, so a spawned crowd is stable across
// frames and reproducible in tests. `policeChance` in [0, 1] is the probability
// the pedestrian is an (arrestable) police officer; those get persona.police =
// true so they may arrest, plus a patrol role. Used to populate the streets
// with talkable strangers.
Persona makePedestrianPersona(std::uint32_t seed, double policeChance);

}  // namespace llm_npc
