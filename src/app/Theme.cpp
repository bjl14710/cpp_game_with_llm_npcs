#include "Theme.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace llm_npc::ui {

namespace {

Font gUi{};
Font gDisplay{};
bool gCustom = false;

// The built-in font is a 10px bitmap; drawing it at `size` scales by size/10
// and DrawText spaces glyphs by the INTEGER size/10, which is why 14 through
// 18 used to render identically (AsciiText.hpp). DrawTextEx takes a float
// spacing, so every size is distinct here. A TTF atlas is drawn at zero
// spacing and lets the design's tracking do the rest.
float spacingFor(float size) { return gCustom ? 0.f : size / 10.f; }

const Font& fontFor(Face face) {
    if (gCustom) return face == Face::Display ? gDisplay : gUi;
    static Font fallback = GetFontDefault();
    return fallback;
}

// The design's smallest text is 10px; below that the bitmap font loses its
// counters, so nothing is ever drawn smaller.
float clampSize(float size) { return std::max(10.f, size); }

Font loadTtf(const std::string& path, int atlasSize) {
    Font font = LoadFontEx(path.c_str(), atlasSize, nullptr, 0);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

}  // namespace

Color alpha(Color c, float a) {
    c.a = static_cast<unsigned char>(std::clamp(a, 0.f, 1.f) * 255.f + 0.5f);
    return c;
}

Color bgAlpha(float a) { return alpha(kBg, a); }

void loadFonts(const std::string& assetsDir) {
    const std::string uiPath = assetsDir + "/fonts/ui.ttf";
    const std::string displayPath = assetsDir + "/fonts/display.ttf";
    if (!FileExists(uiPath.c_str()) || !FileExists(displayPath.c_str())) return;
    // 48px atlases: large enough that the 46px pause title is drawn at scale
    // 1 and the 11px kickers downsample with bilinear filtering.
    gUi = loadTtf(uiPath, 48);
    gDisplay = loadTtf(displayPath, 56);
    gCustom = gUi.texture.id != 0 && gDisplay.texture.id != 0;
}

void unloadFonts() {
    if (!gCustom) return;
    UnloadFont(gUi);
    UnloadFont(gDisplay);
    gCustom = false;
}

bool customFonts() { return gCustom; }

void text(Face face, const std::string& s, float x, float y, float size, Color color,
          float tracking) {
    size = clampSize(size);
    DrawTextEx(fontFor(face), s.c_str(), Vector2{std::round(x), std::round(y)}, size,
               spacingFor(size) + tracking, color);
}

float measure(Face face, const std::string& s, float size, float tracking) {
    size = clampSize(size);
    return MeasureTextEx(fontFor(face), s.c_str(), size, spacingFor(size) + tracking).x;
}

void textCentered(Face face, const std::string& s, float centerX, float y, float size,
                  Color color) {
    text(face, s, centerX - measure(face, s, size) * 0.5f, y, size, color);
}

void textRight(Face face, const std::string& s, float rightX, float y, float size,
               Color color) {
    text(face, s, rightX - measure(face, s, size), y, size, color);
}

std::vector<std::string> wrap(Face face, const std::string& s, float size, float maxWidth) {
    std::vector<std::string> out;
    std::string line;
    std::string word;
    const auto flush = [&]() {
        if (word.empty()) return;
        if (line.empty()) {
            line = word;
        } else if (measure(face, line + " " + word, size) <= maxWidth) {
            line += ' ';
            line += word;
        } else {
            out.push_back(line);
            line = word;
        }
        word.clear();
    };
    for (const char c : s) {
        if (c == '\n') {
            flush();
            out.push_back(line);
            line.clear();
        } else if (c == ' ' || c == '\t') {
            flush();
        } else {
            word += c;
        }
    }
    flush();
    if (!line.empty() || out.empty()) out.push_back(line);
    return out;
}

std::string ellipsize(Face face, const std::string& s, float size, float maxWidth) {
    if (measure(face, s, size) <= maxWidth) return s;
    std::string cut = s;
    while (!cut.empty() && measure(face, cut + "...", size) > maxWidth) cut.pop_back();
    return cut + "...";
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void panel(Rectangle r, float a) { DrawRectangleRec(r, bgAlpha(a)); }

void roundedPanel(Rectangle r, float a, Color ring) {
    const float roundness = (kRadius * 2.f) / std::min(r.width, r.height);
    DrawRectangleRounded(r, roundness, 6, bgAlpha(a));
    if (ring.a != 0) DrawRectangleRoundedLinesEx(r, roundness, 6, 1.f, ring);
}

void card(Rectangle r) {
    const float roundness = (kRadius * 2.f) / std::min(r.width, r.height);
    DrawRectangleRounded(r, roundness, 6, kSurface);
    DrawRectangleRoundedLinesEx(r, roundness, 6, 1.f, kNeutral800);
}

void ruleH(float x, float y, float w, Color c) {
    const float fade = std::min(48.f, w * 0.5f);
    const Color clear = alpha(c, 0.f);
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    const int ifade = static_cast<int>(fade), iw = static_cast<int>(w);
    DrawRectangleGradientH(ix, iy, ifade, 1, clear, c);
    DrawRectangle(ix + ifade, iy, iw - 2 * ifade, 1, c);
    DrawRectangleGradientH(ix + iw - ifade, iy, ifade, 1, c, clear);
}

void ruleV(float x, float y, float h, Color c) {
    const float fade = std::min(48.f, h * 0.5f);
    const Color clear = alpha(c, 0.f);
    const int ix = static_cast<int>(x), iy = static_cast<int>(y);
    const int ifade = static_cast<int>(fade), ih = static_cast<int>(h);
    DrawRectangleGradientV(ix, iy, 1, ifade, clear, c);
    DrawRectangle(ix, iy + ifade, 1, ih - 2 * ifade, c);
    DrawRectangleGradientV(ix, iy + ih - ifade, 1, ifade, c, clear);
}

void kicker(const std::string& s, float x, float y) {
    text(Face::Ui, upper(s), x, y, 11.f, kAccent300, 11.f * 0.12f);
}

void sectionHeader(const std::string& s, float x, float y) {
    text(Face::Ui, upper(s), x, y, 10.f, kNeutral500, 1.f);
}

void selection(Rectangle row) {
    DrawRectangleRec(row, alpha(kAccent, 0.12f));
    DrawRectangleRec(Rectangle{row.x, row.y, 2.f, row.height}, kAccent);
}

float keycapWidth(const std::string& label) { return measure(Face::Ui, label, 11.f) + 12.f; }

float keycap(const std::string& label, float x, float y, bool accent) {
    const float w = keycapWidth(label);
    const Rectangle r{x, y, w, 20.f};
    DrawRectangleRounded(r, 10.f / 20.f, 4, bgAlpha(0.6f));
    DrawRectangleRoundedLinesEx(r, 10.f / 20.f, 4, 1.f, accent ? kAccent600 : kNeutral600);
    text(Face::Ui, label, x + 6.f, y + 4.f, 11.f, accent ? kAccent200 : kNeutral200);
    return w;
}

float tagWidth(const std::string& label) { return measure(Face::Ui, label, 11.f) + 16.f; }

float tag(const std::string& label, float x, float y) {
    const float w = tagWidth(label);
    DrawRectangleRounded(Rectangle{x, y, w, 18.f}, 1.f, 6, kAccent800);
    text(Face::Ui, label, x + 8.f, y + 3.f, 11.f, kAccent100);
    return w;
}

float chipWidth(const std::string& label) { return measure(Face::Ui, label, 12.f) + 20.f; }

float chip(const std::string& label, float x, float y, bool lit) {
    const float w = chipWidth(label);
    DrawRectangleRounded(Rectangle{x, y, w, 22.f}, 1.f, 6, lit ? kAccent800 : kNeutral800);
    text(Face::Ui, label, x + 10.f, y + 4.f, 12.f, lit ? kAccent100 : kNeutral300);
    return w;
}

void glowDot(Vector2 center, bool lit) {
    if (lit) {
        DrawCircleV(center, 8.f, alpha(kAccent, 0.22f));
        DrawCircleV(center, 3.f, kAccent300);
    } else {
        DrawCircleV(center, 3.f, kNeutral700);
    }
}

Rectangle pill(Face face, const std::string& s, Vector2 anchor, float size, Color ink,
               bool ring) {
    const float w = measure(face, s, size) + 20.f;
    const float h = size + 10.f;
    const Rectangle r{std::round(anchor.x - w * 0.5f), std::round(anchor.y - h), w, h};
    const float roundness = 10.f / h;
    DrawRectangleRounded(r, roundness, 4, bgAlpha(0.72f));
    if (ring) {
        DrawRectangleRoundedLinesEx(r, roundness, 4, 1.f, kAccent700);
        DrawRectangle(static_cast<int>(anchor.x), static_cast<int>(anchor.y), 1, 14, kAccent);
    }
    text(face, s, r.x + 10.f, r.y + 5.f, size, ink);
    return r;
}

void button(Rectangle r, const std::string& label, ButtonKind kind, bool hover, bool enabled) {
    const float roundness = (kRadius * 2.f) / std::min(r.width, r.height);
    Color ring = kNeutral700;
    Color ink = kNeutral200;
    if (kind == ButtonKind::Primary) {
        ring = kAccent;
        ink = kAccent200;
    } else if (kind == ButtonKind::Ghost) {
        ring = alpha(kNeutral700, 0.f);
        ink = kNeutral400;
    }
    if (!enabled) {
        ring = alpha(ring, 0.45f);
        ink = alpha(ink, 0.45f);
    }
    if (hover && enabled) DrawRectangleRounded(r, roundness, 6, alpha(kAccent, 0.12f));
    if (ring.a != 0) DrawRectangleRoundedLinesEx(r, roundness, 6, 1.f, ring);
    const float size = 13.f;
    text(Face::Ui, label, r.x + (r.width - measure(Face::Ui, label, size)) * 0.5f,
         r.y + (r.height - size) * 0.5f, size, hover && enabled && kind == ButtonKind::Ghost
                                                   ? kNeutral200
                                                   : ink);
}

void input(Rectangle r, const std::string& value, const std::string& placeholder,
           bool focused, bool enabled) {
    const float roundness = (kRadius * 2.f) / std::min(r.width, r.height);
    DrawRectangleRounded(r, roundness, 6, kSurface);
    DrawRectangleRoundedLinesEx(r, roundness, 6, 1.f,
                                focused ? kAccent : (enabled ? kNeutral700 : kNeutral800));
    const float size = 14.f;
    const float textY = r.y + (r.height - size) * 0.5f;
    const float maxWidth = r.width - 28.f;
    if (value.empty()) {
        text(Face::Ui, ellipsize(Face::Ui, placeholder, size, maxWidth), r.x + 14.f, textY,
             size, kNeutral500);
    } else {
        // Show the tail while typing so the caret always sits on real text.
        std::string shown = value;
        while (shown.size() > 1 && measure(Face::Ui, shown, size) > maxWidth) {
            shown.erase(0, 1);
        }
        text(Face::Ui, shown, r.x + 14.f, textY, size, enabled ? kText : kNeutral500);
        if (focused) {
            DrawRectangle(static_cast<int>(r.x + 14.f + measure(Face::Ui, shown, size) + 2.f),
                          static_cast<int>(textY), 2, static_cast<int>(size), kAccent);
        }
    }
    if (value.empty() && focused) {
        DrawRectangle(static_cast<int>(r.x + 14.f), static_cast<int>(textY), 2,
                      static_cast<int>(size), kAccent);
    }
}

void toast(const std::string& s, float y) {
    const float size = 13.f;
    const float w = measure(Face::Ui, s, size) + 32.f;
    const float x = (static_cast<float>(GetScreenWidth()) - w) * 0.5f;
    roundedPanel(Rectangle{x, y, w, 32.f}, 0.84f, kNeutral800);
    text(Face::Ui, s, x + 16.f, y + (32.f - size) * 0.5f, size, kAccent200);
}

void vignette(float inner, float outer) {
    const float w = static_cast<float>(GetScreenWidth());
    const float h = static_cast<float>(GetScreenHeight());
    DrawRectangleRec(Rectangle{0.f, 0.f, w, h}, bgAlpha(inner));
    // Concentric rings step the wash from `inner` at the centre to `outer`
    // at the corners. Each ring adds only the alpha the composite still
    // needs, so the result is a smooth radial rather than a stack of discs.
    const Vector2 centre{w * 0.5f, h * 0.5f};
    const float rMax = std::sqrt(w * w + h * h) * 0.5f;
    constexpr int kRings = 18;
    float have = inner;
    for (int i = 0; i < kRings; ++i) {
        const float t0 = static_cast<float>(i) / kRings;
        const float t1 = static_cast<float>(i + 1) / kRings;
        const float want = inner + (outer - inner) * t1 * t1;
        const float add = have >= 1.f ? 0.f : (want - have) / (1.f - have);
        have = want;
        const float r0 = rMax * (0.30f + 0.70f * t0);
        const float r1 = (i + 1 == kRings) ? rMax * 1.5f : rMax * (0.30f + 0.70f * t1);
        DrawRing(centre, r0, r1, 0.f, 360.f, 64, bgAlpha(add));
        DrawRing(centre, r1, rMax * 1.5f, 0.f, 360.f, 64, bgAlpha(0.f));
    }
}

}  // namespace llm_npc::ui
