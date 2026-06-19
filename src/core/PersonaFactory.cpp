#include "PersonaFactory.hpp"

#include <string>

namespace llm_npc {

namespace {

// Same bit-mixer family as the renderer's appearanceFromSeed: distinct salts
// give independent, deterministic streams from one seed.
std::uint32_t mix(std::uint32_t seed, std::uint32_t salt) {
    std::uint32_t x = (seed + 0x9e3779b9u) * (salt * 2654435761u + 1u);
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return x;
}

// Picks one entry of a string pool by seed+salt.
template <std::size_t N>
const char* pickOne(std::uint32_t seed, std::uint32_t salt, const char* const (&pool)[N]) {
    return pool[mix(seed, salt) % N];
}

}  // namespace

Persona makePedestrianPersona(std::uint32_t seed, double policeChance) {
    static const char* const firstNames[] = {
        "Alex", "Sam",   "Jordan", "Taylor", "Maria", "Diego", "Priya",
        "Wei",  "Omar",  "Nina",   "Liam",   "Zoe",   "Marcus", "Aisha",
        "Tomas", "Yuki", "Ravi",   "Elena",  "Jamal", "Greta"};
    static const char* const lastNames[] = {
        "Reyes", "Park",  "Okafor", "Bianchi", "Nguyen", "Cohen",  "Santos", "Ivanov",
        "Khan",  "Murphy", "Tanaka", "Silva",  "Adeyemi", "Larsen", "Rossi",  "Haddad"};
    static const char* const roles[] = {
        "jogger",          "tourist",  "student",  "office worker", "dog walker",
        "street musician", "retiree",  "delivery courier", "shopper", "local artist"};
    static const char* const traitPool[] = {
        "cheerful", "shy",       "talkative", "gruff",    "curious",   "distracted",
        "friendly", "sarcastic", "earnest",   "laid-back", "nervous",  "upbeat"};

    Persona p;
    const bool police =
        (mix(seed, 99u) % 1000u) < static_cast<std::uint32_t>(policeChance * 1000.0);

    p.name = std::string(pickOne(seed, 1u, firstNames)) + " " + pickOne(seed, 2u, lastNames);
    if (police) {
        p.role = "police officer on patrol";
        p.police = true;
    } else {
        p.role = pickOne(seed, 3u, roles);
    }

    // Two traits, kept distinct when the pool happens to pick the same twice.
    const char* t1 = pickOne(seed, 4u, traitPool);
    const char* t2 = pickOne(seed, 5u, traitPool);
    p.traits.push_back(t1);
    if (std::string(t2) != t1) p.traits.push_back(t2);

    p.speakingStyle = "casual, 1-2 short sentences";
    p.knowledgeBoundary =
        "knows the city in a general way; nothing about real-world news or events";
    return p;
}

}  // namespace llm_npc
