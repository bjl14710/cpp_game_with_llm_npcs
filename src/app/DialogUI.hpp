#pragma once

#include <deque>
#include <string>

namespace llm_npc {

// One line of the scrolling transcript.
struct TranscriptLine {
    enum class Kind { Player, Npc, System };
    Kind kind;
    std::string speaker;
    std::string text;
};

// The conversation panel, in the Nocturne design (src/app/Theme.hpp): a
// 460px panel on the right with the NPC's name, role, whereabouts and mood
// in its header, a bottom-anchored transcript with a label above each
// message, an in-progress streamed NPC line that grows word by word with a
// caret, a "thinking" indicator, and the input box with its key hints at the
// bottom. Pure rendering + input handling — owns no LLM/Npc state. raylib is
// polled per frame, so this exposes pollInput(), called once per frame while
// the dialog is open.
class DialogUI {
   public:
    DialogUI() = default;

    // Reads this frame's typed characters/backspace/enter into the input
    // line. Returns the submitted text when Enter was pressed (and clears
    // the input); empty string otherwise.
    std::string pollInput();

    void appendLine(TranscriptLine line);
    void setThinking(bool thinking, const std::string& speaker = {});
    void setInputEnabled(bool enabled);

    // Who the panel's header is about. `role` and `location` join into the
    // sub-line ("Baker - at the market"); `mood` is the tag on the right.
    // An empty `name` is a group conversation: no avatar, a plain title.
    void setSpeaker(const std::string& name, const std::string& role,
                    const std::string& location, const std::string& mood);

    // Updates just the mood tag; main.cpp calls this each frame so the tag
    // follows the NPC's reaction to the last exchange.
    void setMood(const std::string& mood) { mood_ = mood; }

    // Ignore any characters already pending this frame. Call when a key
    // press opens this dialog so the talk key's character doesn't leak into
    // the input box.
    void swallowPendingText();

    // Starts an in-progress streamed reply from `speaker`; shown live at the
    // bottom of the transcript until endStreaming().
    void beginStreaming(const std::string& speaker);

    // Appends one streamed fragment to the in-progress line.
    void appendStreamingDelta(const std::string& text);

    // Removes the in-progress line. The caller appends the final transcript
    // line (or an error line) itself from the completed reply.
    void endStreaming();

    // Clears transcript, input, and streaming state for a fresh conversation.
    // The header set by setSpeaker() is kept; the caller sets it per NPC.
    void reset();

    // Draws the panel sized to the current window.
    void render() const;

    // The panel's width; main.cpp dims the world to its left.
    static constexpr float kPanelWidth = 460.f;

   private:
    std::deque<TranscriptLine> transcript_;
    std::string input_;
    bool thinking_ = false;
    std::string thinkingSpeaker_;
    bool inputEnabled_ = true;
    bool swallowThisFrame_ = false;
    bool streaming_ = false;
    std::string streamingSpeaker_;
    std::string streamingText_;
    std::string name_;
    std::string role_;
    std::string location_;
    std::string mood_;

    static constexpr std::size_t kMaxTranscriptLines = 200;

    void pushLine(TranscriptLine line);
};

}  // namespace llm_npc
