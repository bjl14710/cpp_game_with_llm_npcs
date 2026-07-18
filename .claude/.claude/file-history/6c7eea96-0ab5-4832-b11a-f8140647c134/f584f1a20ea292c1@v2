#include "DialogueSession.hpp"

namespace llm_npc {

void DialogueSession::open(int npcIndex) {
    state_ = State::Talking;
    npcIndex_ = npcIndex;
    pendingId_ = 0;
    streamingText_.clear();
}

void DialogueSession::close() {
    state_ = State::Roaming;
    npcIndex_ = -1;
    pendingId_ = 0;
    streamingText_.clear();
}

void DialogueSession::submitted(std::uint64_t requestId) {
    if (state_ != State::Talking) return;
    pendingId_ = requestId;
    streamingText_.clear();
    state_ = State::WaitingReply;
}

bool DialogueSession::deltaArrived(std::uint64_t id, const std::string& text) {
    if (state_ != State::WaitingReply && state_ != State::Streaming) return false;
    if (id == 0 || id != pendingId_) return false;
    streamingText_ += text;
    state_ = State::Streaming;
    return true;
}

bool DialogueSession::replyArrived(std::uint64_t id, bool /*ok*/) {
    if (state_ != State::WaitingReply && state_ != State::Streaming) return false;
    if (id == 0 || id != pendingId_) return false;
    pendingId_ = 0;
    streamingText_.clear();
    state_ = State::Talking;
    return true;
}

}  // namespace llm_npc
