// ProsperoLichess - The furniture every screen shares: layout grid, status bar, hints, icons.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/chrome.hpp"

#include "core/strings.hpp"
#include "lichess/session.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pch::app
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr float kBarText = 24.0f;
constexpr float kBarPadding = 22.0f;
constexpr float kBellKey = 28.0f; // the Touchpad's glyph beside the bell
// Notices start this far into the strip: after a place's name, or after the
// app's own where the strip spans the screen.
constexpr float kNoticeAt = 250.0f;
constexpr float kNoticeAtWide = 380.0f;
constexpr float kNoticeWidth = 640.0f; // a notice whose line fits
constexpr float kNoticeStep = 40.0f;   // ... and what a longer line adds to it, as often as needed
constexpr float kNoticeSide = 30.0f;   // from the stack's bounds to a notice

} // namespace

void kicker(gfx::DrawList &list, const ui::Fonts &fonts, std::string_view text, float x,
            float baseline, Color color)
{
    ui::text(list, fonts.semibold, ui::upper(text), x, baseline, 17.0f, color, Align::left, 3.0f);
}

void draw_hints(const Context &ctx, gfx::DrawList &list, const ui::Hint *hints, int count,
                bool over_scrim, float max_width)
{
    const ui::Theme &theme = ctx.theme();
    ui::GlyphStyle glyphs =
        theme.dark || over_scrim ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    if (!over_scrim)
        glyphs.label = theme.page_text.a > 0.0f ? theme.page_text : theme.text.with_alpha(0.86f);
    ui::HintLayout layout;
    const float width = ui::measure_hints(*ctx.fonts, hints, count, layout);
    if (max_width > 0.0f && width > max_width)
    {
        // Glyphs, labels and gaps shrink together, to two thirds at most.
        const float scale = std::max(max_width / width, 0.66f);
        layout.size *= scale;
        layout.text_size *= scale;
        layout.item_gap *= scale;
    }
    ui::draw_hints(list, *ctx.fonts, glyphs, hints, count, kRight, true, layout);
}

void draw_verdict(gfx::DrawList &list, float cx, float cy, float radius, bool good, Color fill,
                  Color ink)
{
    list.circle(cx, cy, radius, fill);
    const float pen = std::max(radius * 0.2f, 2.2f);
    const float u = radius * 0.42f;
    if (good)
    {
        list.line(cx - u, cy + u * 0.1f, cx - u * 0.25f, cy + u * 0.8f, pen, ink);
        list.line(cx - u * 0.25f, cy + u * 0.8f, cx + u, cy - u * 0.7f, pen, ink);
    }
    else
    {
        list.line(cx - u * 0.8f, cy - u * 0.8f, cx + u * 0.8f, cy + u * 0.8f, pen, ink);
        list.line(cx - u * 0.8f, cy + u * 0.8f, cx + u * 0.8f, cy - u * 0.8f, pen, ink);
    }
}

void draw_nav_icon(gfx::DrawList &list, NavIcon icon, const Rect &box, Color ink)
{
    const float x = box.x;
    const float y = box.y;
    const float s = box.w;
    const float cx = box.cx();
    const float cy = box.cy();
    const float pen = std::max(s * 0.09f, 2.4f);
    const Color clear{ink.r, ink.g, ink.b, 0.0f};
    switch (icon)
    {
    case NavIcon::home:
        list.triangle({x + s * 0.04f, y + s * 0.06f, s * 0.92f, s * 0.44f}, ink);
        list.rounded_rect({x + s * 0.2f, y + s * 0.46f, s * 0.6f, s * 0.46f}, 3.0f, ink);
        break;
    case NavIcon::puzzles:
        list.rounded_rect({x + s * 0.08f, y + s * 0.3f, s * 0.62f, s * 0.62f}, 4.0f, ink);
        list.circle(x + s * 0.39f, y + s * 0.24f, s * 0.15f, ink);
        list.circle(x + s * 0.76f, y + s * 0.61f, s * 0.15f, ink);
        break;
    case NavIcon::play:
        list.ring(cx, cy, s * 0.46f, pen, ink);
        list.triangle({cx - s * 0.16f, cy - s * 0.18f, s * 0.4f, s * 0.36f}, ink, 0.0f, 1.5708f);
        break;
    case NavIcon::watch:
        list.bordered_rect({x + s * 0.04f, y + s * 0.14f, s * 0.92f, s * 0.6f}, 4.0f, clear, pen,
                           ink);
        list.rounded_rect({cx - s * 0.24f, y + s * 0.84f, s * 0.48f, pen}, pen * 0.5f, ink);
        list.circle(cx, cy - s * 0.06f, s * 0.1f, ink);
        break;
    case NavIcon::profile:
        list.circle(cx, y + s * 0.3f, s * 0.2f, ink);
        list.rounded_rect({x + s * 0.14f, y + s * 0.58f, s * 0.72f, s * 0.34f}, s * 0.17f, ink);
        break;
    case NavIcon::settings:
        for (int i = 0; i < 3; ++i)
        {
            const float ly = y + s * (0.22f + 0.28f * static_cast<float>(i));
            list.rounded_rect({x + s * 0.06f, ly - pen * 0.5f, s * 0.88f, pen}, pen * 0.5f, ink);
            list.circle(x + s * (i == 1 ? 0.34f : 0.66f), ly, s * 0.13f, ink);
        }
        break;
    }
}

std::string format_clock(long long milliseconds)
{
    if (milliseconds < 0)
        milliseconds = 0;
    if (milliseconds >= 2147483000LL)
        return "--";
    char text[32];
    const long long total_seconds = milliseconds / 1000;
    if (milliseconds < 10000)
    {
        std::snprintf(text, sizeof(text), "0:%02lld.%lld", total_seconds,
                      (milliseconds / 100) % 10);
    }
    else if (total_seconds >= 3600)
    {
        std::snprintf(text, sizeof(text), "%lld:%02lld:%02lld", total_seconds / 3600,
                      (total_seconds / 60) % 60, total_seconds % 60);
    }
    else
    {
        std::snprintf(text, sizeof(text), "%lld:%02lld", total_seconds / 60, total_seconds % 60);
    }
    return text;
}

// ---- the status bar --------------------------------------------------------

StatusChrome::StatusChrome()
{
    state_.style.leading_dot = true;
    state_.style.height = 36.0f;

    bell_.style.role = ui::ButtonRole::ghost;
    bell_.style.on_page = false;
    bell_.style.ring_on_increase = true;

    spinner_.style.status = ui::Status::warning;

    // Notices are one line each and sit inside the bar, between the wordmark
    // and the account, so they never cover the screen under it.
    notices_.style.look = ui::NotificationLook::compact;
    notices_.style.anchor = ui::ToastAnchor::top_center;
    notices_.style.width = kNoticeWidth;
    notices_.style.margin = 0.0f;
    notices_.style.max_visible = 1;
    notices_.style.duration = 3.5f;
    notices_.style.time_bar = false;
    notices_.style.more_marker = false;
    notices_.style.frost = 0.72f;
    notices_.set_bounds(
        {kBar.x + kNoticeAtWide, kBar.y + 3.0f, kNoticeWidth + 2.0f * kNoticeSide, 200.0f});

    // Announcements float over the page, flush with the bar's right end.
    floating_.style.anchor = ui::ToastAnchor::top_right;
    floating_.style.width = 520.0f;
    floating_.style.margin = 0.0f;
    floating_.style.max_visible = 1;
    floating_.style.frosted = true;
    floating_.set_bounds({kMargin, kTop, kRight - kMargin, kBottom - kTop});
}

void StatusChrome::announce(const std::string &title, const std::string &body, float seconds)
{
    floating_.push(ui::StatusKind::info, title, body, seconds);
}

void StatusChrome::notify(const std::string &text, Note kind)
{
    ui::Notification note;
    note.kind = kind == Note::success   ? ui::StatusKind::success
                : kind == Note::warning ? ui::StatusKind::warning
                                        : ui::StatusKind::info;
    note.title = text;
    note.closable = false;
    notice_ids_.push_back(notices_.push(std::move(note)));
}

// A notice is one line, and a translation is often longer than the English:
// the longest one showing or waiting widens the stack toward the strip's
// right end, over the account, rather than lose its end.
void StatusChrome::fit_notices(const Context &ctx)
{
    const ui::Painter paint(scratch_, *ctx.fonts, notices_.style.theme, 0);
    // What a notice holds beside its line: the padding and the icon.
    const float around = 2.0f * notices_.style.padding + notices_.style.header_icon + 12.0f;
    float needed = 0.0f;
    std::erase_if(notice_ids_,
                  [&](int id)
                  {
                      const ui::Notification *note = notices_.find(id);
                      if (note == nullptr)
                          return true;
                      needed =
                          std::max(needed, around + paint.label_width(note->title,
                                                                      notices_.style.title_size));
                      return false;
                  });
    // It widens in steps, so a count in the line does not make it twitch.
    const float steps = std::max(std::ceil((needed - kNoticeWidth) / kNoticeStep), 0.0f);
    const float left = strip_.x + (place_.empty() ? kNoticeAtWide : kNoticeAt);
    const float width =
        std::min(kNoticeWidth + steps * kNoticeStep, strip_.x + strip_.w - left - kNoticeSide);
    notices_.style.width = width;
    notices_.set_bounds({left, kBar.y + 3.0f, width + 2.0f * kNoticeSide, 200.0f});
}

void StatusChrome::set_place(const Place &place)
{
    // Pages name themselves in English (TR): the name is translated here.
    const std::string title = place.title != nullptr ? tr(place.title) : "";
    // A new place's name slides in instead of swapping.
    if (title != place_)
        place_in_.snap(0.0f);
    place_ = title;
    place_icon_ = place.icon;
    answers_bell_ = place.answers_bell;
    // Notices sit inside the strip, after what it says at its left end
    // (fit_notices places them).
    strip_ = place_.empty() ? kBar : kStrip;
}

void StatusChrome::update(Context &ctx, float dt)
{
    ctx.calm(state_, bell_, avatar_, spinner_, notices_, floating_);
    lichess::Session *session = ctx.lichess;
    online_ = session != nullptr && session->online();
    signed_in_ = session != nullptr && session->signed_in();

    // The connection notice: one line that mirrors what the session has to
    // say (a spinner while something is being retried), and leaves with it.
    lichess::Notice notice;
    if (session != nullptr)
        notice = session->notice();
    const bool lasting = notice.kind == lichess::Notice::Kind::warning && notice.busy;
    trouble_ = notice.kind == lichess::Notice::Kind::warning;
    if (notice.kind != lichess::Notice::Kind::none)
    {
        const bool known = issue_id_ != 0 && notices_.find(issue_id_) != nullptr;
        if (!known || notice.busy != issue_busy_ || trouble_ != issue_warning_)
        {
            ui::Notification note;
            note.kind = trouble_ ? ui::StatusKind::warning : ui::StatusKind::success;
            note.title = notice.text;
            note.closable = false;
            note.seconds = -1.0f;
            if (notice.busy)
            {
                note.icon = [this](ui::Canvas &canvas, const Rect &box)
                {
                    ui::Spinner busy = spinner_;
                    busy.set_bounds(box);
                    busy.draw(canvas);
                };
            }
            if (known)
                notices_.update_notification(issue_id_, std::move(note), false);
            else
                notice_ids_.push_back(issue_id_ = notices_.push(std::move(note)));
            issue_busy_ = notice.busy;
            issue_warning_ = trouble_;
        }
        else if (notice.text != issue_text_)
        {
            notices_.set_title(issue_id_, notice.text);
        }
        issue_text_ = notice.text;
    }
    else if (issue_id_ != 0)
    {
        notices_.dismiss(issue_id_);
        issue_id_ = 0;
    }

    if (session != nullptr && signed_in_)
    {
        if (name_ != session->username())
        {
            name_ = session->username();
            avatar_.set_name(name_);
        }
        const int best = std::max(session->rating("rapid"), session->rating("blitz"));
        rating_ = best > 0 ? std::to_string(best) : std::string();
        int waiting = 0;
        for (const lichess::OngoingGame &game : session->ongoing())
            waiting += game.my_turn ? 1 : 0;
        bell_.set_count(waiting);
    }
    else
    {
        name_.clear();
        rating_.clear();
        bell_.set_count(0);
    }

    if (trouble_)
    {
        state_.label = lasting ? tr("Reconnecting") : tr("Connection trouble");
        state_.style.dot = ui::Status::warning;
    }
    else if (session == nullptr || !online_)
    {
        state_.label = tr("Offline");
        state_.style.dot = ui::Status::danger;
    }
    else if (!signed_in_)
    {
        state_.label = session->signing_in() ? tr("Signing in") : tr("Not signed in");
        state_.style.dot = ui::Status::warning;
    }
    else
    {
        state_.label = tr("Online");
        state_.style.dot = ui::Status::success;
    }

    // Lay the trailing end out here, not while drawing: the bell keeps its
    // badge where its bounds were at the last update.
    {
        scratch_.clear();
        const ui::Painter paint(scratch_, *ctx.fonts, ctx.theme(), 0);
        const Rect b = strip_;
        const float cy = b.cy();
        float right = b.x + b.w - kBarPadding;
        if (signed_in_)
        {
            rating_right_ = right;
            if (!rating_.empty())
                right -= paint.body_width(rating_, 22.0f) + 12.0f;
            name_right_ = right;
            right -= paint.label_width(name_, kBarText) + 14.0f;
            avatar_.set_bounds({right - 40.0f, cy - 20.0f, 40.0f, 40.0f});
            right -= 40.0f + 20.0f;
            if (bell_.count() > 0)
            {
                bell_.set_bounds({right - 46.0f, cy - 23.0f, 46.0f, 46.0f});
                right -= 46.0f;
                if (answers_bell_)
                {
                    touch_x_ = right - 2.0f - ui::button_width(ui::Button::touchpad, kBellKey);
                    right = touch_x_;
                }
                right -= 16.0f;
            }
        }
        const float width = state_.width(paint);
        state_.set_bounds({right - width, cy - 18.0f, width, 36.0f});
    }

    fit_notices(ctx);

    place_in_.target = 1.0f;
    place_in_.update(dt, ctx.reduced_motion() ? 60.0f : 13.0f);
    state_.update(dt);
    bell_.update(dt);
    avatar_.update(dt);
    spinner_.update(dt);
    notices_.update(dt, *ctx.feedback);
    floating_.update(dt, *ctx.feedback);
}

void StatusChrome::draw_bar(const Context &ctx, gfx::DrawList &list) const
{
    const ui::Theme &theme = ctx.theme();
    ui::Canvas canvas = canvas_for(ctx, list);
    ui::Painter paint(list, *ctx.fonts, theme, 0);
    const Rect b = strip_;
    const float cy = b.cy();
    look::panel(list, b, 0.0f, look::kInk, 20.0f);
    if (place_.empty())
    {
        // No menu beside it: the strip carries the app's mark and name.
        ctx.pieces->draw(list, {chess::Color::white, chess::Role::knight},
                         {b.x + 16.0f, cy - 23.0f, 46.0f, 46.0f});
        paint.label("ProsperoLichess", b.x + 76.0f, cy + kBarText * 0.35f, kBarText, theme.text);
    }
    else
    {
        // The place the rail's page shows, with its sign in its colour.
        const float in = tween::clamp01(place_in_.value);
        list.push_opacity(in);
        list.push_transform(1.0f, 0.0f, 0.0f, ctx.reduced_motion() ? 0.0f : -12.0f * (1.0f - in),
                            0.0f);
        float x = b.x + kBarPadding;
        if (place_icon_ >= 0)
        {
            draw_nav_icon(list, static_cast<NavIcon>(place_icon_), {x, cy - 12.0f, 24.0f, 24.0f},
                          ctx.accent);
            x += 24.0f + 14.0f;
        }
        // The name ends before the place where notices appear.
        ui::text_fit(list, ctx.fonts->display, place_, x, cy + 26.0f * 0.35f, 26.0f,
                     b.x + kNoticeAt + kNoticeSide - 16.0f - x, look::kInk);
        list.pop_transform();
        list.pop_opacity();
    }

    if (signed_in_)
    {
        if (!rating_.empty())
            paint.body(rating_, rating_right_, cy + 22.0f * 0.35f, 22.0f, theme.text_muted,
                       Align::right);
        paint.label(name_, name_right_, cy + kBarText * 0.35f, kBarText, theme.text, Align::right);
        avatar_.draw(canvas);
        if (bell_.count() > 0)
        {
            bell_.draw(canvas);
            // The button that opens what the bell counts.
            if (answers_bell_)
                ui::draw_button(list, *ctx.fonts,
                                theme.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light(),
                                ui::Button::touchpad, touch_x_, cy, kBellKey);
        }
    }
    state_.draw(canvas);
}

void StatusChrome::draw_notices(const Context &ctx, gfx::DrawList &list, std::uint32_t glass) const
{
    ui::Canvas canvas = canvas_for(ctx, list, glass);
    notices_.draw(canvas);
    floating_.draw(canvas);
}

} // namespace pch::app
