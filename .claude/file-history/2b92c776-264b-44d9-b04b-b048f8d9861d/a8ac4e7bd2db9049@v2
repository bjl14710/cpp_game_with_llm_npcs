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

// Minimal dialog box: scrolling transcript on top, single-line input at
// bottom, a "thinking..." indicator, and an in-progress streamed NPC line
// that grows word by word. Pure rendering + input handling — owns no
// LLM/Npc state. raylib is polled per frame, so instead of the SFML event
// hook this exposes pollInput(), called once per frame while the dialog is
// open.
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
    void reset();

    // Draws the transcript + input box sized to the current window.
    void render() const;

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

    static constexpr std::size_t kMaxTranscriptLines = 200;

    void pushLine(TranscriptLine line);
};

}  // namespace llm_npc
