#include "Npc.hpp"

#include <utility>

namespace llm_npc {

Npc::Npc(Persona persona, LlmClient& client, int maxHistoryTurns)
    : persona_(std::move(persona)), client_(client), maxHistoryTurns_(maxHistoryTurns) {}

std::uint64_t Npc::ask(const std::string& playerLine) {
    // Send a snapshot of the current history; the player line is appended to
    // history only once a successful reply arrives. That way a failed request
    // can be retried without polluting context with unanswered user turns.
    pendingUserLine_ = playerLine;
    pendingId_ = client_.submit(persona_.renderSystemPrompt(), history_, playerLine);
    return pendingId_;
}

std::optional<std::string> Npc::onReplyArrived(const ChatReply& reply) {
    if (reply.id != pendingId_) return std::nullopt;
    pendingId_ = 0;

    if (!reply.ok) {
        // Don't append anything on failure — let the UI report the error and
        // let the player retry. Clear the pending user line too.
        pendingUserLine_.clear();
        return std::nullopt;
    }

    history_.push_back({"user", std::move(pendingUserLine_)});
    history_.push_back({"assistant", reply.content});
    pendingUserLine_.clear();
    trimHistory();
    return reply.content;
}

void Npc::trimHistory() {
    // History is a flat list of role/content turns. Two entries (user +
    // assistant) make one round-trip turn, hence the *2 cap.
    const int maxEntries = maxHistoryTurns_ * 2;
    if (static_cast<int>(history_.size()) <= maxEntries) return;
    const int drop = static_cast<int>(history_.size()) - maxEntries;
    history_.erase(history_.begin(), history_.begin() + drop);
}

}  // namespace llm_npc
