#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>

#include "LlmClient.hpp"
#include "NetServer.hpp"
#include "World.hpp"

namespace llm_npc {

// Routes remote players' NPC conversations through the host's single
// World + LlmClient, so every player talks to the same NPCs with the same
// memory and mood (plan: .claude/plans/multiplayer-and-aws-deploy.md).
//
// Runs entirely on the host's main loop — it is the "glue" half of
// NetServer's thread contract. Each frame:
//
//   router.update();                                  // apply remote chat events
//   for (auto& d : llm.drainDeltas())
//       if (!router.routeDelta(d)) ...host's own...   // consumed = remote
//   for (auto& r : llm.drainReplies())
//       if (!router.routeReply(r)) ...host's own...
//
// Lines to an NPC that is already answering (anyone's request — host or
// remote) queue per-NPC and submit in arrival order; the asker's UI shows
// its usual waiting state meanwhile. Replies stream back to the requesting
// player only; everyone gets NpcMoodUpdate + NpcSpeechBubble so bystanders
// see the conversation happen.
class HostChatRouter {
   public:
    // Both outlive the router; world's NPC indices must stay stable (NPCs
    // are never deleted — existing invariant). No LlmClient parameter: each
    // Npc submits through its own client reference; the router only maps
    // request ids to players.
    HostChatRouter(World& world, NetServer& server);

    // Drains the server's chat events and submits queued lines for every
    // NPC that is free. Call once per host frame.
    void update();

    // Offers one drained LlmClient delta/reply. True when it belonged to a
    // remote player's request and was routed over the network; false means
    // it is the host's own conversation and the caller handles it as in
    // solo play.
    bool routeDelta(const ChatDelta& delta);
    bool routeReply(const ChatReply& reply);

    // Announces one NPC's mood to all players (NpcMoodUpdate). The host
    // loop calls this after its *own* conversations change an NPC, so
    // bystanders see mood shifts no matter who caused them.
    void announceNpcMood(int npcIndex);

   private:
    struct PendingLine {
        int playerId = -1;
        std::string text;
    };

    // Submits the front queued line for this NPC when it is not busy.
    void pumpNpc(int npcIndex);

    World& world_;
    NetServer& server_;

    // In-flight LLM request -> (remote player, NPC) it belongs to.
    std::unordered_map<std::uint64_t, std::pair<int, int>> inflight_;

    // Lines waiting for a busy NPC, per NPC index, in arrival order.
    std::unordered_map<int, std::deque<PendingLine>> queued_;
};

}  // namespace llm_npc
