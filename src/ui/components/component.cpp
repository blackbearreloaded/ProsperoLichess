// ProsperoLichess - Components: shared helpers and the gliding focus highlight.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/components/component.hpp"

#include <algorithm>
#include <cmath>

namespace pch::ui
{

using gfx::Color;
using gfx::Rect;

namespace
{

// Acrylic with the house's deeper night, larger radii and softer hairlines,
// so the kit's controls sit in the panels the screens draw (app/look.hpp).
Theme house()
{
    Theme t = themes()[0];
    t.id = "house";
    t.name = "House";
    t.page = Color::rgb(0x0f142a);
    t.surface = Color::rgb(0x161c38, 0.74f);
    t.surface_high = Color::rgb(0xffffff, 0.1f);
    t.text = Color::rgb(0xeef2ff);
    t.text_muted = Color::rgb(0xeef2ff, 0.68f);
    t.primary = Color::rgb(0x7cd8ff);
    t.on_primary = Color::rgb(0x070b16);
    t.secondary = Color::rgb(0xffffff, 0.09f);
    t.accent = t.primary;
    t.outline = Color::rgb(0xffffff, 0.16f);
    t.shadow = Color::rgb(0x000000, 0.45f);
    t.light = Color::rgb(0xffffff, 0.14f);
    t.radius = 12.0f;
    t.radius_card = 22.0f;
    t.shadow_offset = 14.0f;
    t.shadow_blur = 34.0f;
    return t;
}

} // namespace

const Theme &default_theme()
{
    static const Theme theme = house();
    return theme;
}

void play_cue(Feedback &feedback, const ComponentStyle &style, audio::Cue cue, float x, float pitch,
              float gain)
{
    if (cue == audio::Cue::count)
        return;
    audio::CueEvent event;
    event.cue = cue;
    event.pitch = pitch;
    event.pan = style.sounds.pan && x >= 0.0f ? pan_for_x(x) : 0.0f;
    event.gain = gain * style.sounds.gain;
    event.set = style.sounds.set;
    feedback.cues.push_back(event);
}

Event refuse(Feedback &feedback, const ComponentStyle &style, const InputFrame &input, Pulse &pulse,
             float x)
{
    if (input.nav_repeat)
        return Event::refused;
    play_cue(feedback, style, style.sounds.refuse, x, 1.0f, 0.6f);
    if (style.sounds.rumble > 0.0f)
        feedback.rumble(0.25f * style.sounds.rumble, 0.05f);
    if (!style.reduced_motion)
        pulse.trigger();
    return Event::refused;
}

namespace
{

// Removes whole UTF-8 characters from the end until the text and "..." fit.
template <typename Measure> std::string fit(std::string_view text, float width, Measure measure)
{
    if (measure(text) <= width)
        return std::string(text);
    note_cut(text);
    std::string cut(text);
    while (!cut.empty())
    {
        // One whole character: its continuation bytes, then its first byte.
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xc0) == 0x80)
            cut.pop_back();
        if (!cut.empty())
            cut.pop_back();
        while (!cut.empty() && cut.back() == ' ')
            cut.pop_back();
        if (measure(cut + "...") <= width)
            break;
    }
    return cut + "...";
}

} // namespace

std::string fit_label(const Painter &paint, std::string_view text, float size, float width)
{
    return fit(text, width, [&](std::string_view s) { return paint.label_width(s, size); });
}

std::string fit_body(const Painter &paint, std::string_view text, float size, float width)
{
    return fit(text, width, [&](std::string_view s) { return paint.body_width(s, size); });
}

void Highlight::snap(const Rect &r)
{
    x_.snap(r.x);
    y_.snap(r.y);
    w_.snap(r.w);
    h_.snap(r.h);
}

void Highlight::target(const Rect &r)
{
    x_.target = r.x;
    y_.target = r.y;
    w_.target = r.w;
    h_.target = r.h;
}

void Highlight::update(float dt, const ComponentStyle &style)
{
    // A highlight must feel immediate whatever the theme's pace, and a hard
    // bounce on a focus ring reads as a glitch: both are clamped.
    const float omega = std::max(style.omega(), 18.0f);
    const float damping = std::max(style.damping(), 0.78f);
    x_.update(dt, omega, damping);
    y_.update(dt, omega, damping);
    w_.update(dt, omega, damping);
    h_.update(dt, omega, damping);
    refusal_.update(dt, 9.0f);
}

Rect Highlight::rect(float time) const
{
    return {x_.value + shake(refusal_.value, time, 10.0f), y_.value, w_.value, h_.value};
}

float Highlight::coverage(const Rect &item) const
{
    const float dx = std::fabs(x_.value - item.x) / std::max(item.w, 1.0f);
    const float dy = std::fabs(y_.value - item.y) / std::max(item.h, 1.0f);
    return tween::clamp01(1.0f - std::max(dx, dy));
}

Color Highlight::text_color(const ComponentStyle &style, const HighlightStyle &look, float focus)
{
    return text_color(style, look, focus, style.theme.text);
}

Color Highlight::text_color(const ComponentStyle &style, const HighlightStyle &look, float focus,
                            Color resting)
{
    if (look.kind != HighlightKind::fill)
        return resting;
    const Color plate = look.color.a > 0.0f ? look.color : style.theme.primary;
    const Color on = look.color.a > 0.0f ? Painter::on(plate) : style.theme.on_primary;
    return gfx::mix(resting, on, tween::clamp01(focus));
}

void Highlight::draw(Canvas &canvas, const ComponentStyle &style, const HighlightStyle &look,
                     float amount) const
{
    if (look.kind == HighlightKind::none || amount <= 0.001f)
        return;
    const Theme &theme = style.theme;
    const Rect r = rect(canvas.time).inset(-look.grow);
    Painter paint(canvas.list, canvas.fonts, theme, canvas.glass);
    const float radius = look.radius >= 0.0f ? std::min(look.radius, std::min(r.w, r.h) * 0.5f)
                                             : paint.control_radius(r);
    const float idle =
        look.breathe && !style.reduced_motion ? 0.82f + 0.18f * breathe(canvas.time) : 1.0f;
    const Color focus = look.color.a > 0.0f ? look.color : theme.focus;

    switch (look.kind)
    {
    case HighlightKind::ring:
        paint.focus_ring(r, radius, amount);
        break;
    case HighlightKind::fill:
    {
        const Color plate = look.color.a > 0.0f ? look.color : theme.primary;
        canvas.list.push_opacity(amount);
        paint.surface(r, radius, plate, theme.outline, 1.0f);
        canvas.list.pop_opacity();
        break;
    }
    case HighlightKind::tint:
        paint.fill(r, radius, focus.with_alpha(0.2f * amount * idle));
        paint.stroke(r, radius, std::max(theme.border, 1.5f), focus.with_alpha(0.75f * amount));
        break;
    case HighlightKind::bar:
        paint.fill(r, radius, focus.with_alpha(0.12f * amount));
        paint.fill({r.x, r.y + 8.0f, look.thickness, r.h - 16.0f},
                   theme.corner == Corner::round ? look.thickness * 0.5f : 0.0f,
                   focus.with_alpha(amount));
        break;
    case HighlightKind::underline:
        paint.fill({r.x, r.y + r.h - look.thickness, r.w, look.thickness},
                   theme.corner == Corner::round ? look.thickness * 0.5f : 0.0f,
                   focus.with_alpha(amount));
        break;
    case HighlightKind::glow:
        canvas.list.glow(r, radius, 26.0f, focus.with_alpha(0.5f * amount * idle));
        paint.fill(r, radius, gfx::mix(theme.surface, focus, 0.14f).with_alpha(amount));
        paint.stroke(r, radius, 2.0f, focus.with_alpha(amount));
        break;
    case HighlightKind::none:
        break;
    }
}

} // namespace pch::ui
