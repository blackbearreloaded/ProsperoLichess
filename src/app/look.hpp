// ProsperoLichess - The house look: palette, lit panels, figures, small charts and icons.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Every screen draws with these, so they agree on what a panel is, how the
// focused thing is lit, what a number looks like and how data arrives:
//
//   - each part of the app has its own accent and its own sky (Section); the
//     app eases the backdrop toward the sky of whatever is showing;
//   - panels are dark gradients with a hairline; a lit panel takes its accent
//     into the edge and the fill, and the focused one floats on a shadow with
//     a breathing glow (panel, lift, ring);
//   - figures count up, clocks are monospaced, labels are small tracked
//     capitals in the accent (figure, ticker, kicker);
//   - data arrives instead of appearing: lines, rings and bars grow from zero
//     with a stagger (rise, sparkline, gauge, bars, donut).

#pragma once

#include "gfx/draw_list.hpp"
#include "ui/fonts.hpp"

#include <span>
#include <string>
#include <string_view>

namespace pch::app::look
{

using gfx::Color;
using gfx::Rect;

// ---- palette ----------------------------------------------------------------

inline const Color kInk = Color::rgb(0xeef2ff);   // text and light
inline const Color kNight = Color::rgb(0x070b16); // text on a bright accent
inline const Color kClear = Color::rgb(0x000000, 0.0f);
inline const Color kGood = Color::rgb(0x5fd68b); // a rise, a win, a solved puzzle
inline const Color kBad = Color::rgb(0xff6b6b);  // a fall, a loss, a mistake
inline const Color kGold = Color::rgb(0xffc857);

inline constexpr float kMuted = 0.68f;  // alpha of secondary text
inline constexpr float kFaint = 0.42f;  // alpha of tertiary text and empty states
inline constexpr float kRadius = 22.0f; // panels
inline constexpr float kPad = 24.0f;    // inside a panel

// The parts of the app. Each has an accent and a sky.
enum class Section : int
{
    home,
    puzzles,
    play,
    watch,
    profile,
    settings,
    // Full screens: the same hues, lower and quieter, so the board leads.
    game,
    puzzle,
    tv,
    account,
    boot,
    count,
};

struct Mood
{
    Color accent;
    Color sky[4]; // the aurora backdrop: top, bottom, cloud, second cloud
};
const Mood &mood(Section section);
inline Color accent(Section section)
{
    return mood(section).accent;
}

// The colours of the five ways to play, as Lichess orders them.
enum class Speed : int
{
    bullet,
    blitz,
    rapid,
    classical,
    correspondence,
};
Color speed_color(Speed speed);
// "bullet", "blitz", ... (anything else is rapid).
Speed speed_from(std::string_view key);
// The speed's sign in one colour: a bullet, a bolt, a rabbit's dash, an
// hourglass, a letter.
void speed_icon(gfx::DrawList &list, const Rect &box, Speed speed, Color ink);

// ---- surfaces ---------------------------------------------------------------

// A panel. lit 0..1 brings the accent into its edge and fill.
void panel(gfx::DrawList &list, const Rect &r, float lit = 0.0f, Color accent = kInk,
           float radius = kRadius);
// The shadow and the breathing glow under a focused panel; draw it first.
// amount 0..1 is how focused; calm: reduced motion (the glow stands still).
void lift(gfx::DrawList &list, const Rect &r, float amount, Color accent, float time, bool calm,
          float radius = kRadius);
// The focus ring: one per screen, gliding (ui::SpringRect) between panels.
void ring(gfx::DrawList &list, const Rect &r, Color accent, float alpha = 1.0f,
          float radius = kRadius);
// A hairline between two groups inside a panel.
void rule(gfx::DrawList &list, float x, float y, float width, float alpha = 0.1f);
// A soft disc of coloured light behind a board or a piece of art.
void halo(gfx::DrawList &list, const Rect &r, Color color, float alpha = 0.3f);
// A board (or any square art) that stands off the page: shadow, light, rim.
void frame_board(gfx::DrawList &list, const Rect &squares, Color accent, float glow = 0.0f);

// ---- type -------------------------------------------------------------------

// Small tracked capitals ("DAILY PUZZLE"). Returns the width. With a
// max_width the line stays inside it, as translated text must (ui::text_fit).
float kicker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, Color color, gfx::Align align = gfx::Align::left, float size = 16.0f,
             float max_width = 0.0f);
// A number that matters (a rating, a score), in the label face.
float figure(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, float size, Color color, gfx::Align align = gfx::Align::left);
// A number that keeps changing (a clock, a counter), in the monospaced face
// so it does not jitter.
float ticker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, float size, Color color, gfx::Align align = gfx::Align::left);
// An up or down mark and the change ("+12" green, "-8" red, nothing for 0).
// Returns the width drawn.
float delta(gfx::DrawList &list, const ui::Fonts &fonts, int change, float x, float baseline,
            float size, gfx::Align align = gfx::Align::left);
// A filled capsule with a word: a tag on art ("LIVE", "RATED", "GM").
float tag(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x, float cy,
          Color fill, Color ink, gfx::Align align = gfx::Align::left, float size = 15.0f);

// A short text set in a narrow place (a card's column), where a translation
// may need a second line. It stays on one line when ui::text_fit would keep
// it whole inside `width`, in the size that gives it; else it is broken in
// two, in the largest size down to the smallest of ui::text_fit at which
// both lines fit. Text that fits no way is cut, and counted as ui::text_fit
// counts it. Draw the lines with ui::text at the size given here.
struct Fitted
{
    std::string first;
    std::string second; // empty: one line
    float size = 0.0f;
};
Fitted fit_lines(const ui::FontRef &font, std::string_view text, float size, float width);

// ---- arrivals ---------------------------------------------------------------

// 0..1 for the index-th part of something that entered `elapsed` seconds
// ago: parts arrive `step` apart and take `duration` each.
float rise(float elapsed, int index = 0, float step = 0.05f, float duration = 0.45f);
// The offset (pixels, falling to 0) that goes with rise(): parts settle upward.
inline float settle(float risen, float distance = 22.0f)
{
    return (1.0f - risen) * distance;
}

// ---- data -------------------------------------------------------------------

// A line over its own range with a soft area under it and a dot on the
// newest value. grow 0..1 raises it from the baseline. Fewer than two values
// draw a quiet baseline: the honest empty state.
void sparkline(gfx::DrawList &list, const Rect &r, std::span<const float> values, Color color,
               float grow = 1.0f, bool area = true);
// A ring that fills clockwise from 12 o'clock over a quiet track.
void gauge(gfx::DrawList &list, float cx, float cy, float radius, float thickness, float share,
           Color color, float grow = 1.0f);
// Shares of a whole as arcs with gaps (wins, draws, losses).
void donut(gfx::DrawList &list, float cx, float cy, float radius, float thickness,
           std::span<const float> shares, std::span<const Color> colors, float grow = 1.0f);
// Upright bars over a common baseline; the last one is the bright one.
void bars(gfx::DrawList &list, const Rect &r, std::span<const float> values, Color color,
          float grow = 1.0f);
// A thin level: time left on a clock, progress through a run. low: it turns
// to the warning colour and glows.
void level(gfx::DrawList &list, const Rect &r, float share, Color color, bool low = false);
// A dot with a ring that spreads and fades: "this is live".
void live_dot(gfx::DrawList &list, float cx, float cy, float radius, Color color, float time,
              bool calm);

// Gives components the section's accent (their own copy of the theme).
template <typename... Components> void tint(Color accent, Components &...components)
{
    ((components.style.theme.primary = accent, components.style.theme.accent = accent,
      components.style.theme.on_primary = kNight),
     ...);
}

} // namespace pch::app::look
