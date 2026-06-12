#pragma once

#include <SFML/Graphics.hpp>

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

// Minimal dialog box: scrolling transcript on top, single-line input at bottom,
// a "thinking..." indicator, and an in-progress streamed NPC line that grows
// word by word. Pure rendering + key handling — owns no LLM/Npc state.
class DialogUI {
   public:
    explicit DialogUI(const sf::Font& font);

    // Returns the player's submitted text and clears the input. Empty string
    // means nothing was submitted this frame.
    std::string handleEvent(const sf::Event& event);

    void appendLine(TranscriptLine line);
    void setThinking(bool thinking, const std::string& speaker = {});
    void setInputEnabled(bool enabled);

    // Ignore the next TextEntered event. Call when a key press opens this
    // dialog so that the key's character doesn't leak into the input box.
    void swallowNextTextEntered();

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

    void render(sf::RenderTarget& target) const;

   private:
    const sf::Font& font_;
    std::deque<TranscriptLine> transcript_;
    std::string input_;
    bool thinking_ = false;
    std::string thinkingSpeaker_;
    bool inputEnabled_ = true;
    bool swallowNext_ = false;
    bool streaming_ = false;
    std::string streamingSpeaker_;
    std::string streamingText_;

    static constexpr std::size_t kMaxTranscriptLines = 200;

    void pushLine(TranscriptLine line);
};

}  // namespace llm_npc
