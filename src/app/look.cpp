// ProsperoLichess - The house look: palette, lit panels, figures, small charts and icons.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/look.hpp"

#include "core/tween.hpp"
#include "ui/motion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace pch::app::look
{

namespace
{

constexpr float kTau = 6.2831853f;
const Color kBlack = Color::rgb(0x000000);
const Color kPanelTop = Color::rgb(0x1c2342);
const Color kPanelBottom = Color::rgb(0x0f142a);

Mood make(std::uint32_t accent, std::uint32_t top, std::uint32_t bottom, std::uint32_t cloud,
          std::uint32_t second)
{
    return {Color::rgb(accent),
            {Color::rgb(top), Color::rgb(bottom), Color::rgb(cloud), Color::rgb(second)}};
}

} // namespace

const Mood &mood(Section section)
{
    static const std::array<Mood, static_cast<std::size_t>(Section::count)> kMoods = {
        make(0x7cd8ff, 0x070b1c, 0x151b40, 0x2f55d4, 0xb44bd0), // home: night blue and magenta
        make(0xffc857, 0x0d0a16, 0x241a2e, 0xb8691f, 0x6a3fc8), // puzzles: amber over violet
        make(0x8fe36f, 0x06101a, 0x0f2a2e, 0x1f9d6b, 0x2f6fd9), // play: green over blue
        make(0xff7a96, 0x0f0818, 0x2a1232, 0xc93a66, 0x6b3fd9), // watch: rose
        make(0xb9a2ff, 0x090a22, 0x1c1a4c, 0x6a4be0, 0x2a9fd8), // profile: violet
        make(0x6fe3cf, 0x07101a, 0x12283a, 0x1f8f9a, 0x3a57b8), // settings: teal
        make(0x8fe36f, 0x05080f, 0x0d1424, 0x15503f, 0x1d2f66), // game: the play hues, low
        make(0xffc857, 0x07070d, 0x15121f, 0x5e3a1a, 0x33245e), // puzzle: the puzzle hues, low
        make(0xff7a96, 0x08060e, 0x180d20, 0x5e2238, 0x33245e), // tv: the watch hues, low
        make(0x7cd8ff, 0x070b1c, 0x141a3c, 0x274aa8, 0x7a3fb0), // account
        make(0x7cd8ff, 0x04060f, 0x0e1230, 0x2f55d4, 0xb44bd0), // boot
    };
    const int index =
        std::clamp(static_cast<int>(section), 0, static_cast<int>(Section::count) - 1);
    return kMoods[static_cast<std::size_t>(index)];
}

Color speed_color(Speed speed)
{
    switch (speed)
    {
    case Speed::bullet:
        return Color::rgb(0xff8a5c);
    case Speed::blitz:
        return Color::rgb(0xffc857);
    case Speed::rapid:
        return Color::rgb(0x8fe36f);
    case Speed::classical:
        return Color::rgb(0x7cd8ff);
    case Speed::correspondence:
        return Color::rgb(0xb9a2ff);
    }
    return Color::rgb(0x8fe36f);
}

Speed speed_from(std::string_view key)
{
    if (key == "bullet" || key == "ultraBullet")
        return Speed::bullet;
    if (key == "blitz")
        return Speed::blitz;
    if (key == "classical")
        return Speed::classical;
    if (key == "correspondence")
        return Speed::correspondence;
    return Speed::rapid;
}

void speed_icon(gfx::DrawList &list, const Rect &box, Speed speed, Color ink)
{
    const float s = std::min(box.w, box.h);
    const float cx = box.cx();
    const float cy = box.cy();
    const float pen = std::max(s * 0.09f, 2.2f);
    switch (speed)
    {
    case Speed::bullet:
    {
        // A round-nosed shell flying right, with two lines of speed behind it.
        list.rounded_rect({cx - s * 0.12f, cy - s * 0.17f, s * 0.36f, s * 0.34f}, s * 0.05f, ink);
        list.circle(cx + s * 0.24f, cy, s * 0.17f, ink);
        list.rounded_rect({cx - s * 0.46f, cy - s * 0.13f, s * 0.24f, pen}, pen * 0.5f, ink);
        list.rounded_rect({cx - s * 0.38f, cy + s * 0.13f - pen, s * 0.16f, pen}, pen * 0.5f, ink);
        break;
    }
    case Speed::blitz:
    {
        const float u = s * 0.25f;
        const float bolt[] = {cx + u * 0.5f, cy - u * 1.8f,  cx - u * 1.1f, cy + u * 0.25f,
                              cx - u * 0.1f, cy + u * 0.25f, cx - u * 0.5f, cy + u * 1.8f,
                              cx + u * 1.1f, cy - u * 0.25f, cx + u * 0.1f, cy - u * 0.25f};
        list.polygon(bolt, 6, ink);
        break;
    }
    case Speed::rapid:
    {
        // Two chevrons: quick, not frantic.
        for (int i = 0; i < 2; ++i)
        {
            const float x = cx - s * 0.3f + static_cast<float>(i) * s * 0.34f;
            list.line(x, cy - s * 0.3f, x + s * 0.28f, cy, pen * 1.2f, ink);
            list.line(x + s * 0.28f, cy, x, cy + s * 0.3f, pen * 1.2f, ink);
        }
        break;
    }
    case Speed::classical:
    {
        // An hourglass.
        list.rounded_rect({cx - s * 0.3f, cy - s * 0.42f, s * 0.6f, pen}, pen * 0.5f, ink);
        list.rounded_rect({cx - s * 0.3f, cy + s * 0.42f - pen, s * 0.6f, pen}, pen * 0.5f, ink);
        list.triangle({cx - s * 0.24f, cy - s * 0.34f, s * 0.48f, s * 0.36f}, ink, 0.0f,
                      kTau * 0.5f);
        list.triangle({cx - s * 0.24f, cy - s * 0.02f, s * 0.48f, s * 0.36f}, ink);
        break;
    }
    case Speed::correspondence:
    {
        // A letter.
        const Rect paper{cx - s * 0.42f, cy - s * 0.3f, s * 0.84f, s * 0.6f};
        list.bordered_rect(paper, s * 0.06f, Color{ink.r, ink.g, ink.b, 0.0f}, pen, ink);
        list.line(paper.x + pen, paper.y + pen, cx, cy + s * 0.04f, pen, ink);
        list.line(cx, cy + s * 0.04f, paper.x + paper.w - pen, paper.y + pen, pen, ink);
        break;
    }
    }
}

void panel(gfx::DrawList &list, const Rect &r, float lit, Color accent, float radius)
{
    radius = std::min(radius, std::min(r.w, r.h) * 0.5f);
    list.gradient_rect(r, radius, gfx::mix(kPanelTop, accent, 0.08f * lit).with_alpha(0.8f),
                       gfx::mix(kPanelBottom, accent, 0.03f * lit).with_alpha(0.88f));
    list.bordered_rect(r, radius, kClear, 1.5f,
                       gfx::mix(kInk.with_alpha(0.1f), accent.with_alpha(0.62f), lit));
}

void lift(gfx::DrawList &list, const Rect &r, float amount, Color accent, float time, bool calm,
          float radius)
{
    if (amount <= 0.01f)
        return;
    radius = std::min(radius, std::min(r.w, r.h) * 0.5f);
    list.shadow({r.x, r.y + 16.0f, r.w, r.h}, radius, 38.0f, kBlack.with_alpha(0.5f * amount));
    const float breath = calm ? 0.5f : ui::breathe(time);
    list.glow(r, radius, 26.0f, accent.with_alpha((0.15f + 0.1f * breath) * amount));
}

void ring(gfx::DrawList &list, const Rect &r, Color accent, float alpha, float radius)
{
    if (alpha <= 0.01f)
        return;
    list.bordered_rect(r.inset(-5.0f), std::min(radius, std::min(r.w, r.h) * 0.5f) + 5.0f, kClear,
                       3.0f, accent.with_alpha(alpha));
}

void rule(gfx::DrawList &list, float x, float y, float width, float alpha)
{
    list.rounded_rect({x, y, width, 1.5f}, 0.0f, kInk.with_alpha(alpha));
}

void halo(gfx::DrawList &list, const Rect &r, Color color, float alpha)
{
    const float core = std::min(r.w, r.h) * 0.3f;
    list.glow({r.cx() - core, r.cy() - core, core * 2.0f, core * 2.0f}, core,
              std::max(r.w, r.h) * 0.5f, color.with_alpha(alpha));
}

void frame_board(gfx::DrawList &list, const Rect &squares, Color accent, float glow)
{
    list.shadow({squares.x - 6.0f, squares.y + 20.0f, squares.w + 12.0f, squares.h}, 14.0f, 46.0f,
                kBlack.with_alpha(0.55f));
    if (glow > 0.01f)
        list.glow(squares.inset(-10.0f), 16.0f, 44.0f, accent.with_alpha(0.2f * glow));
}

float kicker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, Color color, gfx::Align align, float size, float max_width)
{
    const std::string caps = ui::upper(text);
    if (max_width > 0.0f)
        return ui::text_fit(list, fonts.semibold, caps, x, baseline, size, max_width, color, align,
                            3.0f);
    return ui::text(list, fonts.semibold, caps, x, baseline, size, color, align, 3.0f);
}

float figure(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, float size, Color color, gfx::Align align)
{
    return ui::text(list, fonts.semibold, text, x, baseline, size, color, align);
}

float ticker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
             float baseline, float size, Color color, gfx::Align align)
{
    return ui::text(list, fonts.mono, text, x, baseline, size, color, align);
}

float delta(gfx::DrawList &list, const ui::Fonts &fonts, int change, float x, float baseline,
            float size, gfx::Align align)
{
    if (change == 0)
        return 0.0f;
    char text[16];
    std::snprintf(text, sizeof(text), "%d", std::abs(change));
    const float mark = size * 0.62f;
    const float gap = size * 0.28f;
    const float width = mark + gap + fonts.semibold.measure(text, size);
    const float left = align == gfx::Align::right    ? x - width
                       : align == gfx::Align::center ? x - width * 0.5f
                                                     : x;
    const Color color = change > 0 ? kGood : kBad;
    list.triangle({left, baseline - size * 0.66f, mark, mark * 0.86f}, color, 0.0f,
                  change > 0 ? 0.0f : kTau * 0.5f);
    ui::text(list, fonts.semibold, text, left + mark + gap, baseline, size, color);
    return width;
}

float tag(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x, float cy,
          Color fill, Color ink, gfx::Align align, float size)
{
    const std::string caps = ui::upper(text);
    const float tracking = 1.5f;
    const float pad = size * 0.62f;
    const float width = fonts.semibold.measure(caps, size, tracking) + 2.0f * pad;
    const float height = size * 1.75f;
    const float left = align == gfx::Align::right    ? x - width
                       : align == gfx::Align::center ? x - width * 0.5f
                                                     : x;
    list.rounded_rect({left, cy - height * 0.5f, width, height}, height * 0.32f, fill);
    ui::text(list, fonts.semibold, caps, left + pad, cy + size * 0.36f, size, ink, gfx::Align::left,
             tracking);
    return width;
}

Fitted fit_lines(const ui::FontRef &font, std::string_view text, float size, float width)
{
    ui::FitStats &stats = ui::fit_stats();
    const auto fits = [&](std::string_view line, float at)
    { return font.measure(line, at) <= width + 0.5f; };
    // One line, as ui::text_fit sets it.
    const float scale = ui::fit_scale(font, text, size, width);
    if (fits(text, size * scale))
    {
        if (scale < 1.0f && stats.record)
            ++stats.shrunk;
        return {std::string(text), {}, size * scale};
    }
    // Two lines, in the largest size at which both fit.
    const float least = size * scale;
    std::vector<std::string> lines;
    float at = size;
    for (;; at = std::max(at - 1.0f, least))
    {
        lines = font.font->wrap(text, at, width);
        if (at <= least || (lines.size() == 2 && fits(lines[0], at) && fits(lines[1], at)))
            break;
    }
    Fitted out{lines[0], {}, at};
    for (std::size_t i = 1; i < lines.size(); ++i)
        out.second += (i > 1 ? " " : "") + lines[i];
    // What two lines of the smallest size cannot hold is cut.
    if (fits(out.first, at) && fits(out.second, at))
    {
        if (at < size && stats.record)
            ++stats.shrunk;
        return out;
    }
    out.first = font.font->fit(out.first, at, width);
    out.second = font.font->fit(out.second, at, width);
    if (stats.record)
    {
        ++stats.cut;
        stats.cut_texts.emplace_back(text);
    }
    return out;
}

float rise(float elapsed, int index, float step, float duration)
{
    return tween::stagger(elapsed, index, step, duration);
}

void sparkline(gfx::DrawList &list, const Rect &r, std::span<const float> values, Color color,
               float grow, bool area)
{
    const float bottom = r.y + r.h;
    const int count = static_cast<int>(values.size());
    if (count < 2)
    {
        list.rounded_rect({r.x, bottom - 2.0f, r.w, 2.0f}, 1.0f, kInk.with_alpha(0.16f));
        return;
    }
    float low = values[0];
    float high = values[0];
    for (const float v : values)
    {
        low = std::min(low, v);
        high = std::max(high, v);
    }
    const float range = std::max(high - low, 1.0f);
    grow = tween::clamp01(grow);
    const float step = r.w / static_cast<float>(count - 1);
    // A value's height: the line keeps clear of both edges.
    const auto height = [&](float v) { return ((v - low) / range * 0.8f + 0.12f) * r.h * grow; };
    if (area)
    {
        // The area is a row of narrow columns that fade toward the baseline.
        constexpr float kColumn = 4.0f;
        const int columns = static_cast<int>(std::ceil(r.w / kColumn));
        for (int column = 0; column < columns; ++column)
        {
            const float x = static_cast<float>(column) * kColumn;
            const float at = std::min(x / step, static_cast<float>(count - 1) - 0.001f);
            const int i = static_cast<int>(at);
            const float v =
                tween::lerp(values[static_cast<std::size_t>(i)],
                            values[static_cast<std::size_t>(i + 1)], at - static_cast<float>(i));
            const float h = height(v);
            list.gradient_rect({r.x + x, bottom - h, std::min(kColumn, r.w - x), h}, 0.0f,
                               color.with_alpha(0.26f), color.with_alpha(0.0f));
        }
    }
    float last_x = r.x;
    float last_y = bottom - height(values[0]);
    for (int i = 1; i < count; ++i)
    {
        const float x = r.x + static_cast<float>(i) * step;
        const float y = bottom - height(values[static_cast<std::size_t>(i)]);
        list.line(last_x, last_y, x, y, 2.5f, color);
        last_x = x;
        last_y = y;
    }
    list.glow({last_x - 4.0f, last_y - 4.0f, 8.0f, 8.0f}, 4.0f, 10.0f,
              color.with_alpha(0.55f * grow));
    list.circle(last_x, last_y, 4.5f, color);
}

void gauge(gfx::DrawList &list, float cx, float cy, float radius, float thickness, float share,
           Color color, float grow)
{
    list.arc(cx, cy, radius, thickness, 0.0f, kTau, kInk.with_alpha(0.09f));
    const float sweep = kTau * tween::clamp01(share) * tween::clamp01(grow);
    if (sweep > 0.01f)
        list.arc(cx, cy, radius, thickness, 0.0f, sweep, color);
}

void donut(gfx::DrawList &list, float cx, float cy, float radius, float thickness,
           std::span<const float> shares, std::span<const Color> colors, float grow)
{
    float total = 0.0f;
    for (const float share : shares)
        total += std::max(share, 0.0f);
    if (total <= 0.0f)
    {
        list.arc(cx, cy, radius, thickness, 0.0f, kTau, kInk.with_alpha(0.09f));
        return;
    }
    constexpr float kGap = 0.06f;
    grow = tween::clamp01(grow);
    float start = 0.0f;
    for (std::size_t i = 0; i < shares.size() && i < colors.size(); ++i)
    {
        const float sweep = kTau * std::max(shares[i], 0.0f) / total;
        if (sweep > kGap * 1.5f)
            list.arc(cx, cy, radius, thickness, start + kGap * 0.5f, (sweep - kGap) * grow,
                     colors[i], false);
        start += sweep;
    }
}

void bars(gfx::DrawList &list, const Rect &r, std::span<const float> values, Color color,
          float grow)
{
    const int count = static_cast<int>(values.size());
    const float bottom = r.y + r.h;
    list.rounded_rect({r.x, bottom - 1.5f, r.w, 1.5f}, 0.0f, kInk.with_alpha(0.14f));
    if (count == 0)
        return;
    float peak = 0.0f;
    for (const float v : values)
        peak = std::max(peak, v);
    if (peak <= 0.0f)
        return;
    const float gap = std::min(10.0f, r.w / static_cast<float>(count) * 0.3f);
    const float width = (r.w - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    grow = tween::clamp01(grow);
    for (int i = 0; i < count; ++i)
    {
        const float h =
            std::max(values[static_cast<std::size_t>(i)] / peak * (r.h - 4.0f) * grow, 2.0f);
        list.rounded_rect(
            {r.x + static_cast<float>(i) * (width + gap), bottom - 2.0f - h, width, h},
            std::min(width * 0.5f, 6.0f), color.with_alpha(i == count - 1 ? 1.0f : 0.45f));
    }
}

void level(gfx::DrawList &list, const Rect &r, float share, Color color, bool low)
{
    const float radius = r.h * 0.5f;
    list.rounded_rect(r, radius, kInk.with_alpha(0.1f));
    const float width = std::max(r.w * tween::clamp01(share), share > 0.0f ? r.h : 0.0f);
    if (width <= 0.0f)
        return;
    const Color fill = low ? kBad : color;
    const Rect bar{r.x, r.y, width, r.h};
    if (low)
        list.glow(bar, radius, 10.0f, fill.with_alpha(0.4f));
    list.rounded_rect(bar, radius, fill);
}

void live_dot(gfx::DrawList &list, float cx, float cy, float radius, Color color, float time,
              bool calm)
{
    if (!calm)
    {
        const float t = std::fmod(time, 1.6f) / 1.6f;
        list.ring(cx, cy, radius * (1.0f + 1.8f * t), 2.0f, color.with_alpha(0.6f * (1.0f - t)));
    }
    list.circle(cx, cy, radius, color);
}

} // namespace pch::app::look
