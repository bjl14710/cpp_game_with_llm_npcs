#pragma once

#include "raylib.h"

#include <string>
#include <vector>

// Nocturne: the one place the 2D UI gets its colours, type and widgets.
//
// Ported from the Redesign design file (claude.ai/design project "Game visual
// improvements", Redesign.dc.html, direction 1b) and its Nocturne design
// system (styles.css). Every token below is that stylesheet's value; the
// widgets are the handful of shapes the screens are built from — fading
// rules, pills, keycaps, tags, outlined buttons, selection bars — so Menu,
// DialogUI and the HUD draw the same thing when they mean the same thing.
//
// Two fonts, two roles: `Ui` for everything you read, `Display` for titles,
// NPC names, signs and cutscene slugs. The design pairs Inter with Instrument
// Serif. Both are OFL-licensed, and assets/LICENSES.md records a CC0-only
// asset policy, so neither is fetched here; loadFonts() picks up
// assets/fonts/ui.ttf and assets/fonts/display.ttf when they are present and
// falls back to raylib's built-in font otherwise. Layout, sizes and colours
// follow the design either way.
namespace llm_npc::ui {

// ---- tokens --------------------------------------------------------------
inline constexpr Color kBg{22, 24, 38, 255};
inline constexpr Color kSurface{35, 37, 50, 255};
inline constexpr Color kText{233, 233, 237, 255};
inline constexpr Color kAccent{145, 132, 217, 255};

inline constexpr Color kNeutral100{243, 245, 254, 255};
inline constexpr Color kNeutral200{228, 231, 245, 255};
inline constexpr Color kNeutral300{207, 211, 229, 255};
inline constexpr Color kNeutral400{178, 182, 202, 255};
inline constexpr Color kNeutral500{147, 151, 171, 255};
inline constexpr Color kNeutral600{117, 121, 140, 255};
inline constexpr Color kNeutral700{89, 93, 108, 255};
inline constexpr Color kNeutral800{63, 66, 77, 255};
inline constexpr Color kNeutral900{41, 43, 49, 255};

inline constexpr Color kAccent100{245, 244, 255, 255};
inline constexpr Color kAccent200{231, 229, 254, 255};
inline constexpr Color kAccent300{210, 206, 253, 255};
inline constexpr Color kAccent400{181, 171, 252, 255};
inline constexpr Color kAccent500{150, 138, 224, 255};
inline constexpr Color kAccent600{121, 108, 191, 255};
inline constexpr Color kAccent700{93, 82, 148, 255};
inline constexpr Color kAccent800{66, 58, 106, 255};
inline constexpr Color kAccent900{43, 39, 65, 255};

// Corner radius every rounded shape shares (design: --radius 8px).
inline constexpr float kRadius = 8.f;

// `c` at opacity `a` (0..1).
Color alpha(Color c, float a);
// The page background at opacity `a` — panels, pills and overlays are all
// this colour at different strengths (.55 dim, .72 pill, .94 panel).
Color bgAlpha(float a);

// ---- type ----------------------------------------------------------------
enum class Face { Ui, Display };

// Loads the optional TTFs from `<assetsDir>/fonts/`; call after InitWindow.
// Missing files are not an error — the built-in font is the fallback.
void loadFonts(const std::string& assetsDir);
void unloadFonts();
// True when the design's TTFs were found; false on the built-in fallback.
bool customFonts();

// Draws `s` with its top-left at (x, y). `tracking` is extra letter spacing
// in pixels (kickers use 0.12em).
void text(Face face, const std::string& s, float x, float y, float size, Color color,
          float tracking = 0.f);
float measure(Face face, const std::string& s, float size, float tracking = 0.f);
void textCentered(Face face, const std::string& s, float centerX, float y, float size,
                  Color color);
void textRight(Face face, const std::string& s, float rightX, float y, float size,
               Color color);
// Word-wraps `s` so every line measures at most `maxWidth` at `size`.
std::vector<std::string> wrap(Face face, const std::string& s, float size, float maxWidth);
// Truncates with "..." when `s` would not fit `maxWidth`.
std::string ellipsize(Face face, const std::string& s, float size, float maxWidth);
std::string upper(std::string s);

// ---- widgets -------------------------------------------------------------
// Flat panel: the background at opacity `a`, square corners.
void panel(Rectangle r, float a);
// Rounded panel with an optional 1px ring (pass alpha 0 ring for none).
void roundedPanel(Rectangle r, float a, Color ring);
// `.card`: surface fill, 1px neutral-800 ring, 8px radius.
void card(Rectangle r);
// Hairlines that fade to transparent over 48px at each end.
void ruleH(float x, float y, float w, Color c);
void ruleV(float x, float y, float h, Color c);
// 11px uppercase accent-300 with 0.12em tracking ("Lowell Square - Day 1").
void kicker(const std::string& s, float x, float y);
// 10px uppercase neutral-500 group label ("PLAY", "LOOK").
void sectionHeader(const std::string& s, float x, float y);
// Selected row: accent at 12% plus a 2px accent bar inset on the left.
void selection(Rectangle row);
// Key cap ("Esc", "T", "Enter"): 1px border, 5px radius, 11px label, 20px
// tall. Returns the width drawn.
float keycap(const std::string& label, float x, float y, bool accent = false);
float keycapWidth(const std::string& label);
// `.tag-accent` ("Irritated", "Current", "Conflicting accounts"): accent-800
// fill, accent-100 text, 18px tall. Returns the width drawn.
float tag(const std::string& label, float x, float y);
float tagWidth(const std::string& label);
// `.tag-neutral`: neutral-800 fill, neutral-300 text; `lit` swaps to accent.
float chip(const std::string& label, float x, float y, bool lit);
float chipWidth(const std::string& label);
// 6px status dot; `lit` is the accent with a soft glow, otherwise neutral-700.
void glowDot(Vector2 center, bool lit);
// World label pill: text centred above `anchor`, bg .72, 5px radius; `ring`
// adds the 1px accent-700 ring and the tick that marks the nearest talkable
// NPC. Returns the pill's rectangle.
Rectangle pill(Face face, const std::string& s, Vector2 anchor, float size, Color ink,
               bool ring);
// Buttons are outlined, never filled: Primary = accent ring and text,
// Secondary = neutral ring, Ghost = text only. Hover adds the accent tint.
enum class ButtonKind { Primary, Secondary, Ghost };
void button(Rectangle r, const std::string& label, ButtonKind kind, bool hover,
            bool enabled = true);
// Text field: surface fill, 1px ring (accent when focused), 8px radius,
// placeholder in neutral-500, caret while focused.
void input(Rectangle r, const std::string& value, const std::string& placeholder,
           bool focused, bool enabled = true);
// Bottom-centre notice pill.
void toast(const std::string& s, float y);
// Full-screen radial wash from `inner` opacity at the centre to `outer` at
// the corners (the dead screen's vignette).
void vignette(float inner, float outer);

}  // namespace llm_npc::ui
