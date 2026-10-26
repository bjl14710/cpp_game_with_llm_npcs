#pragma once

#include <cstdint>
#include <string>

#include "StreamTagFilter.hpp"

namespace llm_npc {

// Tracks the player's conversation with one NPC at a time, including the
// in-flight LLM request and the partially streamed reply. Pure state machine
// (no rendering, no networking) so the app layer stays a thin shell:
//
//   Roaming --open()--> Talking --submitted()--> WaitingReply
//   WaitingReply --deltaArrived()--> Streaming
//   WaitingReply/Streaming --replyArrived()--> Talking
//   any state --close()--> Roaming
class DialogueSession {
   public:
    enum class State { Roaming, Talking, WaitingReply, Streaming };

    // Begin talking to the NPC at `npcIndex` (index into the world's roster).
    void open(int npcIndex);

    // Leave the conversation and return to roaming. Any in-flight request is
    // forgotten by the session; the app routes its late reply to the NPC's
    // history separately.
    void close();

    // True in every state except Roaming.
    bool isOpen() const { return state_ != State::Roaming; }

    State state() const { return state_; }

    // Roster index of the NPC being talked to; -1 while roaming.
    int npcIndex() const { return npcIndex_; }

    // The player submitted a line that became LLM request `requestId`.
    void submitted(std::uint64_t requestId);

    // A streamed text fragment arrived. Returns true only when `id` matches
    // the in-flight request of this open session; then the fragment passes
    // through a StreamTagFilter and only its displayable part is accumulated
    // (see lastDisplayDelta), so directive tags never show while streaming.
    bool deltaArrived(std::uint64_t id, const std::string& text);

    // The displayable text the most recent accepted delta added to
    // streamingText(); empty while a possible tag is being held back.
    const std::string& lastDisplayDelta() const { return lastDisplayDelta_; }

    // The final reply arrived. Returns true when it closes out this
    // session's in-flight request; streaming text is cleared either way the
    // request resolves (success or error).
    bool replyArrived(std::uint64_t id, bool ok);

    // Partial reply text accumulated while Streaming, tags removed.
    const std::string& streamingText() const { return streamingText_; }

    // The id of the request awaiting a reply; 0 when none.
    std::uint64_t pendingRequestId() const { return pendingId_; }

   private:
    State state_ = State::Roaming;
    int npcIndex_ = -1;
    std::uint64_t pendingId_ = 0;
    std::string streamingText_;
    std::string lastDisplayDelta_;
    StreamTagFilter tagFilter_;  // reset whenever a request starts or ends
};

}  // namespace llm_npc
