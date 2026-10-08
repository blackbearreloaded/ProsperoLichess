// ProsperoLichess - The font set every screen draws with.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/strings.hpp"
#include "gfx/draw_list.hpp"
#include "gfx/font.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pch::ui
{

// A loaded font and the GL texture holding its atlas.
struct FontRef
{
    const gfx::Font *font = nullptr;
    std::uint32_t texture = 0;

    float measure(std::string_view text, float size, float tracking = 0.0f) const
    {
        return font->measure(text, size, tracking);
    }
};

struct Fonts
{
    FontRef regular;  // Inter Regular: body text
    FontRef semibold; // Inter SemiBold: titles, labels, buttons
    FontRef display;  // Montserrat Medium: wide geometric headlines
    FontRef mono;     // DejaVu Sans Mono: numbers that must not jump, terminals
    FontRef pixel;    // Press Start 2P: an 8x8 bitmap face (the Pixel theme)
    FontRef hand;     // Patrick Hand: handwriting (the Sketch theme)
};

// Draws one line with its baseline at y; x is the left edge, centre or right
// edge depending on align. Returns the width.
inline float text(gfx::DrawList &list, const FontRef &font, std::string_view value, float x,
                  float baseline, float size, gfx::Color color, gfx::Align align = gfx::Align::left,
                  float tracking = 0.0f)
{
    return list.text(*font.font, font.texture, value, x, baseline, size, color, align, tracking);
}

// Upper case, for small tracked labels ("CONTINUE PLAYING"), in every
// alphabet the fonts have.
inline std::string upper(std::string_view value)
{
    return pch::upper(value);
}

// How often text_fit had to shrink a line or cut it. The PC tools switch
// `record` on and report it per language, so a translation that does not
// fit its place is found before a console shows it.
struct FitStats
{
    bool record = false;
    int shrunk = 0;
    int cut = 0;
    std::vector<std::string> cut_texts;
};
inline FitStats &fit_stats()
{
    static FitStats stats;
    return stats;
}
// Notes a line that was cut (a component ended it in dots), when recording.
inline void note_cut(std::string_view text)
{
    FitStats &stats = fit_stats();
    if (!stats.record)
        return;
    ++stats.cut;
    stats.cut_texts.emplace_back(text);
}

// The share of its size a line is drawn at to be no wider than max_width:
// 1 when it fits, never less than `least`.
inline float fit_scale(const FontRef &font, std::string_view value, float size, float max_width,
                       float tracking = 0.0f, float least = 0.78f)
{
    const float width = font.measure(value, size, tracking);
    return width <= max_width || width <= 0.0f ? 1.0f : std::max(least, max_width / width);
}

// One line that stays inside max_width, as translated text must: its
// letters shrink first (to `least` of their size at most), then it ends in
// an ellipsis. Text that fits is drawn exactly as text() draws it. Returns
// the width drawn.
inline float text_fit(gfx::DrawList &list, const FontRef &font, std::string_view value, float x,
                      float baseline, float size, float max_width, gfx::Color color,
                      gfx::Align align = gfx::Align::left, float tracking = 0.0f,
                      float least = 0.78f)
{
    const float scale = fit_scale(font, value, size, max_width, tracking, least);
    if (scale >= 1.0f)
        return text(list, font, value, x, baseline, size, color, align, tracking);
    const float small = size * scale;
    const float spread = tracking * scale;
    FitStats &stats = fit_stats();
    if (font.measure(value, small, spread) <= max_width + 0.5f)
    {
        if (stats.record)
            ++stats.shrunk;
        return text(list, font, value, x, baseline, small, color, align, spread);
    }
    note_cut(value);
    return text(list, font, font.font->fit(value, small, max_width, spread), x, baseline, small,
                color, align, spread);
}

// Draws word-wrapped text from its first baseline at y, at most max_lines
// lines (the last one ends in an ellipsis if text remains). Returns the
// baseline after the last line drawn.
inline float paragraph(gfx::DrawList &list, const FontRef &font, std::string_view value, float x,
                       float y, float size, float width, float line_height, gfx::Color color,
                       int max_lines = 99, gfx::Align align = gfx::Align::left)
{
    const std::vector<std::string> lines = font.font->wrap(value, size, width);
    int drawn = 0;
    for (const std::string &line : lines)
    {
        const bool last =
            drawn + 1 == max_lines && lines.size() > static_cast<std::size_t>(max_lines);
        if (last)
            text(list, font, font.font->fit(line + " \xE2\x80\xA6", size, width), x, y, size, color,
                 align);
        else
            text(list, font, line, x, y, size, color, align);
        y += line_height;
        if (++drawn >= max_lines)
            break;
    }
    return y;
}

} // namespace pch::ui
