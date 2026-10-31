#include "Montage.hpp"

#include <algorithm>

#include "Gossip.hpp"  // factIdFor

namespace llm_npc {

std::vector<ClueStep> solutionChain(const MysterySetup& setup) {
    std::vector<ClueStep> chain;
    chain.reserve(setup.evidence.size() + setup.witnesses.size());

    // Evidence first, in the order the template authored it. StorylineClue
    // carries an explicit `order` and castStoryline emits evidence in that
    // order, so position here IS authored order — nothing needs re-sorting,
    // and re-sorting would destroy the only thing that makes the montage read
    // as reasoning rather than as a list.
    int order = 1;
    for (const Evidence& evidence : setup.evidence) {
        ClueStep step;
        step.zoneId = evidence.zoneId;
        step.caption = evidenceFactContent(evidence);
        // Computed through the shared builders rather than reconstructed, so
        // this cannot drift from whatever seeds them. Present whether or not
        // anything has committed the fact: an id for a fact nobody made simply
        // matches nothing, which is the correct answer for an undiscovered clue.
        step.factId =
            factIdFor(evidenceFactSubject(evidence), evidenceFactContent(evidence));
        step.order = order++;
        chain.push_back(std::move(step));
    }

    // Then the testimony that bears on the killer. Only testimony ABOUT the
    // killer belongs in the argument — an account of someone else's evening is
    // real content, and real noise, but it is not why the players were right.
    //
    // Reading setup.killer here is a sanctioned use: this function is
    // host-only and its output must not reach a client before MatchOver, which
    // the header says and the win cutscene is the only caller of.
    for (const Witness& witness : setup.witnesses) {
        if (witness.agent.empty()) continue;
        if (witness.about != setup.killer) continue;

        ClueStep step;
        step.zoneId = witness.sawZoneId;
        step.caption = witnessFactContent(witness);
        step.factId =
            factIdFor(witnessFactSubject(witness), witnessFactContent(witness));
        step.order = order++;
        chain.push_back(std::move(step));
    }

    return chain;
}

MontagePlan buildMontage(const std::vector<ClueStep>& chain,
                         const WorldState& state, const std::string& agent) {
    MontagePlan plan;
    for (const ClueStep& step : chain) {
        // A step with no factId can never be found: there is nothing on the
        // bus to have learned. `missed` is the beat that exists to show it.
        const bool found =
            !step.factId.empty() && state.knows(agent, step.factId);
        (found ? plan.found : plan.missed).push_back(step);
    }
    // Both lists came out of one ordered walk, so each already holds chain
    // order. No sort — a sort here would be the exact mistake the `order`
    // field exists to prevent.
    return plan;
}

}  // namespace llm_npc
