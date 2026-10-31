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
// and a "thinking..." indicator. Pure rendering + key handling — owns no
// LLM/Npc state.
class DialogUI {
   public:
    explicit DialogUI(const sf::Font& font);

    // Returns the player's submitted text and clears the input. Empty string
    // means nothing was submitted this frame.
    std::string handleEvent(const sf::Event& event);

    void appendLine(TranscriptLine line);
    void setThinking(bool thinking, const std::string& speaker = {});
    void setInputEnabled(bool enabled);

    void render(sf::RenderTarget& target) const;

   private:
    const sf::Font& font_;
    std::deque<TranscriptLine> transcript_;
    std::string input_;
    bool thinking_ = false;
    std::string thinkingSpeaker_;
    bool inputEnabled_ = true;

    static constexpr std::size_t kMaxTranscriptLines = 200;

    void pushLine(TranscriptLine line);
};

}  // namespace llm_npc
