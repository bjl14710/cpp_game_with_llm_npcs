#include "DialogUI.hpp"

#include <algorithm>
#include <utility>

namespace llm_npc {

namespace {
constexpr float kPadding = 16.f;
constexpr float kInputHeight = 48.f;
constexpr float kLineHeight = 22.f;
constexpr unsigned kFontSize = 16;

sf::Color colorFor(TranscriptLine::Kind k) {
    switch (k) {
        case TranscriptLine::Kind::Player: return sf::Color(180, 220, 255);
        case TranscriptLine::Kind::Npc:    return sf::Color(255, 230, 180);
        case TranscriptLine::Kind::System: return sf::Color(180, 180, 180);
    }
    return sf::Color::White;
}

// Wrap a string to roughly `maxChars` per line. Word-aware: never splits a
// word unless the word itself exceeds maxChars. Crude but enough for chat.
std::vector<std::string> wrap(const std::string& s, std::size_t maxChars) {
    std::vector<std::string> out;
    std::string current;
    std::string word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        if (current.empty()) {
            current = word;
        } else if (current.size() + 1 + word.size() <= maxChars) {
            current += ' ';
            current += word;
        } else {
            out.push_back(current);
            current = word;
        }
        word.clear();
    };
    for (char c : s) {
        if (c == '\n') {
            flushWord();
            out.push_back(current);
            current.clear();
        } else if (c == ' ' || c == '\t') {
            flushWord();
        } else {
            word += c;
            if (word.size() > maxChars) {
                // Oversized word: hard-break.
                if (!current.empty()) {
                    out.push_back(current);
                    current.clear();
                }
                out.push_back(word);
                word.clear();
            }
        }
    }
    flushWord();
    if (!current.empty()) out.push_back(current);
    return out;
}
}  // namespace

DialogUI::DialogUI(const sf::Font& font) : font_(font) {}

std::string DialogUI::handleEvent(const sf::Event& event) {
    if (!inputEnabled_) return {};

    if (event.type == sf::Event::TextEntered) {
        const auto unicode = event.text.unicode;
        if (unicode == 8) {  // backspace
            if (!input_.empty()) input_.pop_back();
        } else if (unicode == 13 || unicode == 10) {  // enter
            // Handled in KeyPressed below to avoid double-fire on some platforms.
        } else if (unicode >= 32 && unicode < 127) {
            input_ += static_cast<char>(unicode);
        }
        return {};
    }

    if (event.type == sf::Event::KeyPressed &&
        (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Return)) {
        if (input_.empty()) return {};
        std::string submitted = std::move(input_);
        input_.clear();
        return submitted;
    }
    return {};
}

void DialogUI::appendLine(TranscriptLine line) { pushLine(std::move(line)); }

void DialogUI::pushLine(TranscriptLine line) {
    transcript_.push_back(std::move(line));
    while (transcript_.size() > kMaxTranscriptLines) transcript_.pop_front();
}

void DialogUI::setThinking(bool thinking, const std::string& speaker) {
    thinking_ = thinking;
    thinkingSpeaker_ = speaker;
}

void DialogUI::setInputEnabled(bool enabled) { inputEnabled_ = enabled; }

void DialogUI::render(sf::RenderTarget& target) const {
    const sf::Vector2u sz = target.getSize();
    const float w = static_cast<float>(sz.x);
    const float h = static_cast<float>(sz.y);

    const float transcriptBottom = h - kInputHeight - kPadding;
    const std::size_t wrapChars = std::max<std::size_t>(20, static_cast<std::size_t>((w - 2 * kPadding) / 9.0f));

    // Collect wrapped, colored lines from newest backward until we run out of
    // vertical space. Then render them in chronological order.
    struct RenderedLine { std::string text; sf::Color color; };
    std::vector<RenderedLine> rendered;
    float used = 0.f;
    const float available = transcriptBottom - kPadding;

    if (thinking_) {
        std::string indicator = "... " + (thinkingSpeaker_.empty() ? std::string("(thinking)")
                                                                    : thinkingSpeaker_ + " is thinking...");
        rendered.push_back({indicator, sf::Color(140, 140, 140)});
        used += kLineHeight;
    }

    for (auto it = transcript_.rbegin(); it != transcript_.rend(); ++it) {
        const auto& line = *it;
        std::string prefix = line.speaker.empty() ? "" : line.speaker + ": ";
        auto wrapped = wrap(prefix + line.text, wrapChars);
        // Push in reverse so chronological order is preserved once we reverse the whole vector.
        for (auto wi = wrapped.rbegin(); wi != wrapped.rend(); ++wi) {
            if (used + kLineHeight > available) break;
            rendered.push_back({*wi, colorFor(line.kind)});
            used += kLineHeight;
        }
        if (used + kLineHeight > available) break;
    }

    std::reverse(rendered.begin(), rendered.end());

    float y = transcriptBottom - static_cast<float>(rendered.size()) * kLineHeight;
    for (const auto& rl : rendered) {
        sf::Text t(rl.text, font_, kFontSize);
        t.setFillColor(rl.color);
        t.setPosition(kPadding, y);
        target.draw(t);
        y += kLineHeight;
    }

    // Input box.
    sf::RectangleShape box(sf::Vector2f(w - 2 * kPadding, kInputHeight));
    box.setPosition(kPadding, h - kInputHeight - kPadding * 0.5f);
    box.setFillColor(sf::Color(30, 30, 36));
    box.setOutlineColor(inputEnabled_ ? sf::Color(120, 160, 220) : sf::Color(70, 70, 70));
    box.setOutlineThickness(2.f);
    target.draw(box);

    sf::Text prompt(std::string("> ") + input_ + (inputEnabled_ ? "_" : ""), font_, kFontSize);
    prompt.setFillColor(inputEnabled_ ? sf::Color::White : sf::Color(120, 120, 120));
    prompt.setPosition(kPadding + 10.f, h - kInputHeight - kPadding * 0.5f + 12.f);
    target.draw(prompt);
}

}  // namespace llm_npc
