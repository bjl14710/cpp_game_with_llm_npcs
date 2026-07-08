#include "DialogUI.hpp"

#include "raylib.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace llm_npc {

namespace {
constexpr float kPadding = 16.f;
constexpr float kInputHeight = 48.f;
constexpr float kLineHeight = 22.f;
constexpr int kFontSize = 16;

Color colorFor(TranscriptLine::Kind k) {
    switch (k) {
        case TranscriptLine::Kind::Player: return Color{180, 220, 255, 255};
        case TranscriptLine::Kind::Npc:    return Color{255, 230, 180, 255};
        case TranscriptLine::Kind::System: return Color{180, 180, 180, 255};
    }
    return WHITE;
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

std::string DialogUI::pollInput() {
    // Drain this frame's typed characters even when swallowed/disabled so a
    // burst of keystrokes can't replay later.
    int ch = GetCharPressed();
    const bool accept = inputEnabled_ && !swallowThisFrame_;
    while (ch != 0) {
        if (accept && ch >= 32 && ch < 127) input_ += static_cast<char>(ch);
        ch = GetCharPressed();
    }
    swallowThisFrame_ = false;
    if (!inputEnabled_) return {};

    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) &&
        !input_.empty()) {
        input_.pop_back();
    }
    if (IsKeyPressed(KEY_ENTER) && !input_.empty()) {
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

void DialogUI::swallowPendingText() { swallowThisFrame_ = true; }

void DialogUI::beginStreaming(const std::string& speaker) {
    streaming_ = true;
    streamingSpeaker_ = speaker;
    streamingText_.clear();
}

void DialogUI::appendStreamingDelta(const std::string& text) {
    if (streaming_) streamingText_ += text;
}

void DialogUI::endStreaming() {
    streaming_ = false;
    streamingSpeaker_.clear();
    streamingText_.clear();
}

void DialogUI::reset() {
    transcript_.clear();
    input_.clear();
    thinking_ = false;
    thinkingSpeaker_.clear();
    swallowThisFrame_ = false;
    endStreaming();
}

void DialogUI::render() const {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());

    const float transcriptBottom = h - kInputHeight - kPadding;
    const std::size_t wrapChars =
        std::max<std::size_t>(20, static_cast<std::size_t>((w - 2 * kPadding) / 9.0f));

    // Collect wrapped, colored lines from newest backward until we run out of
    // vertical space. Then render them in chronological order.
    struct RenderedLine {
        std::string text;
        Color color;
    };
    std::vector<RenderedLine> rendered;
    float used = 0.f;
    const float available = transcriptBottom - kPadding;

    if (streaming_ && !streamingText_.empty()) {
        // Live, partially-streamed NPC line with a cursor.
        const std::string prefix = streamingSpeaker_.empty() ? "" : streamingSpeaker_ + ": ";
        auto wrapped = wrap(prefix + streamingText_ + "_", wrapChars);
        for (auto wi = wrapped.rbegin(); wi != wrapped.rend(); ++wi) {
            if (used + kLineHeight > available) break;
            rendered.push_back({*wi, colorFor(TranscriptLine::Kind::Npc)});
            used += kLineHeight;
        }
    } else if (thinking_ || streaming_) {
        const std::string speaker = streaming_ ? streamingSpeaker_ : thinkingSpeaker_;
        std::string indicator = "... " + (speaker.empty() ? std::string("(thinking)")
                                                          : speaker + " is thinking...");
        rendered.push_back({indicator, Color{140, 140, 140, 255}});
        used += kLineHeight;
    }

    for (auto it = transcript_.rbegin(); it != transcript_.rend(); ++it) {
        const auto& line = *it;
        std::string prefix = line.speaker.empty() ? "" : line.speaker + ": ";
        auto wrapped = wrap(prefix + line.text, wrapChars);
        // Reverse push preserves chronological order after the final reverse.
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
        DrawText(rl.text.c_str(), static_cast<int>(kPadding), static_cast<int>(y),
                 kFontSize, rl.color);
        y += kLineHeight;
    }

    // Input box.
    const Rectangle box{kPadding, h - kInputHeight - kPadding * 0.5f,
                        w - 2 * kPadding, kInputHeight};
    DrawRectangleRec(box, Color{30, 30, 36, 255});
    DrawRectangleLinesEx(box, 2.f,
                         inputEnabled_ ? Color{120, 160, 220, 255} : Color{70, 70, 70, 255});

    const std::string prompt = "> " + input_ + (inputEnabled_ ? "_" : "");
    DrawText(prompt.c_str(), static_cast<int>(kPadding + 10.f),
             static_cast<int>(box.y + 12.f), kFontSize,
             inputEnabled_ ? WHITE : Color{120, 120, 120, 255});
}

}  // namespace llm_npc
