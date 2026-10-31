#include "HostChatRouter.hpp"

#include "AsciiText.hpp"

#include "NpcAction.hpp"

namespace llm_npc {

HostChatRouter::HostChatRouter(World& world, NetServer& server)
    : world_(world), server_(server) {}

void HostChatRouter::update() {
    for (const auto& ev : server_.drainChatEvents()) {
        if (ev.npcIndex < 0 || ev.npcIndex >= static_cast<int>(world_.npcs().size())) {
            continue;  // stale or hostile index; nothing sane to do
        }
        if (ev.open) {
            // v1: opening a conversation carries no server-side effect; the
            // NPC reacts when the first line arrives.
            continue;
        }
        queued_[ev.npcIndex].push_back(PendingLine{ev.playerId, ev.text});
    }
    // Submit for every NPC with waiting lines; each pump sends at most one
    // request (the NPC is busy until its reply routes back).
    for (auto& entry : queued_) pumpNpc(entry.first);
}

void HostChatRouter::pumpNpc(int npcIndex) {
    auto it = queued_.find(npcIndex);
    if (it == queued_.end() || it->second.empty()) return;
    Npc& npc = world_.npcs()[static_cast<std::size_t>(npcIndex)];
    if (npc.waiting()) return;  // busy with anyone's request — host's included

    PendingLine line = std::move(it->second.front());
    it->second.pop_front();
    const std::uint64_t id = npc.ask(line.text);
    inflight_[id] = {line.playerId, npcIndex};
}

bool HostChatRouter::routeDelta(const ChatDelta& delta) {
    const auto it = inflight_.find(delta.id);
    if (it == inflight_.end()) return false;
    // A dead player's stream just drops on the floor (sendToPlayer false).
    server_.sendToPlayer(it->second.first, MessageType::ChatDelta,
                         {{"npc", it->second.second}, {"text", delta.text}});
    return true;
}

bool HostChatRouter::routeReply(const ChatReply& reply) {
    const auto it = inflight_.find(reply.id);
    if (it == inflight_.end()) return false;
    const int playerId = it->second.first;
    const int npcIndex = it->second.second;
    inflight_.erase(it);

    Npc& npc = world_.npcs()[static_cast<std::size_t>(npcIndex)];
    // Updates history/behavior/mood exactly as a host-local reply would and
    // returns the directive-stripped text to show.
    const auto text = npc.onReplyArrived(reply);

    if (reply.ok) {
        // The fallback is RAW model text: onReplyArrived returns nullopt when
        // the reply id does not match the NPC's pending one, which is
        // reachable today because an Npc has a single pendingId_ shared across
        // askers. Folding here too, or the "don?t" this release fixes comes
        // straight back for every bystander in exactly the concurrent-dialogue
        // case the report was about. Found by behaviour QA, not by a test.
        const std::string spoken =
            text ? *text : toRenderableAscii(reply.content);
        server_.sendToPlayer(playerId, MessageType::ChatReply,
                             {{"npc", npcIndex}, {"ok", true}, {"text", spoken}});
        // Bystanders see the NPC speak and feel it — the requester included,
        // which keeps every client's rendering state identical.
        server_.broadcast(MessageType::NpcSpeechBubble,
                          {{"npc", npcIndex}, {"text", spoken}});
        announceNpcMood(npcIndex);
    } else {
        // Failures concern only the player who asked (plan edge case).
        server_.sendToPlayer(playerId, MessageType::ChatReply,
                             {{"npc", npcIndex}, {"ok", false}, {"error", reply.errorMessage}});
    }

    pumpNpc(npcIndex);  // next queued line, if anyone was waiting
    return true;
}

void HostChatRouter::announceNpcSpeech(int npcIndex, const std::string& text) {
    if (text.empty()) return;
    if (npcIndex < 0 || npcIndex >= static_cast<int>(world_.npcs().size())) return;
    server_.broadcast(MessageType::NpcSpeechBubble, {{"npc", npcIndex}, {"text", text}});
}

void HostChatRouter::announceNpcMood(int npcIndex) {
    if (npcIndex < 0 || npcIndex >= static_cast<int>(world_.npcs().size())) return;
    const Npc& npc = world_.npcs()[static_cast<std::size_t>(npcIndex)];
    server_.broadcast(MessageType::NpcMoodUpdate,
                      {{"npc", npcIndex}, {"mood", static_cast<int>(npc.mood())}});
}

}  // namespace llm_npc
