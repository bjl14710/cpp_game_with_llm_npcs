#include "DialogUI.hpp"

#include "raylib.h"

#include <algorithm>
#include <cctype>
#include <utility>
#include <vector>

#include "Theme.hpp"

namespace llm_npc {

namespace {
constexpr float kPad = 20.f;        // inside the panel
constexpr float kHeaderBottom = 100.f;
constexpr float kInputHeight = 44.f;
constexpr float kHintsHeight = 20.f;
constexpr float kTextSize = 15.f;
constexpr float kTextLine = 20.f;
constexpr float kLabelLine = 16.f;
constexpr float kSystemLine = 18.f;
constexpr float kMessageGap = 10.f;

// First letters of the first two words ("Marge Dunn" -> "MD").
std::string initials(const std::string& name) {
    std::string out;
    bool atWordStart = true;
    for (const char c : name) {
        if (c == ' ') {
            atWordStart = true;
        } else if (atWordStart && out.size() < 2) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            atWordStart = false;
        }
    }
    return out;
}

// One drawn row of the transcript, resolved from newest to oldest so the
// panel fills from the bottom.
struct Row {
    std::string text;
    Color color;
    float size;
    float height;
    ui::Face face;
    bool centered;
    bool caret;  // the streaming line's cursor follows this text
};
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

void DialogUI::setSpeaker(const std::string& name, const std::string& role,
                          const std::string& location, const std::string& mood) {
    name_ = name;
    role_ = role;
    location_ = location;
    mood_ = mood;
}

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
    using ui::Face;
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    const float px = w - kPanelWidth;
    const float x = px + kPad;
    const float innerW = kPanelWidth - 2.f * kPad;

    ui::panel(Rectangle{px, 0.f, kPanelWidth, h}, 0.94f);
    ui::ruleV(px, 0.f, h, ui::alpha(ui::kText, 0.16f));

    // ---- header -----------------------------------------------------------
    if (name_.empty()) {
        ui::text(Face::Display, "Conversation", x, 26.f, 26.f, ui::kText);
        ui::text(Face::Ui, role_.empty() ? "Everyone nearby can hear you." : role_, x, 60.f, 12.f,
                 ui::kNeutral400);
    } else {
        const Vector2 avatar{x + 26.f, 20.f + 26.f};
        DrawCircleV(avatar, 26.f, ui::kAccent900);
        DrawRing(avatar, 25.f, 27.f, 0.f, 360.f, 48, ui::kAccent700);
        const std::string ini = initials(name_);
        ui::textCentered(Face::Display, ini, avatar.x, avatar.y - 10.f, 20.f, ui::kAccent200);
        const float nameX = x + 52.f + 14.f;
        float nameW = innerW - 66.f;
        if (!mood_.empty()) nameW -= ui::tagWidth(mood_) + 12.f;
        ui::text(Face::Display, ui::ellipsize(Face::Display, name_, 26.f, nameW), nameX, 18.f,
                 26.f, ui::kText);
        std::string sub = role_;
        if (!location_.empty()) sub += (sub.empty() ? "" : " - ") + location_;
        ui::text(Face::Ui, ui::ellipsize(Face::Ui, sub, 12.f, innerW - 66.f), nameX, 52.f, 12.f,
                 ui::kNeutral400);
        if (!mood_.empty()) ui::tag(mood_, x + innerW - ui::tagWidth(mood_), 24.f);
    }
    ui::ruleH(x, 86.f, innerW, ui::alpha(ui::kText, 0.16f));

    // ---- transcript, newest first until the space runs out ----------------
    const float hintsY = h - kPad - kHintsHeight;
    const Rectangle box{x, hintsY - 10.f - kInputHeight, innerW, kInputHeight};
    const float transcriptBottom = box.y - 16.f;
    const float available = transcriptBottom - kHeaderBottom;
    const float textW = innerW - 4.f;

    std::vector<Row> rows;  // reverse chronological while collecting
    float used = 0.f;
    const auto fits = [&](float height) { return used + height <= available; };
    const auto push = [&](Row row) {
        rows.push_back(std::move(row));
        used += rows.back().height;
    };
    const auto labelFor = [&](const TranscriptLine::Kind kind, const std::string& speaker) {
        if (kind == TranscriptLine::Kind::Player) {
            return Row{"YOU", ui::kAccent300, 10.f, kLabelLine, Face::Ui, false, false};
        }
        return Row{ui::upper(speaker), ui::kNeutral500, 10.f, kLabelLine, Face::Ui, false, false};
    };
    // Pushes one message (text lines, then its label) newest-line first.
    // Returns false when it did not fit whole.
    const auto pushMessage = [&](TranscriptLine::Kind kind, const std::string& speaker,
                                 const std::string& text, bool caret) {
        if (kind == TranscriptLine::Kind::System) {
            const std::vector<std::string> lines = ui::wrap(Face::Ui, text, 11.f, textW);
            if (!fits(lines.size() * kSystemLine + kMessageGap)) return false;
            for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
                push(Row{*it, ui::kNeutral500, 11.f, kSystemLine, Face::Ui, true, false});
            }
            used += kMessageGap;
            return true;
        }
        const std::vector<std::string> lines = ui::wrap(Face::Ui, text, kTextSize, textW);
        if (!fits(lines.size() * kTextLine + kLabelLine + kMessageGap)) return false;
        const Color ink = kind == TranscriptLine::Kind::Player ? ui::kNeutral200 : ui::kText;
        bool last = true;
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            push(Row{*it, ink, kTextSize, kTextLine, Face::Ui, false, caret && last});
            last = false;
        }
        if (!speaker.empty() || kind == TranscriptLine::Kind::Player) {
            push(labelFor(kind, speaker));
        }
        used += kMessageGap;
        return true;
    };

    bool roomLeft = true;
    if (streaming_ && !streamingText_.empty()) {
        roomLeft = pushMessage(TranscriptLine::Kind::Npc, streamingSpeaker_, streamingText_, true);
    } else if (thinking_ || streaming_) {
        const std::string speaker = streaming_ ? streamingSpeaker_ : thinkingSpeaker_;
        const std::string indicator =
            (speaker.empty() ? std::string("Thinking") : speaker + " is thinking") + "...";
        push(Row{indicator, ui::kNeutral500, 12.f, kSystemLine + kMessageGap, Face::Ui, false,
                 false});
    }
    for (auto it = transcript_.rbegin(); roomLeft && it != transcript_.rend(); ++it) {
        roomLeft = pushMessage(it->kind, it->speaker, it->text, false);
    }

    float y = transcriptBottom - used;
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        const Row& row = *it;
        if (row.centered) {
            ui::textCentered(row.face, row.text, px + kPanelWidth * 0.5f, y, row.size, row.color);
        } else {
            ui::text(row.face, row.text, x, y, row.size, row.color);
            if (row.caret) {
                const float cx = x + ui::measure(row.face, row.text, row.size) + 4.f;
                DrawRectangle(static_cast<int>(cx), static_cast<int>(y + 1.f), 2, 14,
                              ui::kAccent);
            }
        }
        y += row.height;
    }

    // ---- input ------------------------------------------------------------
    const float roundness = (ui::kRadius * 2.f) / kInputHeight;
    DrawRectangleRounded(box, roundness, 6, ui::kSurface);
    DrawRectangleRoundedLinesEx(box, roundness, 6, 1.f,
                                inputEnabled_ ? ui::kAccent : ui::kNeutral800);
    const float textY = box.y + (kInputHeight - 14.f) * 0.5f;
    ui::text(Face::Ui, ">", box.x + 14.f, textY, 14.f,
             inputEnabled_ ? ui::kAccent300 : ui::kNeutral600);
    const float fieldX = box.x + 32.f;
    const float fieldW = box.width - 46.f;
    if (input_.empty()) {
        const std::string placeholder = inputEnabled_
                                            ? "Say something" +
                                                  (name_.empty() ? std::string("")
                                                                 : " to " + name_) +
                                                  "..."
                                            : "Waiting for a reply...";
        ui::text(Face::Ui, ui::ellipsize(Face::Ui, placeholder, 14.f, fieldW), fieldX, textY,
                 14.f, ui::kNeutral500);
        if (inputEnabled_) {
            DrawRectangle(static_cast<int>(fieldX), static_cast<int>(textY), 2, 14, ui::kAccent);
        }
    } else {
        std::string shown = input_;
        while (shown.size() > 1 && ui::measure(Face::Ui, shown, 14.f) > fieldW) shown.erase(0, 1);
        ui::text(Face::Ui, shown, fieldX, textY, 14.f, inputEnabled_ ? ui::kText : ui::kNeutral500);
        if (inputEnabled_) {
            DrawRectangle(static_cast<int>(fieldX + ui::measure(Face::Ui, shown, 14.f) + 2.f),
                          static_cast<int>(textY), 2, 14, ui::kAccent);
        }
    }

    // ---- key hints --------------------------------------------------------
    float hx = x;
    const auto hint = [&](const char* key, const char* what) {
        hx += ui::keycap(key, hx, hintsY) + 5.f;
        ui::text(Face::Ui, what, hx, hintsY + 4.f, 11.f, ui::kNeutral500);
        hx += ui::measure(Face::Ui, what, 11.f) + 14.f;
    };
    hint("Enter", "send");
    hint("Esc", "leave");
    hint("F1", "like");
    hint("F2", "flag reply");
}

}  // namespace llm_npc
