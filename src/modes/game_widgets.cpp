// ProsperoLichess - What board screens share: player panels with clocks and the move table.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modes/game_widgets.hpp"

#include "core/strings.hpp"
#include "ui/components/data_common.hpp"

#include <algorithm>
#include <cstdio>

namespace pch::modes
{

namespace
{

using gfx::Align;
using gfx::Color;
using gfx::Rect;

constexpr float kPad = 24.0f;
constexpr float kClockWidth = 196.0f;
constexpr float kClockHeight = 76.0f;
constexpr float kClockSize = 50.0f;

} // namespace

PlayerPanel::PlayerPanel() = default;

void PlayerPanel::set_bounds(const Rect &bounds)
{
    bounds_ = bounds;
    const float size = bounds.h - 2.0f * kPad;
    avatar_.set_bounds({bounds.x + kPad, bounds.y + kPad, size, size});
}

void PlayerPanel::set_player(const std::string &name, const std::string &rating,
                             const std::string &title, chess::Color color)
{
    if (name != name_)
    {
        name_ = name;
        avatar_.set_name(name);
    }
    rating_ = rating;
    title_ = title;
    color_ = color;
}

void PlayerPanel::set_note(const std::string &note)
{
    note_ = note;
}

void PlayerPanel::set_captured(std::vector<chess::Piece> pieces)
{
    captured_ = std::move(pieces);
}

void PlayerPanel::set_clock(long long milliseconds, bool has_clock, bool active, long long low_ms)
{
    clock_ms_ = milliseconds;
    has_clock_ = has_clock && milliseconds >= 0;
    // The most the clock has shown is the level's full mark (an increment
    // can raise it).
    if (!has_clock_)
        clock_peak_ms_ = 0;
    else if (milliseconds > clock_peak_ms_)
        clock_peak_ms_ = milliseconds;
    active_ = active;
    low_ = has_clock_ && active && milliseconds < low_ms;
}

void PlayerPanel::set_presence(ui::Presence presence)
{
    avatar_.set_presence(presence);
}

void PlayerPanel::set_accent(Color accent)
{
    accent_ = accent;
}

Rect PlayerPanel::clock_box() const
{
    return {bounds_.x + bounds_.w - kPad - kClockWidth, bounds_.cy() - kClockHeight * 0.5f,
            kClockWidth, kClockHeight};
}

void PlayerPanel::update(app::Context &ctx, float dt)
{
    ctx.calm(avatar_);
    avatar_.update(dt);
    active_amount_.target = active_ ? 1.0f : 0.0f;
    active_amount_.update(dt, ctx.reduced_motion() ? 60.0f : 14.0f);
}

void PlayerPanel::draw(const app::Context &ctx, gfx::DrawList &list) const
{
    namespace look = app::look;
    const ui::Theme &theme = ctx.theme();
    const ui::Fonts &fonts = *ctx.fonts;
    ui::Canvas canvas = app::canvas_for(ctx, list);
    ui::Painter paint(list, fonts, theme, 0);
    const Rect b = bounds_;
    // The side to move is lit: its panel floats and takes the accent.
    const float a = active_amount_.value;
    const Color tint = low_ ? look::kBad : accent_;
    look::lift(list, b, a * 0.6f, tint, ctx.time, ctx.reduced_motion());
    look::panel(list, b, a, tint);
    avatar_.draw(canvas);

    const float size = b.h - 2.0f * kPad;
    const float left = b.x + kPad + size + 20.0f;
    const float text_right = (has_clock_ ? clock_box().x : b.x + b.w) - 20.0f;
    float x = left;
    const float first = b.cy() - 6.0f;
    if (!title_.empty())
        x += look::tag(list, fonts, title_, x, first - 10.0f, look::kGold, look::kNight,
                       Align::left, 16.0f) +
             12.0f;
    const float rating_width =
        rating_.empty() ? 0.0f : fonts.semibold.measure(rating_, 24.0f) + 14.0f;
    x += paint.label(ui::fit_label(paint, name_, 30.0f, text_right - x - rating_width), x, first,
                     30.0f, theme.text) +
         14.0f;
    if (!rating_.empty())
        look::figure(list, fonts, rating_, x, first, 24.0f, look::kInk.with_alpha(look::kMuted));

    // Whose pieces these are, then what this player took.
    float tx = left;
    const float second = b.cy() + 32.0f;
    ctx.pieces->draw(list, {color_, chess::Role::pawn}, {tx - 6.0f, second - 26.0f, 32.0f, 32.0f});
    tx += 28.0f;
    tx += ui::text_fit(list, fonts.regular, note_, tx, second, 21.0f, text_right - tx,
                       gfx::mix(theme.text_muted, tint, 0.55f * a)) +
          14.0f;
    for (const chess::Piece &piece : captured_)
    {
        if (tx + 26.0f > text_right)
            break;
        ctx.pieces->draw(list, piece, {tx - 4.0f, second - 24.0f, 30.0f, 30.0f});
        tx += 19.0f;
    }

    if (!has_clock_)
        return;
    // The clock: large monospaced digits over a level that drains.
    const Rect box = clock_box();
    const std::string text = app::format_clock(clock_ms_);
    const Color ink =
        gfx::mix(look::kInk.with_alpha(look::kFaint + 0.14f), low_ ? look::kBad : look::kInk, a);
    look::ticker(list, fonts, text, box.x + box.w, box.cy() + kClockSize * 0.22f, kClockSize, ink,
                 Align::right);
    const float share = clock_peak_ms_ > 0
                            ? static_cast<float>(clock_ms_) / static_cast<float>(clock_peak_ms_)
                            : 0.0f;
    const Rect bar{box.x, box.y + box.h - 6.0f, box.w, 6.0f};
    look::level(list, bar, share, gfx::mix(look::kInk.with_alpha(0.3f), accent_, a),
                low_ && a > 0.5f);
}

MoveTable::MoveTable()
{
    std::vector<ui::TableColumn> columns(3);
    columns[0].title = "#";
    columns[0].width = 56.0f;
    columns[0].numeric = true;
    columns[0].sortable = false;
    columns[1].title = trc("side", "White");
    columns[1].sortable = false;
    columns[2].title = trc("side", "Black");
    columns[2].sortable = false;
    for (std::size_t side = 1; side <= 2; ++side)
    {
        columns[side].cell =
            [this, side](ui::Canvas &canvas, const Rect &cell, const ui::TableRow &data, float)
        {
            const ui::Theme &theme = table_.style.theme;
            ui::Painter paint(canvas.list, canvas.fonts, theme, canvas.glass);
            const std::string &san = data.cells[side].text;
            const bool current = data.tag == static_cast<int>(side);
            const float size = 24.0f;
            if (current)
            {
                const float width = paint.label_width(san, size) + 28.0f;
                paint.fill({cell.x - 14.0f, cell.cy() - 19.0f, width, 38.0f},
                           std::min(theme.radius, 19.0f), theme.primary);
            }
            paint.label(san, cell.x, cell.cy() + size * 0.35f, size,
                        current ? theme.on_primary : theme.text);
        };
    }
    table_.set_columns(std::move(columns));
    table_.style.text_size = 24.0f;
    table_.style.header_size = 17.0f;
    table_.style.row_height = 52.0f;
    table_.style.header_height = 46.0f;
    table_.style.padding = 24.0f;
    table_.style.lines = ui::TableLines::zebra;
    table_.style.highlight.kind = ui::HighlightKind::none;
    table_.style.entrance_step = 0.0f;
    table_.set_active(false);
    app::look::tint(app::look::accent(app::look::Section::game), table_);
}

void MoveTable::set_bounds(const Rect &bounds)
{
    table_.set_bounds(bounds);
}

void MoveTable::set_accent(Color accent)
{
    app::look::tint(accent, table_);
}

void MoveTable::set_moves(const std::vector<std::string> &sans, int shown, int first_number,
                          bool black_first)
{
    if (sans == sans_ && shown == shown_ && first_number == first_number_ &&
        black_first == black_first_)
        return;
    sans_ = sans;
    shown_ = shown;
    first_number_ = first_number;
    black_first_ = black_first;

    // A game that starts with Black to move has an empty first White cell.
    const int lead = black_first ? 1 : 0;
    const int plies = static_cast<int>(sans.size()) + lead;
    std::vector<ui::TableRow> rows;
    int focus = -1;
    for (int at = 0; at < plies; at += 2)
    {
        ui::TableRow row;
        const int number = first_number + at / 2;
        row.id = number;
        const auto san_at = [&](int slot) -> std::string
        {
            const int ply = slot - lead;
            return ply >= 0 && ply < static_cast<int>(sans.size())
                       ? sans[static_cast<std::size_t>(ply)]
                       : std::string();
        };
        row.cells = {{std::to_string(number), static_cast<double>(number)},
                     {black_first && at == 0 ? "\xE2\x80\xA6" : san_at(at), 0.0},
                     {san_at(at + 1), 0.0}};
        // shown counts plies played: the move that led to the shown position
        // is ply shown - 1.
        const int marked = shown - 1 + lead;
        if (marked == at)
            row.tag = 1;
        else if (marked == at + 1)
            row.tag = 2;
        if (row.tag != 0)
            focus = static_cast<int>(rows.size());
        rows.push_back(std::move(row));
    }
    const int count = static_cast<int>(rows.size());
    table_.set_rows(std::move(rows));
    if (count > 0)
        table_.set_focus(focus >= 0 ? focus : 0, false);
}

void MoveTable::update(app::Context &ctx, float dt)
{
    ctx.calm(table_);
    table_.update(dt);
}

void MoveTable::draw(const app::Context &ctx, gfx::DrawList &list) const
{
    ui::Canvas canvas = app::canvas_for(ctx, list);
    table_.draw(canvas);
}

std::vector<chess::Piece> captured_by(const chess::Position &position, chess::Color taker)
{
    constexpr chess::Role kRoles[] = {chess::Role::queen, chess::Role::rook, chess::Role::bishop,
                                      chess::Role::knight, chess::Role::pawn};
    constexpr int kFull[] = {1, 2, 2, 2, 8};
    const chess::Color victim = chess::opposite(taker);
    std::vector<chess::Piece> out;
    for (std::size_t i = 0; i < 5; ++i)
    {
        const int missing = kFull[i] - position.count(victim, kRoles[i]);
        for (int n = 0; n < missing; ++n)
            out.push_back({victim, kRoles[i]});
    }
    return out;
}

std::string rating_text(int rating, bool provisional)
{
    if (rating <= 0)
        return {};
    return std::to_string(rating) + (provisional ? "?" : "");
}

} // namespace pch::modes
