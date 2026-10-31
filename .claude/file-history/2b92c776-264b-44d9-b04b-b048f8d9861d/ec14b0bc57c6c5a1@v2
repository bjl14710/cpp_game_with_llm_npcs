#include "Gossip.hpp"

#include <cctype>

#include "json.hpp"

namespace llm_npc {

std::string normalizeSubject(const std::string& raw) {
    std::string out;
    bool lastUnderscore = true;  // also trims leading separators
    for (const char c : raw) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::tolower(uc)));
            lastUnderscore = false;
        } else if (!lastUnderscore) {
            out.push_back('_');
            lastUnderscore = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

std::string factIdFor(const std::string& subject, const std::string& content) {
    // FNV-1a over subject|content — the same stable-hash approach Assets
    // uses for filler buildings; collisions across a town's worth of facts
    // are not a realistic concern.
    std::size_t h = 1469598103934665603ull;
    for (const unsigned char c : subject + "|" + content) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return "fact_" + std::to_string(h);
}

std::vector<ProposedFact> validateProposedFacts(const std::string& json) {
    std::vector<ProposedFact> out;
    const nlohmann::json parsed =
        nlohmann::json::parse(json, nullptr, /*allow_exceptions=*/false);
    if (!parsed.is_array() || parsed.size() > 2) return out;

    for (const auto& item : parsed) {
        if (!item.is_object()) return {};
        if (!item.contains("subject") || !item["subject"].is_string() ||
            !item.contains("content") || !item["content"].is_string() ||
            !item.contains("direction") || !item["direction"].is_string()) {
            return {};
        }
        ProposedFact fact;
        fact.subject = normalizeSubject(item["subject"].get<std::string>());
        fact.content = item["content"].get<std::string>();
        const std::string direction = item["direction"].get<std::string>();
        if (direction == "player_learned") {
            fact.playerLearned = true;
        } else if (direction != "npc_learned") {
            return {};
        }
        if (fact.subject.empty() || fact.content.empty() ||
            fact.content.size() > 140) {
            return {};
        }
        out.push_back(std::move(fact));
    }
    return out;
}

KnownFact commitFact(WorldState& state, const ProposedFact& fact,
                     const std::string& npcName) {
    KnownFact record;
    record.subject = fact.subject;
    record.content = fact.content;
    record.factId = factIdFor(fact.subject, fact.content);
    record.source = fact.playerLearned ? npcName : "player";
    record.learnedAtSeconds = state.number("world_time_seconds");
    state.addFact(record);  // idempotent: an old fact keeps its stamp
    state.grantKnowledge(npcName, record.factId);
    if (fact.playerLearned) state.grantKnowledge("player", record.factId);
    return *state.findFact(record.factId);  // the surviving (original) record
}

int propagateGossip(WorldState& state, const std::vector<AgentAt>& agents,
                    std::mt19937& rng) {
    const double now = state.number("world_time_seconds");
    std::uniform_real_distribution<float> roll(0.f, 1.f);
    int transfers = 0;

    for (std::size_t a = 0; a < agents.size(); ++a) {
        for (std::size_t b = a + 1; b < agents.size(); ++b) {
            if (distanceXZ(agents[a].position, agents[b].position) > kGossipRadius) {
                continue;
            }
            // Symmetric pair: each side may pass one eligible fact.
            for (const auto& [teller, hearer] :
                 {std::pair{&agents[a], &agents[b]}, std::pair{&agents[b], &agents[a]}}) {
                if (roll(rng) >= kGossipChance) continue;
                for (const KnownFact* fact : state.factsKnownBy(teller->name)) {
                    // Age gate is wrap-aware only in spirit: facts learned
                    // "in the future" (clock wrapped past midnight) count
                    // as old enough — gossip should not go quiet at 00:00.
                    const double age = now - fact->learnedAtSeconds;
                    const bool oldEnough =
                        age >= kGossipMinAgeSeconds || age < 0.0;
                    if (!oldEnough || state.knows(hearer->name, fact->factId)) {
                        continue;
                    }
                    state.grantKnowledge(hearer->name, fact->factId);
                    ++transfers;
                    break;  // at most one fact per pair direction per tick
                }
            }
        }
    }
    return transfers;
}

}  // namespace llm_npc
