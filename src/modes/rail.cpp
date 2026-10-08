// ProsperoLichess - The rail: the app's mark and the menu down the left of the home screen.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/rail.hpp"

#include <algorithm>

namespace pch::modes
{

namespace
{

namespace look = app::look;
using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr float kPanelRadius = 28.0f;
constexpr float kInset = 14.0f;        // from the panel's sides to a row
constexpr float kBrandHeight = 104.0f; // the mark and the wordmark
constexpr float kRowsTop = 22.0f;      // from the brand's rule to the first row
constexpr float kRowHeight = 62.0f;
constexpr float kRowGap = 8.0f;
constexpr float kRowRadius = 18.0f;
constexpr float kTile = 44.0f; // the square behind a sign
constexpr float kTileRadius = 13.0f;
constexpr float kLabel = 24.0f;
constexpr float kLabelX = 9.0f + kTile + 16.0f; // from a row's left edge to its label

} // namespace

void Rail::set_entries(std::vector<RailEntry> entries)
{
    entries_ = std::move(entries);
    lit_.assign(entries_.size(), {});
    lean_.assign(entries_.size(), {});
    placed_ = false;
}

void Rail::set_note(RailNote note)
{
    note_in_.target = note.title.empty() ? 0.0f : 1.0f;
    // The words stay while the card fades out.
    if (!note.title.empty())
        note_ = std::move(note);
}

void Rail::set_bounds(const Rect &bounds)
{
    bounds_ = bounds;
    placed_ = false;
}

void Rail::set_current(int index, bool snap)
{
    current_ = std::clamp(index, 0, std::max(0, static_cast<int>(entries_.size()) - 1));
    if (snap)
        placed_ = false;
}

void Rail::set_focus(int index)
{
    focus_ = std::clamp(index, 0, std::max(0, static_cast<int>(entries_.size()) - 1));
}

// The last entry is pinned to the bottom of the rail; the others run down
// from under the brand.
Rect Rail::row(int index) const
{
    const float x = bounds_.x + kInset;
    const float w = bounds_.w - 2.0f * kInset;
    if (index == static_cast<int>(entries_.size()) - 1)
        return {x, bounds_.y + bounds_.h - kInset - kRowHeight, w, kRowHeight};
    return {
        x, bounds_.y + kBrandHeight + kRowsTop + static_cast<float>(index) * (kRowHeight + kRowGap),
        w, kRowHeight};
}

ui::Event Rail::handle(const InputFrame &input, app::Context &ctx)
{
    if (entries_.empty())
        return ui::Event::none;
    const int count = static_cast<int>(entries_.size());
    const float pan = ui::pan_for_x(bounds_.cx());
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        const int next = focus_ + (input.nav == Direction::down ? 1 : -1);
        if (next < 0 || next >= count)
        {
            // The end of the menu answers, unless the direction is only held.
            if (!input.nav_repeat)
            {
                refusal_.trigger();
                ctx.cue(audio::Cue::error, 1.0f, pan, 0.5f);
            }
            return ui::Event::refused;
        }
        focus_ = next;
        const float along =
            count > 1 ? static_cast<float>(focus_) / static_cast<float>(count - 1) : 0.0f;
        ctx.cue(audio::Cue::focus, tween::lerp(1.05f, 0.95f, along), pan);
        return ui::Event::moved;
    }
    if (input.is_pressed(Action::confirm))
    {
        press_.trigger();
        ctx.cue(audio::Cue::select, 1.0f, pan);
        return ui::Event::activated;
    }
    if (input.is_pressed(Action::back))
    {
        ctx.cue(audio::Cue::back, 1.0f, pan);
        return ui::Event::cancelled;
    }
    return ui::Event::none;
}

void Rail::update(app::Context &ctx, float dt)
{
    calm_ = ctx.reduced_motion();
    age_ += dt;
    if (entries_.empty())
        return;
    const Color ink = look::accent(entries_[static_cast<std::size_t>(current_)].section);
    plate_.target(row(current_));
    plate_ink_.target(ink);
    ring_.target(row(focus_));
    ring_alpha_.target = focused_ ? 1.0f : 0.0f;
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        lit_[i].target = static_cast<int>(i) == current_ ? 1.0f : 0.0f;
        lean_[i].target = focused_ && static_cast<int>(i) == focus_ ? 1.0f : 0.0f;
    }
    if (!placed_)
    {
        placed_ = true;
        plate_.snap(row(current_));
        plate_ink_.snap(ink);
        ring_.snap(row(focus_));
        ring_alpha_.snap(ring_alpha_.target);
        for (std::size_t i = 0; i < entries_.size(); ++i)
        {
            lit_[i].snap(lit_[i].target);
            lean_[i].snap(lean_[i].target);
        }
    }
    const float quick = calm_ ? 60.0f : 18.0f;
    plate_.update(dt, calm_ ? 60.0f : 16.0f);
    plate_ink_.update(dt, calm_ ? 60.0f : 10.0f);
    ring_.update(dt, quick);
    ring_alpha_.update(dt, quick);
    for (std::size_t i = 0; i < entries_.size(); ++i)
    {
        lit_[i].update(dt, quick);
        lean_[i].update(dt, quick);
    }
    note_in_.update(dt, calm_ ? 60.0f : 10.0f);
    refusal_.update(dt);
    press_.update(dt, 10.0f);
}

void Rail::draw(const app::Context &ctx, gfx::DrawList &list) const
{
    const ui::Fonts &fonts = *ctx.fonts;
    const Rect b = bounds_;
    look::panel(list, b, 0.0f, look::kInk, kPanelRadius);
    if (entries_.empty())
        return;

    // ---- the app's mark, lit in the colour of the page that is showing
    const Color accent = plate_ink_.value();
    const Rect mark{b.x + 20.0f, b.y + 24.0f, 56.0f, 56.0f};
    list.push_clip(b.inset(2.0f));
    look::halo(list, {mark.x - 50.0f, mark.y - 50.0f, mark.w + 100.0f, mark.h + 100.0f}, accent,
               0.2f);
    list.pop_clip();
    list.glow(mark, 16.0f, 14.0f, accent.with_alpha(0.3f));
    list.gradient_rect(mark, 16.0f, gfx::mix(accent, look::kInk, 0.2f),
                       gfx::mix(accent, look::kNight, 0.5f));
    ctx.pieces->draw(list, {chess::Color::white, chess::Role::knight}, mark.inset(4.0f));
    const float wx = mark.x + mark.w + 14.0f;
    look::kicker(list, fonts, "Prospero", wx, mark.y + 20.0f, look::kInk.with_alpha(look::kMuted),
                 Align::left, 13.0f);
    ui::text(list, fonts.display, "Lichess", wx - 1.0f, mark.y + 50.0f, 30.0f, look::kInk);
    look::rule(list, b.x + 20.0f, b.y + kBrandHeight, b.w - 40.0f);

    // ---- the lit plate under the entry that is showing
    const Rect plate = plate_.value();
    const Color ink = plate_ink_.value();
    list.glow(plate, kRowRadius, 16.0f, ink.with_alpha(0.1f));
    list.gradient_rect_h(plate, kRowRadius, ink.with_alpha(0.3f), ink.with_alpha(0.06f));
    list.bordered_rect(plate, kRowRadius, look::kClear, 1.5f, ink.with_alpha(0.5f));
    list.rounded_rect({b.x + 4.0f, plate.cy() - 14.0f, 4.0f, 28.0f}, 2.0f, ink);

    // ---- the entries
    const int count = static_cast<int>(entries_.size());
    look::rule(list, b.x + 20.0f, row(count - 1).y - 11.0f, b.w - 40.0f);
    // The rail's width is fixed. Every label has the size the longest of them
    // needs to end before the row does (or before the row's pulsing mark).
    const auto label_room = [&](int i)
    { return row(i).w - kLabelX - (entries_[static_cast<std::size_t>(i)].live ? 42.0f : 12.0f); };
    float label_size = kLabel;
    for (int i = 0; i < count; ++i)
        label_size =
            std::min(label_size, kLabel * ui::fit_scale(fonts.semibold,
                                                        entries_[static_cast<std::size_t>(i)].label,
                                                        kLabel, label_room(i)));
    for (int i = 0; i < count; ++i)
    {
        const RailEntry &entry = entries_[static_cast<std::size_t>(i)];
        const Rect r = row(i);
        const float lit = lit_[static_cast<std::size_t>(i)].value;
        const float lean = lean_[static_cast<std::size_t>(i)].value;
        const float in = calm_ ? 1.0f : look::rise(age_, i, 0.045f, 0.4f);
        const float nudge = i == focus_ ? ui::shake(refusal_.value, age_, 8.0f) : 0.0f;
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, (calm_ ? 0.0f : 5.0f * lean) - 14.0f * (1.0f - in),
                            nudge);
        const Color c = look::accent(entry.section);
        const Rect tile{r.x + 9.0f, r.cy() - kTile * 0.5f, kTile, kTile};
        if (lit > 0.01f)
            list.glow(tile, kTileRadius, 10.0f, c.with_alpha(0.35f * lit));
        list.rounded_rect(tile, kTileRadius, gfx::mix(c.with_alpha(0.15f), c, lit));
        app::draw_nav_icon(list, entry.icon, {tile.cx() - 12.0f, tile.cy() - 12.0f, 24.0f, 24.0f},
                           gfx::mix(c, look::kNight, lit));
        ui::text_fit(list, fonts.semibold, entry.label, tile.x + tile.w + 16.0f,
                     r.cy() + label_size * 0.35f, label_size, label_room(i),
                     look::kInk.with_alpha(tween::lerp(look::kMuted, 1.0f, std::max(lit, lean))));
        if (entry.live)
        {
            const Color rose = look::accent(look::Section::watch);
            look::live_dot(list, r.x + r.w - 20.0f, r.cy(), 5.0f, rose, ctx.time, calm_);
        }
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- the note, in the free space above the last entry
    if (note_in_.value > 0.01f && !note_.title.empty())
    {
        const float in = note_in_.value;
        const Color c = look::accent(note_.section);
        const float room = b.w - 2.0f * kInset - 36.0f;
        // A body too long for one line takes two, and the card grows upward.
        const look::Fitted body = look::fit_lines(fonts.regular, note_.body, 18.0f, room);
        const float second = body.second.empty() ? 0.0f : body.size + 4.0f;
        const float height = (note_.body.empty() ? 92.0f : 118.0f) + second;
        const Rect card{b.x + kInset, row(count - 1).y - 24.0f - height, b.w - 2.0f * kInset,
                        height};
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, 0.0f, calm_ ? 0.0f : look::settle(in, 12.0f));
        list.gradient_rect(card, kRowRadius, c.with_alpha(0.16f), c.with_alpha(0.05f));
        list.bordered_rect(card, kRowRadius, look::kClear, 1.5f, c.with_alpha(0.32f));
        look::live_dot(list, card.x + 22.0f, card.y + 27.0f, 4.5f, c, ctx.time, calm_);
        look::kicker(list, fonts, note_.kicker, card.x + 36.0f, card.y + 32.0f, c, Align::left,
                     13.0f, card.w - 36.0f - 14.0f);
        ui::text_fit(list, fonts.semibold, note_.title, card.x + 18.0f, card.y + 66.0f, 22.0f, room,
                     look::kInk);
        if (!note_.body.empty())
            ui::text(list, fonts.regular, body.first, card.x + 18.0f, card.y + 94.0f, body.size,
                     look::kInk.with_alpha(look::kMuted));
        if (!body.second.empty())
            ui::text(list, fonts.regular, body.second, card.x + 18.0f, card.y + 94.0f + second,
                     body.size, look::kInk.with_alpha(look::kMuted));
        list.pop_transform();
        list.pop_opacity();
    }

    // ---- the ring, while the rail has the controller
    if (ring_alpha_.value > 0.01f)
    {
        Rect ring = ring_.value();
        ring.y += ui::shake(refusal_.value, age_, 8.0f);
        const float squeeze = 2.0f * press_.value;
        look::ring(list, ring.inset(squeeze), look::kInk, ring_alpha_.value, kRowRadius);
    }
}

} // namespace pch::modes
